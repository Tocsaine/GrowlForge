#include "WorkflowManager.h"
#include "../common/Math.h"
#include <algorithm>
#include <cmath>

namespace growlforge {

namespace {

bool isWorkflowParameter(clap_id id) {
    return isPresetParameter(id);
}

} // namespace

WorkflowManager::WorkflowManager(ParameterStore& parameters) : parameters_(parameters) {
    initializeFromCurrent();
}

WorkflowSnapshot WorkflowManager::captureCurrent() const {
    WorkflowSnapshot snapshot{};
    for (clap_id id = 0; id < kParamCount; ++id) {
        snapshot[id] = isWorkflowParameter(id) ? parameters_.values[id].load() : defs[id].def;
    }
    return snapshot;
}

bool WorkflowManager::equal(const WorkflowSnapshot& a, const WorkflowSnapshot& b) {
    for (clap_id id = 0; id < kParamCount; ++id) {
        if (!isWorkflowParameter(id)) continue;
        if (std::abs(a[id] - b[id]) > 1.0e-9) return false;
    }
    return true;
}

void WorkflowManager::initializeFromCurrent() {
    const WorkflowSnapshot current = captureCurrent();
    std::lock_guard lock(mutex_);
    slots_[0] = current;
    slots_[1] = current;
    activeSlot_ = 0;
    undo_.clear();
    redo_.clear();
    pendingAction_.reset();
    actionDepth_ = 0;
}

void WorkflowManager::beginAction() {
    std::lock_guard lock(mutex_);
    if (actionDepth_++ == 0) pendingAction_ = captureCurrent();
}

void WorkflowManager::pushUndoLocked(const WorkflowSnapshot& snapshot) {
    if (!undo_.empty() && equal(undo_.back(), snapshot)) return;
    undo_.push_back(snapshot);
    if (undo_.size() > kMaxHistory) undo_.erase(undo_.begin());
}

bool WorkflowManager::commitAction() {
    const WorkflowSnapshot current = captureCurrent();
    std::lock_guard lock(mutex_);
    if (actionDepth_ == 0) return false;
    if (--actionDepth_ != 0) return false;
    if (!pendingAction_) return false;
    const bool changed = !equal(*pendingAction_, current);
    if (changed) {
        pushUndoLocked(*pendingAction_);
        redo_.clear();
        slots_[activeSlot_] = current;
    }
    pendingAction_.reset();
    return changed;
}

void WorkflowManager::cancelAction() {
    std::lock_guard lock(mutex_);
    pendingAction_.reset();
    actionDepth_ = 0;
}

void WorkflowManager::applySnapshot(const WorkflowSnapshot& snapshot) {
    bool autoGainBecameEnabled = false;
    for (clap_id id = 0; id < kParamCount; ++id) {
        if (!isWorkflowParameter(id)) continue;
        double value = clamp(snapshot[id], defs[id].min, defs[id].max);
        value = isToggleParameter(id) ? (value >= 0.5 ? 1.0 : 0.0) : quantize01(value);
        const double previous = parameters_.values[id].exchange(value);
        if (previous == value) continue;
        parameters_.guiPendingValue[id] = value;
        parameters_.guiPendingFlags[id].fetch_or(2u, std::memory_order_release);
        if (id == AutoGain && value >= 0.5) autoGainBecameEnabled = true;
    }
    parameters_.values[ApplyAutoGain] = 0.0;
    if (autoGainBecameEnabled) parameters_.autoGainResetPending = true;
    parameters_.configDirty = true;
    parameters_.requestParamFlush();
}

bool WorkflowManager::undo() {
    WorkflowSnapshot target{};
    const WorkflowSnapshot current = captureCurrent();
    {
        std::lock_guard lock(mutex_);
        if (undo_.empty()) return false;
        target = undo_.back();
        undo_.pop_back();
        if (redo_.empty() || !equal(redo_.back(), current)) {
            redo_.push_back(current);
            if (redo_.size() > kMaxHistory) redo_.erase(redo_.begin());
        }
        slots_[activeSlot_] = target;
        pendingAction_.reset();
        actionDepth_ = 0;
    }
    applySnapshot(target);
    return true;
}

bool WorkflowManager::redo() {
    WorkflowSnapshot target{};
    const WorkflowSnapshot current = captureCurrent();
    {
        std::lock_guard lock(mutex_);
        if (redo_.empty()) return false;
        target = redo_.back();
        redo_.pop_back();
        pushUndoLocked(current);
        slots_[activeSlot_] = target;
        pendingAction_.reset();
        actionDepth_ = 0;
    }
    applySnapshot(target);
    return true;
}

bool WorkflowManager::canUndo() const {
    std::lock_guard lock(mutex_);
    return !undo_.empty();
}

bool WorkflowManager::canRedo() const {
    std::lock_guard lock(mutex_);
    return !redo_.empty();
}

bool WorkflowManager::switchToSlot(uint32_t slot) {
    if (slot > 1) return false;
    const WorkflowSnapshot current = captureCurrent();
    WorkflowSnapshot target{};
    {
        std::lock_guard lock(mutex_);
        slots_[activeSlot_] = current;
        if (slot == activeSlot_) return false;
        activeSlot_ = slot;
        target = slots_[activeSlot_];
        pendingAction_.reset();
        actionDepth_ = 0;
    }
    const bool changed = !equal(current, target);
    if (changed) applySnapshot(target);
    return changed;
}

bool WorkflowManager::copySlot(uint32_t source, uint32_t destination) {
    if (source > 1 || destination > 1 || source == destination) return false;
    const WorkflowSnapshot current = captureCurrent();
    WorkflowSnapshot target{};
    bool apply = false;
    bool changed = false;
    {
        std::lock_guard lock(mutex_);
        slots_[activeSlot_] = current;
        changed = !equal(slots_[source], slots_[destination]);
        slots_[destination] = slots_[source];
        apply = activeSlot_ == destination;
        target = slots_[destination];
    }
    if (apply && changed) applySnapshot(target);
    return changed;
}

uint32_t WorkflowManager::activeSlot() const {
    std::lock_guard lock(mutex_);
    return activeSlot_;
}

void WorkflowManager::syncActiveFromCurrent() {
    const WorkflowSnapshot current = captureCurrent();
    std::lock_guard lock(mutex_);
    slots_[activeSlot_] = current;
}

WorkflowState WorkflowManager::stateForSave() {
    const WorkflowSnapshot current = captureCurrent();
    std::lock_guard lock(mutex_);
    slots_[activeSlot_] = current;
    return WorkflowState{slots_[0], slots_[1], activeSlot_};
}

void WorkflowManager::restoreState(const WorkflowState& state) {
    std::lock_guard lock(mutex_);
    slots_[0] = state.slotA;
    slots_[1] = state.slotB;
    activeSlot_ = std::min<uint32_t>(state.activeSlot, 1u);
    undo_.clear();
    redo_.clear();
    pendingAction_.reset();
    actionDepth_ = 0;
}

} // namespace growlforge
