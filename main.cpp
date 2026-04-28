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
#include <regex>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <dirent.h>
    #include <sys/stat.h>
    #include <sys/statvfs.h>
    #include <unistd.h>
#endif

using namespace std;

// M-5: Thread-safe localtime wrapper
static bool safeLocaltime(const long* ts, struct tm* out) {
    time_t t = (time_t)*ts;
#if defined(_MSC_VER)
    return localtime_s(out, &t) == 0;
#else
    struct tm* tmp = localtime(&t);
    if (!tmp) return false;
    *out = *tmp;
    return true;
#endif
}

// C-5: Compute 10-year day index (0-3649)
static int computeDayIndex(long ts) {
    struct tm ti{};
    if (!safeLocaltime(&ts, &ti)) return 0;
    int yearOff = ti.tm_year - 120; // years since 2020
    if (yearOff < 0) yearOff = 0;
    if (yearOff > 9) yearOff = 9;
    return yearOff * 365 + ti.tm_yday;
}

// ═══════════════════════════════════
// GLOBALS
// ═══════════════════════════════════
BPlusTree bpt;
Trie trie;
SegmentTree segTree(3650);
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
    for (size_t i = 0; i < r.length(); i++) if (r[i] == '|') r[i] = '\x01';
    return r;
}

static string unsafe(const string& s) {
    string r = s;
    for (size_t i = 0; i < r.length(); i++) if (r[i] == '\x01') r[i] = '|';
    return r;
}

void loadSnapshots() {
    ifstream ifs("snapshots.dat");
    if (!ifs) return;
    string header;
    while (getline(ifs, header)) {
        if (header != "SNAPSHOT_V1") continue;
        Snapshot s;
        string meta;
        if (!getline(ifs, meta)) break;
        istringstream mss(meta);
        mss >> s.id >> s.timestamp >> s.fileCount >> s.totalBytes;
        if (s.fileCount < 0 || s.fileCount > 10000000) break;
        for (int i = 0; i < s.fileCount; i++) {
            string fline;
            if (!getline(ifs, fline)) break;
            size_t sp = fline.rfind(' ');
            if (sp == string::npos) continue;
            string path = fline.substr(0, sp);
            long long sz = stoll(fline.substr(sp + 1));
            s.fileMap[path] = sz;
        }
        string endLine;
        getline(ifs, endLine); // consume END
        snapshots.push_back(s);
        if (s.id >= nextSnapshotId) nextSnapshotId = s.id + 1;
    }
}

void saveSnapshots() {
    ofstream ofs("snapshots.dat", ios::trunc);
    for (size_t i = 0; i < snapshots.size(); i++) {
        const auto& s = snapshots[i];
        ofs << "SNAPSHOT_V1" << "\n";
        ofs << s.id << " " << s.timestamp << " " << s.fileCount << " " << s.totalBytes << "\n";
        for (auto it = s.fileMap.begin(); it != s.fileMap.end(); ++it) {
            ofs << it->first << " " << it->second << "\n";
        }
        ofs << "END" << "\n";
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
    for(size_t i = 0; i < all.size(); i++) indexedSize += all[i].size;

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
            pair<int, long> sub = loadDirectory(fullPath, dirId);
            count += sub.first; totalBytes += sub.second;
        } else {
            ULARGE_INTEGER ull;
            ull.LowPart = ffd.ftLastWriteTime.dwLowDateTime;
            ull.HighPart = ffd.ftLastWriteTime.dwHighDateTime;
            long ts = (long)((ull.QuadPart - 116444736000000000ULL) / 10000000ULL);
            long fileSize = (long)(((long long)ffd.nFileSizeHigh << 32) | ffd.nFileSizeLow);
            int id = nextFileId++;
            FileNode f(fname, fullPath, parentId, fileSize, ts, false); 
            f.id = id; f.modifiedAt = ts;
            bpt.insert(fullPath, f); trie.insert(fname, id);
            segTree.addFile(computeDayIndex(ts), f.size);
            uf.addNode(id); uf.unite(id, parentId);
            allFileIds.push_back(id);
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
            pair<int, long> sub = loadDirectory(fullPath, dirId);
            count += sub.first; totalBytes += sub.second;
        } else if (S_ISREG(st.st_mode)) {
            long fileSize = (long)st.st_size;
            long ts = (long)st.st_mtime;
            int id = nextFileId++;
            FileNode f(fname, fullPath, parentId, fileSize, ts, false);
            f.id = id; f.modifiedAt = ts;
            bpt.insert(fullPath, f); trie.insert(fname, id);
            segTree.addFile(computeDayIndex(ts), f.size);
            uf.addNode(id); uf.unite(id, parentId);
            allFileIds.push_back(id);
            count++; totalBytes += fileSize;
            if (count % 100 == 0) cout << "INDEXING|" << count << "|" << safe(fullPath) << endl << flush;
        }
    }
    closedir(dir);
#endif
    return {count, totalBytes};
}

void performDeepAnalysis(vector<FileNode>& files) {
    for (size_t i = 0; i < files.size(); i++) {
        if ((files[i].trueType == "UNKNOWN" || files[i].entropy == 0.0) && files[i].size > 0) {
            FileNode deep(files[i].name, files[i].path, files[i].parentId, files[i].size, files[i].createdAt, true);
            deep.id = files[i].id;
            deep.modifiedAt = files[i].modifiedAt;
            files[i] = deep;
            bpt.remove(deep.path);
            bpt.insert(deep.path, deep);
        }
    }
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
                string fname, pdir, sz_str, mt_str, ttype, ent_str;
                if(getline(biss, fname, '|') && getline(biss, pdir, '|') && getline(biss, sz_str, '|') && getline(biss, mt_str, '|') && getline(biss, ttype, '|') && getline(biss, ent_str)) {
                    try {
                        long sz = stol(sz_str);
                        long mt = stol(mt_str);
                        double ent = stod(ent_str);
                        int parentId = 0;
                        if(directoryIds.find(pdir) != directoryIds.end()) {
                            parentId = directoryIds[pdir];
                        } else {
                            parentId = nextDirId++;
                            directoryIds[pdir] = parentId;
                            uf.addNode(parentId);
                            uf.unite(parentId, 0); // All dirs connect to root
                        }
                        int id = nextFileId++;
                        FileNode f(fname, pdir + "/" + fname, parentId, sz, mt, false);
                        f.id = id; f.modifiedAt = mt;
                        f.trueType = ttype; f.entropy = ent;
                        bpt.insert(pdir + "/" + fname, f); trie.insert(fname, id);
                        segTree.addFile(computeDayIndex(mt), f.size);
                        uf.addNode(id); uf.unite(id, parentId);
                        pds.saveVersion(f); // Create baseline version
                        allFileIds.push_back(id);
                        count++;
                    } catch (...) {}
                }
            }
            cout << "BATCH_DONE|" << count << endl << flush;
        }
        else if (cmd == "DISK_INFO") { sendDiskInfo(scanRoot); }
        else if (cmd == "SNAPSHOT") {
            vector<FileNode> all = bpt.getAllLeaves();
            Snapshot s;
            s.id = nextSnapshotId++;
            s.timestamp = (long long)time(0);
            s.fileCount = (int)all.size();
            s.totalBytes = 0;
            for (size_t i = 0; i < all.size(); i++) {
                s.totalBytes += all[i].size;
                s.fileMap[all[i].path] = all[i].size;
            }
            snapshots.push_back(s);
            saveSnapshots();
            cout << "SNAPSHOT_OK|" << s.id << "|" << s.fileCount << "|" << s.totalBytes << endl << flush;
        }
        else if (cmd == "DIFF") {
            int id1, id2; if (!(iss >> id1 >> id2)) { cout << "ERROR|DIFF|args" << endl << flush; continue; }
            Snapshot *s1 = nullptr, *s2 = nullptr;
            for (size_t i = 0; i < snapshots.size(); i++) {
                if (snapshots[i].id == id1) s1 = &snapshots[i];
                if (snapshots[i].id == id2) s2 = &snapshots[i];
            }
            if (!s1 || !s2) { cout << "ERROR|DIFF|not_found" << endl << flush; continue; }
            
            int added = 0, deleted = 0, modified = 0, unchanged = 0;
            for (auto it = s2->fileMap.begin(); it != s2->fileMap.end(); ++it) {
                if (s1->fileMap.find(it->first) == s1->fileMap.end()) added++;
                else if (s1->fileMap[it->first] != it->second) modified++;
                else unchanged++;
            }
            for (auto it = s1->fileMap.begin(); it != s1->fileMap.end(); ++it) {
                if (s2->fileMap.find(it->first) == s2->fileMap.end()) deleted++;
            }
            cout << "DIFF_RESULT|" << added << "|" << deleted << "|" << modified << "|" << unchanged << endl << flush;
        }
        else if (cmd == "WARNINGS") {
            vector<FileNode> all = bpt.getAllLeaves();
            map<string, vector<FileNode> > groups;
            for (size_t i = 0; i < all.size(); i++) groups[all[i].checksum].push_back(all[i]);
            long long dupeWaste = 0;
            for (auto it = groups.begin(); it != groups.end(); ++it) 
                if (it->second.size() > 1) dupeWaste += (long long)(it->second.size() - 1) * it->second[0].size;
            if (dupeWaste > 500 * 1048576LL) cout << "WARN|DUPLICATES|Duplicate files wasting " << (dupeWaste / 1048576) << " MB|" << dupeWaste << endl;
            long long oldWaste = 0;
            long long twoYearsAgo = (long long)time(0) - (2LL * 365 * 86400);
            for (size_t i = 0; i < all.size(); i++) if ((long long)all[i].modifiedAt < twoYearsAgo) oldWaste += all[i].size;
            if (oldWaste > 1024 * 1048576LL) cout << "WARN|OLD_FILES|Files >2yrs occupy " << (oldWaste / 1048576) << " MB|" << oldWaste << endl;
            cout << "WARNINGS_DONE" << endl << flush;
        }
        else if (cmd == "RECLAIM") {
            vector<FileNode> all = bpt.getAllLeaves();
            long long oneYearAgo = (long long)time(0) - (365LL * 86400);
            long long totalRec = 0;
            map<string, vector<FileNode> > groups;
            for (size_t i = 0; i < all.size(); i++) groups[all[i].checksum].push_back(all[i]);
            long long dupeRec = 0;
            for (auto it = groups.begin(); it != groups.end(); ++it) 
                if (it->second.size() > 1) dupeRec += (long long)(it->second.size() - 1) * it->second[0].size;
            cout << "REC|DUPLICATES|" << dupeRec << endl; totalRec += dupeRec;
            long long oldRec = 0;
            for (size_t i = 0; i < all.size(); i++) if ((long long)all[i].modifiedAt < oneYearAgo) oldRec += all[i].size;
            cout << "REC|OLD_FILES|" << oldRec << endl; totalRec += oldRec;
            for (size_t i = 0; i < all.size(); i++) if (all[i].size > 100 * 1048576LL) cout << "REC|LARGE_FILE|" << safe(all[i].name) << "|" << all[i].size << "|" << safe(all[i].path) << endl;
            cout << "RECLAIM_DONE|" << totalRec << endl << flush;
        }
        else if (cmd == "DELETE") {
            int fid = -1; iss >> fid;
            vector<FileNode> all = bpt.getAllLeaves();
            bool found = false;
            for (size_t i = 0; i < all.size(); i++) if (all[i].id == fid) {
                pds.saveVersion(all[i]);
                pds.backupFile(all[i]);
                if (remove(all[i].path.c_str()) == 0) { 
                    bpt.remove(all[i].path); 
                    trie.remove(all[i].name); 
                    cout << "DELETE_OK|" << fid << endl; 
                }
                else cout << "ERROR|DELETE|failed" << endl;
                found = true; break;
            }
            if(!found) cout << "ERROR|DELETE|not_found" << endl;
            cout << flush;
        }
        else if (cmd == "DELETE_ORPHANS") {
            vector<int> orphans = uf.findAllOrphans(0, allFileIds);
            vector<FileNode> all = bpt.getAllLeaves();
            int count = 0;
            for (size_t i = 0; i < orphans.size(); i++) {
                for (size_t j = 0; j < all.size(); j++) {
                    if (all[j].id == orphans[i]) {
                        if (remove(all[j].path.c_str()) == 0) {
                            bpt.remove(all[j].path); trie.remove(all[j].name);
                            count++;
                        }
                        break;
                    }
                }
            }
            cout << "ORPHAN_DONE|" << count << "|0" << endl << flush;
        }
        else if (cmd == "DUMP_CACHE") {
            vector<FileNode> all = bpt.getAllLeaves();
            for (size_t i = 0; i < all.size(); i++) {
                cout << "CACHE_ITEM|" << safe(all[i].name) << "|" << safe(all[i].path) << "|" << all[i].size << "|" << all[i].modifiedAt << "|" << all[i].trueType << "|" << fixed << setprecision(4) << all[i].entropy << endl;
            }
            cout << "CACHE_DONE" << endl << flush;
        }
        else if (cmd == "TYPE_DISTRIBUTION") {
            vector<FileNode> all = bpt.getAllLeaves();
            performDeepAnalysis(all); 
            map<string, long long> types;
            for (size_t i = 0; i < all.size(); i++) types[all[i].trueType] += all[i].size;
            
            struct Comp {
                bool operator()(const pair<string, long long>& a, const pair<string, long long>& b) {
                    return a.second > b.second;
                }
            };
            vector<pair<string, long long> > sortedTypes(types.begin(), types.end());
            sort(sortedTypes.begin(), sortedTypes.end(), Comp());
            
            for (int i = 0; i < min((int)sortedTypes.size(), 6); i++) {
                cout << "TYPE|" << sortedTypes[i].first << "|" << sortedTypes[i].second << endl;
            }
            cout << "TYPE_DONE" << endl << flush;
        }
        else if (cmd == "SUSPICIOUS") {
            vector<FileNode> all = bpt.getAllLeaves();
            performDeepAnalysis(all);
            int count = 0;
            for (size_t i = 0; i < all.size(); i++) {
                if (all[i].entropy > 7.5) {
                    cout << "SUSPICIOUS|" << safe(all[i].name) << "|" << safe(all[i].path) << "|" << fixed << setprecision(2) << all[i].entropy << endl;
                    count++;
                }
            }
            cout << "SUSPICIOUS_DONE|" << count << endl << flush;
        }
        else if (cmd == "LOAD_DIR") {
            string path; getline(iss >> ws, path);
            if (path.empty()) { cout << "ERROR|LOAD_DIR|no path" << endl << flush; continue; }
            pair<int, long> res = loadDirectory(path);
            // E-7: Initialize dense vectors for file ID range
            if (nextFileId > 10000) uf.initDense(10000, nextFileId - 1);
            cout << "LOADED|" << res.first << "|" << res.second << endl << flush;
        }
        else if (cmd == "ROLLBACK") {
            int fid, ver; if (!(iss >> fid >> ver)) { cout << "ROLLBACK_ERROR|args" << endl << flush; continue; }
            // C-6: rollback() now prints ROLLBACK_OK or ROLLBACK_ERROR directly
            pds.rollback(fid, ver, bpt);
            cout << flush;
        }
        else if (cmd == "SEARCH") {
            string query; getline(iss >> ws, query);
            string filename = query;
            long minSize = -1;
            size_t pos = query.find(" --size-gt ");
            if (pos != string::npos) {
                filename = query.substr(0, pos);
                try { minSize = stol(query.substr(pos + 11)); } catch(...) {}
            }

            FileNode* node = bpt.search(filename);
            if (node) {
                if (minSize == -1 || node->size > minSize) {
                    cout << "RESULT|FOUND|" << safe(node->name) << "|" << safe(node->path) << "|" << node->size / 1024 << "|Engine|" << safe(node->path) << "|0|Direct" << endl << flush;
                } else {
                    cout << "RESULT|NOT_FOUND|" << safe(filename) << " (Filtered)|| |Engine||0|Direct" << endl << flush;
                }
            } else {
                cout << "RESULT|NOT_FOUND|" << safe(filename) << "|||Engine||0|Direct" << endl << flush;
            }
        }
        else if (cmd == "REGEX") {
            string pattern; getline(iss >> ws, pattern);
            try {
                regex re(pattern, regex_constants::icase);
                vector<FileNode> all = bpt.getAllLeaves();
                int count = 0;
                for (size_t i = 0; i < all.size(); i++) {
                    if (regex_search(all[i].name, re)) {
                        cout << "RESULT|FOUND|" << safe(all[i].name) << "|" << safe(all[i].path) << "|" << all[i].size / 1024 << "|Engine|" << safe(all[i].path) << "|0|Regex" << endl;
                        count++;
                        if (count > 100) break; 
                    }
                }
                if (count == 0) cout << "RESULT|NOT_FOUND|" << safe(pattern) << "|||Engine||0|Regex" << endl;
            } catch (const exception& e) {
                cout << "ERROR|REGEX|" << e.what() << endl;
            }
            cout << flush;
        }
        else if (cmd == "PREFIX") {
            string prefix; getline(iss >> ws, prefix);
            vector<string> names = trie.prefixSearch(prefix);
            if (!names.empty()) {
                for (size_t i = 0; i < names.size(); i++) {
                    FileNode* fn = bpt.search(names[i]);
                    if (fn) {
                        cout << "RESULT|FOUND|" << safe(fn->name) << "|" << safe(fn->path) << "|" << fn->size / 1024 << "|Engine|" << safe(fn->path) << "|0|Prefix" << endl;
                    }
                }
            } else {
                cout << "RESULT|NOT_FOUND|" << safe(prefix) << "|||Engine||0|Prefix" << endl << flush;
            }
        }
        else if (cmd == "DUPLICATES") {
            vector<FileNode> all = bpt.getAllLeaves();
            performDeepAnalysis(all);
            map<string, vector<FileNode> > groups;
            for (size_t i = 0; i < all.size(); i++) groups[all[i].checksum].push_back(all[i]);
            int count = 0; long long total = 0;
            for (auto it = groups.begin(); it != groups.end(); ++it) {
                if (it->second.size() > 1) {
                    count++; total += (long long)(it->second.size()-1) * it->second[0].size;
                    string paths = "";
                    for(size_t j = 0; j < it->second.size(); j++) paths += safe(it->second[j].path) + ":" + to_string(it->second[j].id) + ",";
                    cout << "DUP|" << safe(it->second[0].name) << "|" << safe(it->first) << "|" << paths << "|" << it->second[0].size/1024 << endl;
                }
            }
            cout << "DUP_DONE|" << count << "|" << total/1048576.0 << "|FAST" << endl << flush;
        }
        else if (cmd == "ORPHANS") {
            vector<int> orphans = uf.findAllOrphans(0, allFileIds);
            vector<FileNode> all = bpt.getAllLeaves();
            // H-4: Build id->node map for O(n) lookup instead of O(n^2)
            unordered_map<int, const FileNode*> idToNode;
            for (size_t i = 0; i < all.size(); i++) idToNode[all[i].id] = &all[i];
            long long total = 0;
            for (size_t i = 0; i < orphans.size(); i++) {
                auto it = idToNode.find(orphans[i]);
                if (it != idToNode.end()) {
                    const FileNode* f = it->second;
                    cout << "ORPHAN|" << f->id << "|" << safe(f->name) << "|" << safe(f->path) << "|" << f->size/1024 << endl;
                    total += f->size;
                }
            }
            cout << "ORPHAN_DONE|" << (int)orphans.size() << "|" << total/1048576.0 << endl << flush;
        }
        else if (cmd == "ANALYTICS") {
            // C-5: Use year-aware day offsets
            time_t now = time(0);
            struct tm tnow{};
            safeLocaltime((long*)&now, &tnow);
            int yearOffset = tnow.tm_year - 120;
            if (yearOffset < 0) yearOffset = 0;
            if (yearOffset > 9) yearOffset = 9;
            int base = yearOffset * 365;
            for (int i = 0; i < 12; i++) {
                const string months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
                const int starts[] = {0,31,59,90,120,151,181,212,243,273,304,334};
                const int ends[] = {30,58,89,119,150,180,211,242,272,303,333,364};
                cout << "MONTH|" << months[i] << "|" << fixed << setprecision(2) << segTree.queryRange(base + starts[i], base + ends[i])/1048576.0 << endl;
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
