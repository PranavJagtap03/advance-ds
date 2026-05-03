// ═══════════════════════════════════════════════════════════════════
// FSM — File System Directory Manager
// Modular Architecture Entry Point
//
// main() owns the engine and snapshot manager.
// No globals. No logic. Just wiring.
// ═══════════════════════════════════════════════════════════════════

#include "FileSystemEngine.h"
#include "CommandProcessor.h"
#include "SnapshotManager.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    bool machine = false;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--machine") machine = true;
    }

    FileSystemEngine engine;
    SnapshotManager snapshots;
    snapshots.load();

    if (machine) {
        CommandProcessor processor(engine, snapshots);
        processor.run();
    } else {
        std::cout << "Menu mode disabled. Use --machine." << std::endl;
    }

    return 0;
}
