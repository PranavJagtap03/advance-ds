#include "FileSystemEngine.h"
#include "FileScanner.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <dirent.h>
    #include <sys/stat.h>
    #include <sys/statvfs.h>
    #include <unistd.h>
#endif

using namespace std;

// Pipe-safe string encoding
static string safe(const string& s) {
    string r = s;
    for (size_t i = 0; i < r.length(); i++) if (r[i] == '|') r[i] = '\x01';
    return r;
}

// ═══════════════════════════════════════════════════════════════════
// CONSTRUCTION
// ═══════════════════════════════════════════════════════════════════

FileSystemEngine::FileSystemEngine()
    : segTree_(3660) // N-2: Extra slots for leap-year safety margin (10 years * 366 days)
{
    uf_.addNode(0); // Root node for orphan detection
}

// ═══════════════════════════════════════════════════════════════════
// FILE CRUD
// ═══════════════════════════════════════════════════════════════════

int FileSystemEngine::addFile(const string& path, const string& name,
                              int parentId, long size, long ts,
                              bool deepAnalysis,
                              const string& trueType, double entropy) {
    int id = nextFileId_++;
    FileNode f(name, path, parentId, size, ts, deepAnalysis);
    f.id = id;
    f.modifiedAt = ts;
    if (trueType != "UNKNOWN") f.trueType = trueType;
    if (entropy != 0.0) f.entropy = entropy;

    bpt_.insert(path, f);
    trie_.insert(name, id);
    segTree_.addFile(computeDayIndex((time_t)ts), f.size);
    uf_.addNode(id);
    uf_.unite(id, parentId);
    allFileIds_.push_back(id);
    return id;
}

bool FileSystemEngine::deleteFile(int fileId) {
    vector<FileNode> all = bpt_.getAllLeaves();
    for (size_t i = 0; i < all.size(); i++) {
        if (all[i].id == fileId) {
            pds_.saveVersion(all[i]);
            pds_.backupFile(all[i]);
            if (remove(all[i].path.c_str()) == 0) {
                bpt_.remove(all[i].path);
                trie_.remove(all[i].name);
                return true;
            }
            return false;
        }
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════
// QUERIES
// ═══════════════════════════════════════════════════════════════════

FileNode* FileSystemEngine::search(const string& key) {
    return bpt_.search(key);
}

vector<string> FileSystemEngine::prefixSearch(const string& prefix) {
    return trie_.prefixSearch(prefix);
}

vector<FileNode> FileSystemEngine::getAllFiles() const {
    // const_cast needed because BPlusTree::getAllLeaves() is not const
    return const_cast<BPlusTree&>(bpt_).getAllLeaves();
}

int FileSystemEngine::getFileCount() const {
    return const_cast<BPlusTree&>(bpt_).getSize();
}

// ═══════════════════════════════════════════════════════════════════
// DIRECTORY SCANNING — Legacy recursive (kept for compatibility)
// ═══════════════════════════════════════════════════════════════════

pair<int, long> FileSystemEngine::loadDirectory(const string& dirPath, int parentId) {
    int count = 0;
    long totalBytes = 0;

    if (parentId == 0 && scanRoot_.empty()) {
        scanRoot_ = dirPath;
        directoryIds_[dirPath] = 0;
    }

#ifdef _WIN32
    string pattern = dirPath + "\\*";
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        cout << "ERROR|LOAD_DIR|Could not open directory: " << safe(dirPath) << endl << flush;
        return {count, totalBytes};
    }
    do {
        string fname = ffd.cFileName;
        if (fname == "." || fname == "..") continue;
        if (ffd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | 0x400)) continue;
        string fullPath = dirPath + "\\" + fname;
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            int dirId = nextDirId_++;
            directoryIds_[fullPath] = dirId;
            uf_.addNode(dirId);
            uf_.unite(dirId, parentId);
            pair<int, long> sub = loadDirectory(fullPath, dirId);
            count += sub.first; totalBytes += sub.second;
        } else {
            ULARGE_INTEGER ull;
            ull.LowPart = ffd.ftLastWriteTime.dwLowDateTime;
            ull.HighPart = ffd.ftLastWriteTime.dwHighDateTime;
            time_t ts = (time_t)((ull.QuadPart - 116444736000000000ULL) / 10000000ULL);
            long fileSize = (long)(((long long)ffd.nFileSizeHigh << 32) | ffd.nFileSizeLow);
            int id = nextFileId_++;
            FileNode f(fname, fullPath, parentId, fileSize, ts, false);
            f.id = id; f.modifiedAt = ts;
            bpt_.insert(fullPath, f); trie_.insert(fname, id);
            segTree_.addFile(computeDayIndex(ts), f.size);
            uf_.addNode(id); uf_.unite(id, parentId);
            allFileIds_.push_back(id);
            count++; totalBytes += fileSize;
            if (count % 100 == 0) cout << "INDEXING|" << count << "|" << safe(fullPath) << endl << flush;
        }
    } while (FindNextFileA(hFind, &ffd) != 0);
    FindClose(hFind);
#else
    DIR* dir = opendir(dirPath.c_str());
    if (!dir) return {count, totalBytes};
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        string fname = entry->d_name;
        if (fname == "." || fname == "..") continue;
        string fullPath = dirPath + "/" + fname;
        struct stat st;
        if (lstat(fullPath.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            int dirId = nextDirId_++;
            directoryIds_[fullPath] = dirId;
            uf_.addNode(dirId);
            uf_.unite(dirId, parentId);
            pair<int, long> sub = loadDirectory(fullPath, dirId);
            count += sub.first; totalBytes += sub.second;
        } else if (S_ISREG(st.st_mode)) {
            long fileSize = (long)st.st_size;
            time_t ts = st.st_mtime;
            int id = nextFileId_++;
            FileNode f(fname, fullPath, parentId, fileSize, ts, false);
            f.id = id; f.modifiedAt = ts;
            bpt_.insert(fullPath, f); trie_.insert(fname, id);
            segTree_.addFile(computeDayIndex(ts), f.size);
            uf_.addNode(id); uf_.unite(id, parentId);
            allFileIds_.push_back(id);
            count++; totalBytes += fileSize;
            if (count % 100 == 0) cout << "INDEXING|" << count << "|" << safe(fullPath) << endl << flush;
        }
    }
    closedir(dir);
#endif
    return {count, totalBytes};
}

// ═══════════════════════════════════════════════════════════════════
// PARALLEL SCAN — Uses FileScanner with thread pool and buffered output
// ═══════════════════════════════════════════════════════════════════

pair<int, long> FileSystemEngine::scanDirectoryParallel(const string& dirPath, int threadCount) {
    return scanDirectoryIncremental(dirPath, 0, threadCount);
}

pair<int, long> FileSystemEngine::scanDirectoryIncremental(const string& dirPath, long lastScanTime, int threadCount) {
    if (scanRoot_.empty()) {
        scanRoot_ = dirPath;
        directoryIds_[dirPath] = 0;
    }

    int totalCount = 0;
    long totalBytes = 0;

    // Collect SCAN_FOLDER lines for immediate flushing
    vector<ImmediateLine> immediateLines;

    // Progress callback: emit SCAN_PROGRESS every 10,000 files
    auto progressCb = [](int count) {
        if (count % 10000 == 0) {
            cout << "SCAN_PROGRESS|" << count << endl << flush;
        }
    };

    // Run scan — immediateLines will be populated with SCAN_FOLDER entries
    vector<ScannedFile> files = FileScanner::scanDirectoryRecursive(
        dirPath, threadCount, progressCb, lastScanTime, &immediateLines
    );

    // Process results with dual-buffer approach:
    // - immediateLines: SCAN_FOLDER events, flushed immediately per directory
    // - fileBatch: FILE| lines, flushed every 50,000
    vector<string> fileBatch;
    fileBatch.reserve(min((int)files.size(), 50000));

    // Build a map from dirPath -> index range into immediateLines for interleaving
    // Since files are returned in directory order (recursive DFS), we can interleave
    // by tracking which immediate lines have been flushed
    size_t nextImmediate = 0;

    // First flush all SCAN_FOLDER lines that were collected before any FILE processing
    // (These are the ENTERING lines from the scan)
    // We interleave: for each file, check if there are SCAN_FOLDER lines to flush
    string lastParentDir;

    for (size_t i = 0; i < files.size(); i++) {
        const ScannedFile& sf = files[i];
        if (sf.isDirectory) continue;

        // Determine parent directory
        string parentDir;
        size_t lastSep = sf.path.find_last_of("/\\");
        if (lastSep != string::npos) {
            parentDir = sf.path.substr(0, lastSep);
        } else {
            parentDir = dirPath;
        }

        // Flush any pending SCAN_FOLDER lines when we enter a new parent dir
        if (parentDir != lastParentDir) {
            // Flush pending SCAN_FOLDER lines up to this point
            while (nextImmediate < immediateLines.size()) {
                const string& il = immediateLines[nextImmediate].content;
                // Check if this immediate line is about the current or earlier directory
                cout << il << endl << flush;
                nextImmediate++;
                // If this was the ENTERING for our current parentDir, stop
                if (il.find(parentDir) != string::npos && il.find("ENTERING") != string::npos) {
                    break;
                }
            }
            lastParentDir = parentDir;
        }

        // Add to engine data structures
        int parentId = getOrCreateDirId(parentDir);
        int id = addFile(sf.path, sf.name, parentId, sf.size, sf.mtime,
                        false, sf.trueType, sf.entropy);

        // Buffer the FILE| line
        ostringstream oss;
        oss << "FILE|" << safe(sf.path) << "|" << safe(sf.name) << "|"
            << sf.size << "|" << sf.mtime << "|" << sf.trueType << "|"
            << fixed << setprecision(4) << sf.entropy << "|" << sf.checksum;
        fileBatch.push_back(oss.str());

        totalCount++;
        totalBytes += sf.size;

        // Flush file batch every 50,000 lines
        if (fileBatch.size() >= 50000) {
            for (const auto& line : fileBatch) {
                cout << line << "\n";
            }
            cout << flush;
            fileBatch.clear();
        }
    }

    // Flush remaining immediate lines (completion SCAN_FOLDER lines)
    while (nextImmediate < immediateLines.size()) {
        cout << immediateLines[nextImmediate].content << endl << flush;
        nextImmediate++;
    }

    // Flush remaining file batch
    if (!fileBatch.empty()) {
        for (const auto& line : fileBatch) {
            cout << line << "\n";
        }
        cout << flush;
        fileBatch.clear();
    }

    // Finalize
    initDenseRange();
    cout << "SCAN_DONE|" << totalCount << endl << flush;

    return {totalCount, totalBytes};
}

// ═══════════════════════════════════════════════════════════════════
// HELPERS
// ═══════════════════════════════════════════════════════════════════

int FileSystemEngine::getOrCreateDirId(const string& dirPath) {
    auto it = directoryIds_.find(dirPath);
    if (it != directoryIds_.end()) return it->second;
    int id = nextDirId_++;
    directoryIds_[dirPath] = id;
    uf_.addNode(id);
    uf_.unite(id, 0);
    return id;
}

void FileSystemEngine::initDenseRange() {
    if (nextFileId_ > 10000) {
        uf_.initDense(10000, nextFileId_ - 1);
    }
}

void FileSystemEngine::performDeepAnalysis(vector<FileNode>& files) {
    for (size_t i = 0; i < files.size(); i++) {
        if ((files[i].trueType == "UNKNOWN" || files[i].entropy == 0.0) && files[i].size > 0) {
            FileNode deep(files[i].name, files[i].path, files[i].parentId,
                          files[i].size, files[i].createdAt, true);
            deep.id = files[i].id;
            deep.modifiedAt = files[i].modifiedAt;
            files[i] = deep;
            bpt_.remove(deep.path);
            bpt_.insert(deep.path, deep);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
// DISK INFO
// ═══════════════════════════════════════════════════════════════════

void FileSystemEngine::sendDiskInfo() const {
    long long total = 0, free = 0;
#ifdef _WIN32
    ULARGE_INTEGER uFree, uTotal, uTotalFree;
    if (GetDiskFreeSpaceExA(scanRoot_.empty() ? NULL : scanRoot_.c_str(),
                            &uFree, &uTotal, &uTotalFree)) {
        total = (long long)uTotal.QuadPart;
        free = (long long)uFree.QuadPart;
    }
#else
    struct statvfs vfs;
    if (statvfs(scanRoot_.empty() ? "/" : scanRoot_.c_str(), &vfs) == 0) {
        total = (long long)vfs.f_blocks * vfs.f_frsize;
        free = (long long)vfs.f_bfree * vfs.f_frsize;
    }
#endif

    long long indexedSize = 0;
    vector<FileNode> all = getAllFiles();
    for (size_t i = 0; i < all.size(); i++) indexedSize += all[i].size;

    cout << "DISK_INFO|" << total << "|" << free << "|" << indexedSize << endl << flush;
}

// ═══════════════════════════════════════════════════════════════════
// ACCESSORS
// ═══════════════════════════════════════════════════════════════════

SegmentTree& FileSystemEngine::segTree() { return segTree_; }
const SegmentTree& FileSystemEngine::segTree() const { return segTree_; }
UnionFind& FileSystemEngine::uf() { return uf_; }
PersistentDS& FileSystemEngine::pds() { return pds_; }
BPlusTree& FileSystemEngine::bpt() { return bpt_; }
Trie& FileSystemEngine::trie() { return trie_; }
const vector<int>& FileSystemEngine::allFileIds() const { return allFileIds_; }
const string& FileSystemEngine::scanRoot() const { return scanRoot_; }
