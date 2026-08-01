#include "../src/state/WorkflowManager.h"
#include <cmath>
#include <iostream>

int main() {
    using namespace growlforge;
    ParameterStore store(nullptr);
    WorkflowManager workflow(store);

    workflow.beginAction();
    store.values[Drive] = 5.0;
    if (!workflow.commitAction() || !workflow.canUndo()) {
        std::cerr << "history not created\n";
        return 1;
    }
    if (!workflow.undo() || std::abs(store.values[Drive].load()) > 1.0e-9 || !workflow.canRedo()) {
        std::cerr << "undo failed\n";
        return 2;
    }
    if (!workflow.redo() || std::abs(store.values[Drive].load() - 5.0) > 1.0e-9) {
        std::cerr << "redo failed\n";
        return 3;
    }

    workflow.copySlot(0, 1);
    workflow.switchToSlot(1);
    workflow.beginAction();
    store.values[Fuzz] = 7.0;
    workflow.commitAction();
    workflow.switchToSlot(0);
    if (std::abs(store.values[Fuzz].load()) > 1.0e-9 || std::abs(store.values[Drive].load() - 5.0) > 1.0e-9) {
        std::cerr << "slot A wrong\n";
        return 4;
    }
    workflow.switchToSlot(1);
    if (std::abs(store.values[Fuzz].load() - 7.0) > 1.0e-9) {
        std::cerr << "slot B wrong\n";
        return 5;
    }

    const WorkflowState saved = workflow.stateForSave();
    WorkflowManager restored(store);
    restored.restoreState(saved);
    if (restored.activeSlot() != 1) {
        std::cerr << "active slot not restored\n";
        return 6;
    }

    std::cout << "workflow validation: ok\n";
    return 0;
}
