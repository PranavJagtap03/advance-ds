#pragma once
#include "FileSystemEngine.h"
#include "SnapshotManager.h"
#include "AnalyticsEngine.h"
#include <sstream>
#include <string>

// ═══════════════════════════════════════════════════════════════════
// CommandProcessor — Machine-mode command parser and dispatcher.
// Parses stdin, calls engine/analytics/snapshot APIs.
// Output format is a HARD CONTRACT — byte-identical to original.
// ═══════════════════════════════════════════════════════════════════

class CommandProcessor {
private:
    FileSystemEngine& engine_;
    SnapshotManager& snapshots_;
    AnalyticsEngine analytics_;

    // ── Command handlers ─────────────────────────────────────────
    void handleLoadDir(std::istringstream& args);
    void handleLoadBatch();
    void handleSearch(std::istringstream& args);
    void handlePrefix(std::istringstream& args);
    void handleRegex(std::istringstream& args);
    void handleDelete(std::istringstream& args);
    void handleRollback(std::istringstream& args);
    void handleVersionHistory(std::istringstream& args);
    void handleSnapshot();
    void handleDiff(std::istringstream& args);
    void handleDumpCache();
    void handleScanParallel(std::istringstream& args);
    void handleScanIncremental(std::istringstream& args);

public:
    CommandProcessor(FileSystemEngine& engine, SnapshotManager& snapshots);

    // Main command loop — reads stdin until EXIT
    void run();
};
