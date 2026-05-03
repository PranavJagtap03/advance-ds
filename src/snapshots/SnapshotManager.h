#pragma once
#include "FileNode.h"
#include <string>
#include <vector>
#include <map>

// ═══════════════════════════════════════════════════════════════════
// SnapshotManager — Persistent snapshot creation, storage, and diff.
// ═══════════════════════════════════════════════════════════════════

struct Snapshot {
    int id = 0;
    long long timestamp = 0;
    int fileCount = 0;
    long long totalBytes = 0;
    std::map<std::string, long long> fileMap; // path -> size
};

class SnapshotManager {
private:
    std::vector<Snapshot> snapshots_;
    int nextId_ = 1;

public:
    // Load snapshots from disk
    void load();

    // Save all snapshots to disk
    void save();

    // Create a new snapshot from current file list.
    // Prints SNAPSHOT_OK protocol line.
    void createSnapshot(const std::vector<FileNode>& files);

    // Diff two snapshots by ID.
    // Prints DIFF_RESULT or ERROR protocol line.
    void diff(int id1, int id2);
};
