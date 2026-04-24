#include "BPlusTree.h"
#include "Trie.h"
#include "SegmentTree.h"
#include "PersistentDS.h"
#include "UnionFind.h"
#include "Features.h"
#include <iostream>
#include <string>
#include <iomanip>
#include <vector>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <unordered_map>
#include <map>
#include <chrono>
#include <fstream>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <dirent.h>
    #include <sys/stat.h>
    #include <sys/statvfs.h>
    #include <unistd.h>
#endif

using namespace std;

// ═══════════════════════════════════
// GLOBALS
// ═══════════════════════════════════
BPlusTree bpt;
Trie trie;
SegmentTree segTree(365);
PersistentDS pds;
UnionFind uf;
vector<int> allFileIds;
int nextFileId = 10000;
unordered_map<string, int> directoryIds;
int nextDirId = 10; 
string scanRoot = "";

struct Snapshot {
    int id;
    long long timestamp;
    int fileCount;
    long long totalBytes;
    map<string, long long> fileMap; // path -> size
};
vector<Snapshot> snapshots;
int nextSnapshotId = 1;

// ═══════════════════════════════════
// UTILS
// ═══════════════════════════════════

static string safe(const string& s) {
    string r = s;
    for (char& c : r) if (c == '|') c = '_';
    return r;
}

void loadSnapshots() {
    ifstream ifs("snapshots.dat", ios::binary);
    if (!ifs) return;
    while (ifs) {
        Snapshot s;
        if (!(ifs.read((char*)&s.id, sizeof(s.id)))) break;
        ifs.read((char*)&s.timestamp, sizeof(s.timestamp));
        ifs.read((char*)&s.fileCount, sizeof(s.fileCount));
        ifs.read((char*)&s.totalBytes, sizeof(s.totalBytes));
        for (int i = 0; i < s.fileCount; i++) {
            int len; ifs.read((char*)&len, sizeof(len));
            string path(len, ' '); ifs.read(&path[0], len);
            long long sz; ifs.read((char*)&sz, sizeof(sz));
            s.fileMap[path] = sz;
        }
        snapshots.push_back(s);
        if (s.id >= nextSnapshotId) nextSnapshotId = s.id + 1;
    }
}

void saveSnapshots() {
    ofstream ofs("snapshots.dat", ios::binary | ios::trunc);
    for (const auto& s : snapshots) {
        ofs.write((char*)&s.id, sizeof(s.id));
        ofs.write((char*)&s.timestamp, sizeof(s.timestamp));
        ofs.write((char*)&s.fileCount, sizeof(s.fileCount));
        ofs.write((char*)&s.totalBytes, sizeof(s.totalBytes));
        for (auto const& kv : s.fileMap) {
            string path = kv.first;
            long long sz = kv.second;
            int len = path.length();
            ofs.write((char*)&len, sizeof(len));
            ofs.write(path.c_str(), len);
            ofs.write((char*)&sz, sizeof(sz));
        }
    }
}

void sendDiskInfo(const string& path) {
    long long total = 0, free = 0;
#ifdef _WIN32
    ULARGE_INTEGER uFree, uTotal, uTotalFree;
    if (GetDiskFreeSpaceExA(path.empty() ? NULL : path.c_str(), &uFree, &uTotal, &uTotalFree)) {
        total = (long long)uTotal.QuadPart;
        free = (long long)uFree.QuadPart;
    }
#else
    struct statvfs vfs;
    if (statvfs(path.empty() ? "/" : path.c_str(), &vfs) == 0) {
        total = (long long)vfs.f_blocks * vfs.f_frsize;
        free = (long long)vfs.f_bfree * vfs.f_frsize;
    }
#endif
    
    long long indexedSize = 0;
    vector<FileNode> all = bpt.getAllLeaves();
    for(const auto& f : all) indexedSize += f.size;

    cout << "DISK_INFO|" << total << "|" << free << "|" << indexedSize << endl << flush;
}

// ═══════════════════════════════════
// CORE SCAN LOGIC
// ═══════════════════════════════════

pair<int,long> loadDirectory(const string& dirPath, int parentId = 0) {
    int count = 0;
    long totalBytes = 0;

    if (parentId == 0 && scanRoot == "") {
        scanRoot = dirPath;
        directoryIds[dirPath] = 0;
    }

#ifdef _WIN32
    string pattern = dirPath + "\\*";
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return {count, totalBytes};
    do {
        string fname = ffd.cFileName;
        if (fname == "." || fname == "..") continue;
        if (ffd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | 0x400)) continue;
        string fullPath = dirPath + "\\" + fname;
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            int dirId = nextDirId++;
            directoryIds[fullPath] = dirId;
            uf.addNode(dirId);
            uf.unite(dirId, parentId);
            auto sub = loadDirectory(fullPath, dirId);
            count += sub.first; totalBytes += sub.second;
        } else {
            ULARGE_INTEGER ull;
            ull.LowPart = ffd.ftLastWriteTime.dwLowDateTime;
            ull.HighPart = ffd.ftLastWriteTime.dwHighDateTime;
            long ts = (long)((ull.QuadPart - 116444736000000000ULL) / 10000000ULL);
            long fileSize = (long)(((long long)ffd.nFileSizeHigh << 32) | ffd.nFileSizeLow);
            int id = nextFileId++;
            FileNode f(fname, fullPath, parentId, fileSize, ts);
            f.id = id; f.modifiedAt = ts;
            bpt.insert(fname, f); trie.insert(fname, id);
            struct tm* ti = localtime(&ts);
            if (ti) segTree.addFile(ti->tm_yday % 365, f.size);
            uf.addNode(id); uf.unite(id, parentId);
            pds.saveVersion(f); allFileIds.push_back(id);
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
            int dirId = nextDirId++;
            directoryIds[fullPath] = dirId;
            uf.addNode(dirId);
            uf.unite(dirId, parentId);
            auto sub = loadDirectory(fullPath, dirId);
            count += sub.first; totalBytes += sub.second;
        } else if (S_ISREG(st.st_mode)) {
            long fileSize = (long)st.st_size;
            long ts = (long)st.st_mtime;
            int id = nextFileId++;
            FileNode f(fname, fullPath, parentId, fileSize, ts);
            f.id = id; f.modifiedAt = ts;
            bpt.insert(fname, f); trie.insert(fname, id);
            struct tm* ti = localtime(&ts);
            if (ti) segTree.addFile(ti->tm_yday % 365, f.size);
            uf.addNode(id); uf.unite(id, parentId);
            pds.saveVersion(f); allFileIds.push_back(id);
            count++; totalBytes += fileSize;
            if (count % 100 == 0) cout << "INDEXING|" << count << "|" << safe(fullPath) << endl << flush;
        }
    }
    closedir(dir);
#endif
    return {count, totalBytes};
}

// ═══════════════════════════════════
// MACHINE MODE
// ═══════════════════════════════════

void runMachineMode() {
    string line;
    while (getline(cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        istringstream iss(line);
        string cmd; iss >> cmd;

        if (cmd == "EXIT") { cout << "BYE" << endl << flush; return; }
        else if (cmd == "LOAD_BATCH_BEGIN") {
            int count = 0;
            string batchLine;
            while (getline(cin, batchLine)) {
                if (!batchLine.empty() && batchLine.back() == '\r') batchLine.pop_back();
                if (batchLine == "LOAD_BATCH_END") break;
                istringstream biss(batchLine);
                string fname, pdir, sz_str, mt_str;
                if(getline(biss, fname, '|') && getline(biss, pdir, '|') && getline(biss, sz_str, '|') && getline(biss, mt_str)) {
                    long sz = stol(sz_str);
                    long mt = stol(mt_str);
                    int parentId = 0;
                    if(directoryIds.find(pdir) != directoryIds.end()) {
                        parentId = directoryIds[pdir];
                    } else {
                        parentId = nextDirId++;
                        directoryIds[pdir] = parentId;
                        uf.addNode(parentId);
                        uf.unite(parentId, 0);
                    }
                    int id = nextFileId++;
                    FileNode f(fname, pdir + "/" + fname, parentId, sz, mt);
                    f.id = id; f.modifiedAt = mt;
                    bpt.insert(fname, f); trie.insert(fname, id);
                    struct tm* ti = localtime(&mt);
                    if (ti) segTree.addFile(ti->tm_yday % 365, f.size);
                    uf.addNode(id); uf.unite(id, parentId);
                    pds.saveVersion(f); allFileIds.push_back(id);
                    count++;
                }
            }
            cout << "BATCH_DONE|" << count << endl << flush;
        }
        else if (cmd == "DISK_INFO") { sendDiskInfo(scanRoot); }
        else if (cmd == "SNAPSHOT") {
            vector<FileNode> all = bpt.getAllLeaves();
            Snapshot s;
            s.id = nextSnapshotId++;
            s.timestamp = time(0);
            s.fileCount = all.size();
            s.totalBytes = 0;
            for (const auto& f : all) {
                s.totalBytes += f.size;
                s.fileMap[f.path] = f.size;
            }
            snapshots.push_back(s);
            saveSnapshots();
            cout << "SNAPSHOT_OK|" << s.id << "|" << s.fileCount << "|" << s.totalBytes << endl << flush;
        }
        else if (cmd == "DIFF") {
            int id1, id2; if (!(iss >> id1 >> id2)) { cout << "ERROR|DIFF|args" << endl << flush; continue; }
            Snapshot *s1 = nullptr, *s2 = nullptr;
            for (auto& s : snapshots) {
                if (s.id == id1) s1 = &s;
                if (s.id == id2) s2 = &s;
            }
            if (!s1 || !s2) { cout << "ERROR|DIFF|not_found" << endl << flush; continue; }
            
            int added = 0, deleted = 0, modified = 0, unchanged = 0;
            for (auto const& kv : s2->fileMap) {
                if (s1->fileMap.find(kv.first) == s1->fileMap.end()) added++;
                else if (s1->fileMap[kv.first] != kv.second) modified++;
                else unchanged++;
            }
            for (auto const& kv : s1->fileMap) {
                if (s2->fileMap.find(kv.first) == s2->fileMap.end()) deleted++;
            }
            cout << "DIFF_RESULT|" << added << "|" << deleted << "|" << modified << "|" << unchanged << endl << flush;
        }
        else if (cmd == "WARNINGS") {
            vector<FileNode> all = bpt.getAllLeaves();
            map<string, vector<FileNode>> groups;
            for (const auto& f : all) groups[f.checksum].push_back(f);
            long long dupeWaste = 0;
            for (const auto& g : groups) if (g.second.size() > 1) dupeWaste += (long long)(g.second.size() - 1) * g.second[0].size;
            if (dupeWaste > 500 * 1048576LL) cout << "WARN|DUPLICATES|Duplicate files wasting " << (dupeWaste / 1048576) << " MB" << endl;
            long long oldWaste = 0;
            long long twoYearsAgo = time(0) - (2LL * 365 * 86400);
            for (const auto& f : all) if (f.modifiedAt < twoYearsAgo) oldWaste += f.size;
            if (oldWaste > 1024 * 1048576LL) cout << "WARN|OLD_FILES|Files >2yrs occupy " << (oldWaste / 1048576) << " MB" << endl;
            cout << "WARNINGS_DONE" << endl << flush;
        }
        else if (cmd == "RECLAIM") {
            vector<FileNode> all = bpt.getAllLeaves();
            long long oneYearAgo = time(0) - (365LL * 86400);
            long long totalRec = 0;
            map<string, vector<FileNode>> groups;
            for (const auto& f : all) groups[f.checksum].push_back(f);
            long long dupeRec = 0;
            for (const auto& g : groups) if (g.second.size() > 1) dupeRec += (long long)(g.second.size() - 1) * g.second[0].size;
            cout << "REC|DUPLICATES|" << dupeRec << endl; totalRec += dupeRec;
            long long oldRec = 0;
            for (const auto& f : all) if (f.modifiedAt < oneYearAgo) oldRec += f.size;
            cout << "REC|OLD_FILES|" << oldRec << endl; totalRec += oldRec;
            for (const auto& f : all) if (f.size > 100 * 1048576LL) cout << "REC|LARGE_FILE|" << safe(f.name) << "|" << f.size << "|" << safe(f.path) << endl;
            cout << "RECLAIM_DONE|" << totalRec << endl << flush;
        }
        else if (cmd == "DELETE") {
            int fid = -1; iss >> fid;
            vector<FileNode> all = bpt.getAllLeaves();
            bool found = false;
            for (const auto& f : all) if (f.id == fid) {
                if (remove(f.path.c_str()) == 0) { bpt.remove(f.name); trie.remove(f.name); cout << "DELETE_OK|" << fid << endl; }
                else cout << "ERROR|DELETE|failed" << endl;
                found = true; break;
            }
            if(!found) cout << "ERROR|DELETE|not_found" << endl;
            cout << flush;
        }
        else if (cmd == "DUMP_CACHE") {
            vector<FileNode> all = bpt.getAllLeaves();
            for (const auto& f : all) {
                cout << "CACHE_ITEM|" << safe(f.name) << "|" << safe(f.path) << "|" << f.size << "|" << f.modifiedAt << endl;
            }
            cout << "CACHE_DONE" << endl << flush;
        }
        else if (cmd == "LOAD_DIR") {
            string path; getline(iss >> ws, path);
            if (path.empty()) { cout << "ERROR|LOAD_DIR|no path" << endl << flush; continue; }
            auto res = loadDirectory(path);
            cout << "LOADED|" << res.first << "|" << res.second << endl << flush;
        }
        else if (cmd == "ROLLBACK") {
            int fid, ver; if (!(iss >> fid >> ver)) { cout << "ROLLBACK_ERROR|args" << endl << flush; continue; }
            if (pds.rollback(fid, ver, bpt)) cout << "ROLLBACK_OK|" << fid << "|" << ver << endl << flush;
            else cout << "ROLLBACK_ERROR|failed" << endl << flush;
        }
        else if (cmd == "SEARCH") {
            string filename; getline(iss >> ws, filename);
            FileNode* node = bpt.search(filename);
            if (node) cout << "RESULT|FOUND|" << safe(node->name) << "|" << safe(node->path) << "|" << node->size / 1024 << "|Engine|" << safe(node->path) << "|0|Direct" << endl << flush;
            else cout << "RESULT|NOT_FOUND|" << safe(filename) << "|||Engine||0|Direct" << endl << flush;
        }
        else if (cmd == "PREFIX") {
            string prefix; getline(iss >> ws, prefix);
            vector<string> results = trie.prefixSearch(prefix);
            if (!results.empty()) {
                string resultStr = "";
                for (const auto& name : results) resultStr += safe(name) + ",";
                cout << "RESULT|PREFIX|" << resultStr << "|||Engine||0|Direct" << endl << flush;
            } else {
                cout << "RESULT|NOT_FOUND|" << safe(prefix) << "|||Engine||0|Direct" << endl << flush;
            }
        }
        else if (cmd == "DUPLICATES") {
            vector<FileNode> all = bpt.getAllLeaves();
            map<string, vector<FileNode>> groups;
            for (const auto& f : all) groups[f.checksum].push_back(f);
            int count = 0; long long total = 0;
            for (const auto& g : groups) {
                if (g.second.size() > 1) {
                    count++; total += (long long)(g.second.size()-1) * g.second[0].size;
                    string paths = "";
                    for(const auto& f : g.second) paths += safe(f.path) + ",";
                    cout << "DUP|" << safe(g.second[0].name) << "|" << safe(g.first) << "|" << paths << "|" << g.second[0].size/1024 << endl;
                }
            }
            cout << "DUP_DONE|" << count << "|" << total/1048576.0 << endl << flush;
        }
        else if (cmd == "ORPHANS") {
            vector<int> orphans = uf.findAllOrphans(0, allFileIds);
            vector<FileNode> all = bpt.getAllLeaves();
            long long total = 0;
            for (int id : orphans) for (const auto& f : all) if (f.id == id) {
                cout << "ORPHAN|" << f.id << "|" << safe(f.name) << "|" << safe(f.path) << "|" << f.size/1024 << endl;
                total += f.size; break;
            }
            cout << "ORPHAN_DONE|" << orphans.size() << "|" << total/1048576.0 << endl << flush;
        }
        else if (cmd == "ANALYTICS") {
            for (int i = 0; i < 12; i++) {
                const string months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
                const int starts[] = {0,31,59,90,120,151,181,212,243,273,304,334};
                const int ends[] = {30,58,89,119,150,180,211,242,272,303,333,364};
                cout << "MONTH|" << months[i] << "|" << fixed << setprecision(2) << segTree.queryRange(starts[i], ends[i])/1048576.0 << endl;
            }
            cout << "ANALYTICS_DONE|0" << endl << flush;
        }
        else if (cmd == "VERSION_HISTORY") {
            int fid; if (!(iss >> fid)) { cout << "VERSION_DONE|0" << endl << flush; continue; }
            for (int v = 1; ; v++) {
                FileNode fn = pds.getVersion(fid, v);
                if (fn.id == -1) break;
                cout << "RESULT|VERSION|" << fid << "|" << v << "|" << safe(fn.name) << "|" << safe(fn.path) << "|" << fn.parentId << "|" << fn.size << "|" << fn.modifiedAt << endl;
            }
            cout << "VERSION_DONE|0" << endl << flush;
        }
    }
}

// ═══════════════════════════════════
// MAIN
// ═══════════════════════════════════

int main(int argc, char* argv[]) {
    uf.addNode(0);
    loadSnapshots();
    bool machine = false;
    for (int i = 1; i < argc; i++) if (string(argv[i]) == "--machine") machine = true;
    if (machine) { runMachineMode(); return 0; }
    cout << "Menu mode disabled. Use --machine." << endl;
    return 0;
}
