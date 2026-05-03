#pragma once
#include "FileNode.h"
#include "BPlusTree.h"
#include "Trie.h"
#include "SegmentTree.h"
#include "UnionFind.h"
#include "PersistentDS.h"
#include "Utils.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>

// ═══════════════════════════════════════════════════════════════════
// FileSystemEngine — Central coordinator owning all data structures.
// All mutations go through this class. No global state.
// ═══════════════════════════════════════════════════════════════════

class FileSystemEngine {
private:
    BPlusTree bpt_;
    Trie trie_;
    SegmentTree segTree_;
    PersistentDS pds_;
    UnionFind uf_;

    std::vector<int> allFileIds_;
    int nextFileId_ = 10000;
    int nextDirId_ = 10;
    std::unordered_map<std::string, int> directoryIds_;
    std::string scanRoot_;

public:
    FileSystemEngine();

    // ── File CRUD ──────────────────────────────────────────────────
    // Adds a file to ALL data structures. Returns assigned file ID.
    int addFile(const std::string& path, const std::string& name,
                int parentId, long size, long ts,
                bool deepAnalysis = false,
                const std::string& trueType = "UNKNOWN",
                double entropy = 0.0);

    // Deletes a file by ID. Returns true if found and removed.
    bool deleteFile(int fileId);

    // ── Queries ────────────────────────────────────────────────────
    FileNode* search(const std::string& key);
    std::vector<std::string> prefixSearch(const std::string& prefix);
    std::vector<FileNode> getAllFiles() const;
    int getFileCount() const;

    // ── Directory scanning ─────────────────────────────────────────
    // Recursively scans and indexes a directory. Returns {count, totalBytes}.
    std::pair<int, long> loadDirectory(const std::string& dirPath, int parentId = 0);

    // Parallel scan using FileScanner with thread pool and buffered output.
    // Outputs FILE| lines in batches of 50,000 and SCAN_PROGRESS every 50,000 files.
    std::pair<int, long> scanDirectoryParallel(const std::string& dirPath, int threadCount = 8);

    // Incremental scan: only return files modified since lastScanTime.
    std::pair<int, long> scanDirectoryIncremental(const std::string& dirPath, long lastScanTime, int threadCount = 8);

    // Gets or creates a directory ID for batch loading.
    int getOrCreateDirId(const std::string& dirPath);

    // Initializes dense UnionFind vectors after scanning.
    void initDenseRange();

    // Performs deep file analysis (entropy + true type) on files.
    void performDeepAnalysis(std::vector<FileNode>& files);

    // ── Disk info ──────────────────────────────────────────────────
    void sendDiskInfo() const;

    // ── Accessors for sub-systems ──────────────────────────────────
    // These provide controlled access for CommandProcessor/AnalyticsEngine
    // that need specific DS operations not exposed via the generic API.
    SegmentTree& segTree();
    const SegmentTree& segTree() const;
    UnionFind& uf();
    PersistentDS& pds();
    BPlusTree& bpt();
    Trie& trie();
    const std::vector<int>& allFileIds() const;
    const std::string& scanRoot() const;
};
