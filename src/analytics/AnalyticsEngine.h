#pragma once
#include "FileSystemEngine.h"

// ═══════════════════════════════════════════════════════════════════
// AnalyticsEngine — All analytics/detection commands.
// Receives FileSystemEngine by reference. Never owns data.
// ═══════════════════════════════════════════════════════════════════

class AnalyticsEngine {
private:
    FileSystemEngine& engine_;

public:
    explicit AnalyticsEngine(FileSystemEngine& engine);

    // DUPLICATES command — detects duplicate files by checksum
    void detectDuplicates();

    // SUSPICIOUS command — detects high-entropy files
    void detectSuspicious();

    // TYPE_DISTRIBUTION command — file type breakdown
    void typeDistribution();

    // WARNINGS command — storage health warnings
    void computeWarnings();

    // RECLAIM command — reclaimable space analysis
    void computeReclaim();

    // ORPHANS command — detect disconnected files
    void findOrphans();

    // DELETE_ORPHANS command — bulk delete orphaned files
    void deleteOrphans();

    // ANALYTICS command — monthly storage growth
    void monthlyAnalytics();
};
