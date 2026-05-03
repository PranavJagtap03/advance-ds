#pragma once
#include <string>
#include <vector>
#include <functional>

// ═══════════════════════════════════════════════════════════════════
// FileScanner — Production-grade recursive directory walker.
// Single-threaded recursive traversal with depth limit, incremental
// scan via mtime, progress callbacks, SCAN_FOLDER emission, and
// batch buffering.
// Returns raw data only. Does NOT include FileNode or modify any DS.
// ═══════════════════════════════════════════════════════════════════

struct ScannedFile {
    std::string name;
    std::string path;
    long size;
    long mtime;
    bool isDirectory;
    std::string trueType;
    double entropy;
    std::string checksum;
};

// Immediate output line (SCAN_FOLDER events, not batched with FILE| lines)
struct ImmediateLine {
    std::string content;
};

class FileScanner {
public:
    // Maximum recursion depth to prevent infinite loops
    static const int MAX_DEPTH = 32;

    // Batch size for stdout buffering
    static const int STDOUT_BATCH_SIZE = 50000;

    // Progress callback interval (every N files)
    static const int PROGRESS_INTERVAL = 10000;

    // Scans a single directory level (legacy interface, kept for compatibility).
    static std::vector<ScannedFile> scanDirectory(const std::string& dirPath);

    // Recursive scan with all production features.
    // - dirPath: root directory to scan
    // - threadCount: ignored (kept for interface compat), always single-threaded
    // - progressCallback: called every PROGRESS_INTERVAL files with current count
    // - lastScanTime: if > 0, only return files where mtime > lastScanTime (incremental)
    // - immediateLines: output vector for SCAN_FOLDER lines (flushed immediately)
    // Returns all scanned files.
    static std::vector<ScannedFile> scanDirectoryRecursive(
        const std::string& dirPath,
        int threadCount = 1,
        std::function<void(int)> progressCallback = nullptr,
        long lastScanTime = 0,
        std::vector<ImmediateLine>* immediateLines = nullptr
    );

private:
    // Recursive helper
    static void scanRecursiveHelper(
        const std::string& dirPath,
        int depth,
        std::vector<ScannedFile>& results,
        int& fileCount,
        std::function<void(int)> progressCallback,
        long lastScanTime,
        std::vector<ImmediateLine>* immediateLines
    );
};
