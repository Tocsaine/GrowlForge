#pragma once

#include "../parameters/ParameterStore.h"
#include <array>
#include <cstddef>
#include <mutex>
#include <optional>
#include <vector>

namespace growlforge {

using WorkflowSnapshot = std::array<double, kParamCount>;

struct WorkflowState {
    WorkflowSnapshot slotA{};
    WorkflowSnapshot slotB{};
    uint32_t activeSlot = 0;
};

class WorkflowManager {
public:
    explicit WorkflowManager(ParameterStore& parameters);

    void initializeFromCurrent();

    void beginAction();
    bool commitAction();
    void cancelAction();

    bool undo();
    bool redo();
    bool canUndo() const;
    bool canRedo() const;

    bool switchToSlot(uint32_t slot);
    bool copySlot(uint32_t source, uint32_t destination);
    uint32_t activeSlot() const;

    WorkflowState stateForSave();
    void restoreState(const WorkflowState& state);

    WorkflowSnapshot captureCurrent() const;
    void syncActiveFromCurrent();

private:
    static bool equal(const WorkflowSnapshot& a, const WorkflowSnapshot& b);
    void applySnapshot(const WorkflowSnapshot& snapshot);
    void pushUndoLocked(const WorkflowSnapshot& snapshot);

    ParameterStore& parameters_;
    mutable std::mutex mutex_;
    std::array<WorkflowSnapshot, 2> slots_{};
    uint32_t activeSlot_ = 0;
    std::vector<WorkflowSnapshot> undo_;
    std::vector<WorkflowSnapshot> redo_;
    std::optional<WorkflowSnapshot> pendingAction_;
    uint32_t actionDepth_ = 0;
    static constexpr size_t kMaxHistory = 64;
};

} // namespace growlforge
