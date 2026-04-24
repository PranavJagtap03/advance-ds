#include "BPlusTree.h"
#include "Trie.h"
#include "SegmentTree.h"
#include "PersistentDS.h"
#include "UnionFind.h"
#include "DataGenerator.h"
#include "Features.h"
#include "QueryAnalyzer.h"
#include <iostream>
#include <string>
#include <iomanip>
#include <vector>
#include <sstream>
#include <algorithm>
#include <ctime>

// Platform-specific directory traversal
#ifdef _WIN32
    #include <windows.h>
#else
    #include <dirent.h>
    #include <sys/stat.h>
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

// ═══════════════════════════════════
// HELPER FUNCTIONS  (menu mode only)
// ═══════════════════════════════════

void printSeparator() {
    cout << string(60, '=') << endl;
}

void printBanner() {
    cout << "============================================" << endl;
    cout << "    FILE SYSTEM DIRECTORY MANAGER v1.0" << endl;
    cout << "    B+Tree | Trie | SegTree | Persistent" << endl;
    cout << "============================================" << endl;
}

void printFileTable(const vector<FileNode>& files) {
    if (files.empty()) {
        cout << "No files found." << endl;
        return;
    }
    
    cout << left << setw(6) << "ID" << " | " << setw(28) << "Name" << " | " 
         << setw(10) << "Size KB" << " | " << setw(28) << "Path" << endl;
    cout << string(80, '-') << endl;
    
    for (const auto& f : files) {
        cout << left << setw(6) << f.id << " | " << setw(28) << f.name << " | " 
             << setw(10) << f.size / 1024 << " | " << setw(28) << f.path << endl;
    }
    cout << "Total: " << files.size() << " file(s) found." << endl;
}

void printMenu() {
    cout << "\n---- MENU ----\n";
    cout << "1.  Add file\n";
    cout << "2.  Delete file\n";
    cout << "3.  Search by name prefix\n";
    cout << "4.  Search by size range (KB)\n";
    cout << "5.  Storage analytics by month\n";
    cout << "6.  Find duplicate files\n";
    cout << "7.  File version history\n";
    cout << "8.  Edit file (creates new version)\n";
    cout << "9.  Rollback file to old version\n";
    cout << "10. Detect orphan files\n";
    cout << "11. Generate 10000 test files\n";
    cout << "12. Run benchmark\n";
    cout << "13. Run stress tests\n";
    cout << "14. Show system info\n";
    cout << "0.  Exit\n";
    cout << "Choice: " << flush;
}

// ═══════════════════════════════════
// FEATURE FUNCTIONS  (menu mode)
// ═══════════════════════════════════

void addFile() {
    string name, path;
    long sizeKB;
    int pid;

    cout << "Enter filename: ";
    cin >> name;
    cin.ignore(1000, '\n');
    
    if (name.empty()) {
        cout << "Filename cannot be empty." << endl;
        return;
    }

    cout << "Enter path (e.g. /documents/file.txt): ";
    getline(cin, path);

    cout << "Enter size in KB: ";
    if (!(cin >> sizeKB) || sizeKB <= 0) {
        cin.clear(); cin.ignore(1000, '\n');
        cout << "Invalid size." << endl;
        return;
    }

    cout << "Enter parent folder ID (1=docs,2=downloads,3=photos,4=backup): ";
    if (!(cin >> pid) || pid < 1 || pid > 4) {
        cin.clear(); cin.ignore(1000, '\n');
        cout << "Invalid parent folder ID." << endl;
        return;
    }
    cin.ignore(1000, '\n');

    FileNode f(name, path, pid, sizeKB * 1024, time(0));
    f.id = nextFileId++;
    f.modifiedAt = time(0);

    bpt.insert(name, f);
    trie.insert(name, f.id);
    int dayIndex = (f.createdAt / 86400) % 365;
    segTree.addFile(dayIndex, f.size);
    uf.addNode(f.id);
    uf.unite(f.id, f.parentId);
    pds.saveVersion(f);
    allFileIds.push_back(f.id);

    cout << "File added successfully! ID: " << f.id << endl;
}

void deleteFile() {
    string name;
    cout << "Enter filename to delete: ";
    cin >> name;
    cin.ignore(1000, '\n');

    FileNode* f = bpt.search(name);
    if (!f) {
        cout << "File not found: " << name << endl;
        return;
    }

    bpt.remove(name);
    trie.remove(name);
    cout << "Deleted: " << name << endl;
}

void searchPrefix() {
    string prefix;
    cout << "Enter prefix to search: ";
    cin >> prefix;
    cin.ignore(1000, '\n');

    vector<string> names = trie.prefixSearch(prefix);
    if (names.empty()) {
        cout << "No files found with prefix: " << prefix << endl;
        return;
    }

    vector<FileNode> results;
    for (const string& n : names) {
        FileNode* f = bpt.search(n);
        if (f) results.push_back(*f);
    }
    
    printFileTable(results);
}

void searchBySize() {
    long minKB, maxKB;
    cout << "Enter minimum size in KB: ";
    if (!(cin >> minKB) || minKB <= 0) {
        cin.clear(); cin.ignore(1000, '\n');
        cout << "Invalid minimum size." << endl;
        return;
    }
    cout << "Enter maximum size in KB: ";
    if (!(cin >> maxKB) || maxKB <= 0) {
        cin.clear(); cin.ignore(1000, '\n');
        cout << "Invalid maximum size." << endl;
        return;
    }
    cin.ignore(1000, '\n');

    if (minKB > maxKB) {
        cout << "Invalid range: min cannot be greater than max." << endl;
        return;
    }

    vector<FileNode> r = bpt.searchBySize(minKB * 1024, maxKB * 1024);
    printFileTable(r);
}

void showVersionHistory() {
    int fid;
    cout << "Enter file ID: ";
    if (!(cin >> fid)) {
        cin.clear(); cin.ignore(1000, '\n');
        cout << "Invalid format." << endl;
        return;
    }
    cin.ignore(1000, '\n');
    
    pds.showHistory(fid);
}

void editFile() {
    string name;
    cout << "Enter filename to edit: ";
    cin >> name;
    cin.ignore(1000, '\n');

    FileNode* f = bpt.search(name);
    if (!f) {
        cout << "File not found" << endl;
        return;
    }

    cout << "Current Details - Name: " << f->name << " Size: " << f->size / 1024 
         << " KB Path: " << f->path << " Version: " << f->version << endl;

    cout << "Enter new size in KB (0 to keep same): ";
    long newKB;
    if (!(cin >> newKB)) {
        cin.clear(); cin.ignore(1000, '\n');
        newKB = 0;
    } else {
        cin.ignore(1000, '\n');
    }

    cout << "Enter new path (press Enter to keep same): ";
    string newPath;
    getline(cin, newPath);

    FileNode updatedNode = *f;
    if (newKB > 0) {
        updatedNode.size = newKB * 1024;
        updatedNode.checksum = to_string(updatedNode.size) + updatedNode.name.substr(0, min(3, (int)updatedNode.name.size()));
    }
    if (!newPath.empty()) {
        updatedNode.path = newPath;
    }
    updatedNode.modifiedAt = time(0);
    pds.saveVersion(updatedNode);

    bpt.remove(name);
    bpt.insert(updatedNode.name, updatedNode);

    cout << "File updated! Version " << updatedNode.version << " saved." << endl;
}

void rollbackFile() {
    int fid;
    cout << "Enter file ID: ";
    if (!(cin >> fid)) {
        cin.clear(); cin.ignore(1000, '\n');
        cout << "Invalid input." << endl;
        return;
    }
    cin.ignore(1000, '\n');

    if (!pds.hasHistory(fid)) {
        cout << "No history for this ID" << endl;
        return;
    }
    pds.showHistory(fid);

    int ver;
    cout << "Enter version number to rollback to: ";
    if (!(cin >> ver)) {
        cin.clear(); cin.ignore(1000, '\n');
        cout << "Invalid version input." << endl;
        return;
    }
    cin.ignore(1000, '\n');

    pds.rollback(fid, ver, bpt);
}

void detectOrphans() {
    vector<int> orphans = uf.findAllOrphans(0, allFileIds);
    if (orphans.empty()) {
        cout << "No orphan files found. All files connected." << endl;
        return;
    }

    cout << "ORPHAN FILES DETECTED:" << endl;
    long totalSize = 0;
    vector<FileNode> allLeaves = bpt.getAllLeaves();

    for (int id : orphans) {
        for (const auto& file : allLeaves) {
            if (file.id == id) {
                cout << "  ID:" << file.id << " Name:" << file.name << " Size:" << file.size / 1024 << " KB Path:" << file.path << endl;
                totalSize += file.size;
                break;
            }
        }
    }
    cout << "Total: " << orphans.size() << " orphans, " << fixed << setprecision(2) << (totalSize / 1048576.0) << " MB potentially unrecoverable" << endl;
}

void showSystemInfo() {
    cout << "=== SYSTEM INFO ===" << endl;
    cout << "Total files: " << bpt.getSize() << endl;
    cout << "Total indexed names (Trie): " << allFileIds.size() << endl;
    cout << "Storage tracked: " << fixed << setprecision(2) << (segTree.queryRange(0, 364) / 1048576.0) << " MB" << endl;
    
    int filesWithHistory = 0;
    for (int id : allFileIds) {
        if (pds.hasHistory(id)) filesWithHistory++;
    }
    cout << "Files with version history: " << filesWithHistory << endl;
}

// ═══════════════════════════════════════════════════════════════════
// MACHINE MODE — LOAD_DIR implementation
// ═══════════════════════════════════════════════════════════════════

// Join a vector of strings with a delimiter
static string joinStrings(const vector<string>& v, char delim = ',') {
    string s;
    for (int i = 0; i < (int)v.size(); i++) {
        if (i > 0) s += delim;
        s += v[i];
    }
    return s;
}

// Sanitise a pipe-unsafe string (replace | with _)
static string safe(const string& s) {
    string r = s;
    for (char& c : r) if (c == '|') c = '_';
    return r;
}

    // Returns {count_loaded, total_bytes}
pair<int,long> loadDirectory(const string& dirPath) {
    int count = 0;
    long totalBytes = 0;

#ifdef _WIN32
    // Windows: use FindFirstFile / FindNextFile
    string pattern = dirPath + "\\*";
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return {count, totalBytes};

    do {
        string fname = ffd.cFileName;
        if (fname == "." || fname == "..") continue;
        
        // Skip hidden, system, and junction points (symlinks) to prevent infinite loops on C:\
        if (ffd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | 0x400)) continue;

        string fullPath = dirPath + "\\" + fname;

        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Recurse
            auto sub = loadDirectory(fullPath);
            count     += sub.first;
            totalBytes += sub.second;
        } else {
            // File
            LARGE_INTEGER sz;
            sz.LowPart  = ffd.nFileSizeLow;
            sz.HighPart = (LONG)ffd.nFileSizeHigh;
            long fileSize = (long)sz.QuadPart;

            int id = nextFileId++;
            int parentId = 1; // assign to /documents by default
            long ts = (long)time(0);

            FileNode f(fname, fullPath, parentId, fileSize, ts);
            f.id = id;
            f.modifiedAt = ts;

            bpt.insert(fname, f);
            trie.insert(fname, f.id);
            int dayIndex = (ts / 86400) % 365;
            segTree.addFile(dayIndex, f.size);
            uf.addNode(f.id);
            uf.unite(f.id, f.parentId);
            pds.saveVersion(f);
            allFileIds.push_back(f.id);

            count++;
            totalBytes += fileSize;
            
            static int lastCount = 0;
            if (count > 0 && (count % 100 == 0 || count - lastCount >= 50)) {
                lastCount = count;
                cout << "INDEXING|" << count << "|" << safe(fullPath) << endl << flush;
            }
        }
    } while (FindNextFileA(hFind, &ffd) != 0);

    FindClose(hFind);

#else
    // POSIX: use opendir/readdir
    DIR* dir = opendir(dirPath.c_str());
    if (!dir) return {count, totalBytes};

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        string fname = entry->d_name;
        if (fname == "." || fname == "..") continue;

        string fullPath = dirPath + "/" + fname;

        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            auto sub = loadDirectory(fullPath);
            count     += sub.first;
            totalBytes += sub.second;
        } else {
            long fileSize = (long)st.st_size;
            int id = nextFileId++;
            int parentId = 1;
            long ts = (long)time(0);

            FileNode f(fname, fullPath, parentId, fileSize, ts);
            f.id = id;
            f.modifiedAt = ts;

            bpt.insert(fname, f);
            trie.insert(fname, f.id);
            int dayIndex = (ts / 86400) % 365;
            segTree.addFile(dayIndex, f.size);
            uf.addNode(f.id);
            uf.unite(f.id, f.parentId);
            pds.saveVersion(f);
            allFileIds.push_back(f.id);

            count++;
            totalBytes += fileSize;
            
            static int lastCountPosix = 0;
            if (count > 0 && (count % 100 == 0 || count - lastCountPosix >= 50)) {
                lastCountPosix = count;
                cout << "INDEXING|" << count << "|" << safe(fullPath) << endl << flush;
            }
        }
    }
    closedir(dir);
#endif

    return {count, totalBytes};
}

// ═══════════════════════════════════════════════════════════════════
// MACHINE MODE — command processor
// ═══════════════════════════════════════════════════════════════════

// Month name mapping for output
static const string MONTH_NAMES[12] = {
    "January","February","March","April","May","June",
    "July","August","September","October","November","December"
};
static const int MONTH_START_DAYS[12] = {0,31,59,90,120,151,181,212,243,273,304,334};
static const int MONTH_END_DAYS[12]   = {30,58,89,119,150,180,211,242,272,303,333,364};

void runMachineMode() {
    string line;
    while (getline(cin, line)) {
        // Trim trailing \r on Windows
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        istringstream iss(line);
        string cmd;
        iss >> cmd;

        // ── EXIT ────────────────────────────────────────────────────
        if (cmd == "EXIT") {
            cout << "BYE" << endl << flush;
            return;
        }

        // ── LOAD_DIR <path> ─────────────────────────────────────────
        else if (cmd == "LOAD_DIR") {
            string path;
            getline(iss >> ws, path);
            if (path.empty()) {
                cout << "ERROR|LOAD_DIR|no path provided" << endl << flush;
                continue;
            }
            try {
                pair<int,long> ldResult = loadDirectory(path);
                cout << "LOADED|" << ldResult.first << "|" << ldResult.second << endl << flush;
            } catch (...) {
                cout << "ERROR|LOAD_DIR|exception during directory scan" << endl << flush;
            }
        }

        // ── LOAD_RECORD <name>|<path>|<size_bytes>|<modified_ts> ────
        // Inserts a pre-scanned file record directly — no filesystem walk.
        // Used by the Python GUI to replay the SQLite cache on startup.
        else if (cmd == "LOAD_RECORD") {
            string rest;
            getline(iss >> ws, rest);
            // Parse pipe-delimited fields
            vector<string> fields;
            stringstream ss2(rest);
            string tok;
            while (getline(ss2, tok, '|')) fields.push_back(tok);
            if (fields.size() < 2) {
                cout << "ERROR|LOAD_RECORD|need name|path" << endl << flush;
                continue;
            }
            string fname    = fields[0];
            string fpath    = fields[1];
            long   fsize    = fields.size() > 2 ? stol(fields[2]) : 0;
            long   fmod     = fields.size() > 3 ? stol(fields[3]) : 0;

            FileNode node(fname, fpath, 0, fsize, fmod);
            node.id         = nextFileId++;
            node.modifiedAt = fmod;
            node.checksum   = to_string(fsize) + fname.substr(0, min(3, (int)fname.size()));

            bpt.insert(fname, node);
            trie.insert(fname, node.id);
            if (fmod > 0) {
                struct tm* t = localtime(&fmod);
                if (t) {
                    int day = t->tm_yday + 1;
                    segTree.addFile(day, fsize);
                }
            }
            allFileIds.push_back(node.id);
            cout << "REC_OK|" << node.id << endl << flush;
        }

        // ── LOAD_BATCH_BEGIN / LOAD_BATCH_END ────────────────────────
        // Accepts many LOAD_RECORD lines bracketed by these markers.
        // Returns BATCH_DONE|count at the end.
        else if (cmd == "LOAD_BATCH_BEGIN") {
            int batchCount = 0;
            string bline;
            while (getline(cin, bline)) {
                if (!bline.empty() && bline.back() == '\r') bline.pop_back();
                if (bline == "LOAD_BATCH_END") break;
                // Each line is: name|path|size|modified
                vector<string> fields;
                stringstream ss3(bline);
                string tok3;
                while (getline(ss3, tok3, '|')) fields.push_back(tok3);
                if (fields.size() < 2) continue;
                string fname = fields[0];
                string fpath = fields[1];
                long fsize   = fields.size() > 2 ? stol(fields[2]) : 0;
                long fmod    = fields.size() > 3 ? stol(fields[3]) : 0;
                FileNode node(fname, fpath, 0, fsize, fmod);
                node.id  = nextFileId++;
                node.modifiedAt = fmod;
                node.checksum = to_string(fsize) + fname.substr(0, min(3, (int)fname.size()));
                bpt.insert(fname, node);
                trie.insert(fname, node.id);
                if (fmod > 0) {
                    struct tm* t = localtime(&fmod);
                    if (t) {
                        int day = t->tm_yday + 1;
                        segTree.addFile(day, fsize);
                    }
                }
                allFileIds.push_back(node.id);
                batchCount++;
            }
            cout << "BATCH_DONE|" << batchCount << endl << flush;
        }

        // ── GENERATE <count> ────────────────────────────────────────
        else if (cmd == "GENERATE") {
            int n = 0;
            iss >> n;
            if (n <= 0) {
                cout << "ERROR|GENERATE|invalid count" << endl << flush;
                continue;
            }
            // Suppress generateData()'s cout by redirecting stdout temporarily
            // We simply call it and ignore its prints (they go to stdout pre-flush).
            // Since machine mode must not print banners, we redirect.
            // Simplest safe approach: call generateData and let its prints happen
            // (they are informational messages during generation).
            // Per spec: output only the final structured line.
            // We save/restore cout stream buffer.
            streambuf* oldBuf = cout.rdbuf(nullptr); // suppress generateData output
            generateData(n);
            cout.rdbuf(oldBuf); // restore
            cout << "GENERATED|" << n << endl << flush;
        }

        // ── SEARCH <filename> ────────────────────────────────────────
        else if (cmd == "SEARCH") {
            string filename;
            iss >> filename;
            if (filename.empty()) {
                cout << "ERROR|SEARCH|no filename provided" << endl << flush;
                continue;
            }

            QueryResult qr = classify(filename);
            pair<FileNode*, string> swpResult = bpt.searchWithPath(filename);
            FileNode* node  = swpResult.first;
            string    tpath = swpResult.second;

            // Count hops from path string
            int hops = 0;
            {
                size_t pos = tpath.find('[');
                if (pos != string::npos) {
                    sscanf(tpath.c_str() + pos + 1, "%d", &hops);
                }
            }

            if (node) {
                cout << "RESULT|FOUND"
                     << "|" << safe(node->name)
                     << "|" << safe(node->path)
                     << "|" << node->size / 1024
                     << "|" << qr.dsName
                     << "|" << safe(tpath)
                     << "|" << hops
                     << "|" << safe(qr.reason)
                     << endl << flush;
            } else {
                cout << "RESULT|NOT_FOUND"
                     << "|" << safe(filename)
                     << "|"
                     << "|"
                     << "|" << qr.dsName
                     << "|"
                     << "|" << hops
                     << "|" << safe(qr.reason)
                     << endl << flush;
            }
        }

        // ── PREFIX <prefix> ─────────────────────────────────────────
        else if (cmd == "PREFIX") {
            string prefix;
            iss >> prefix;
            if (prefix.empty()) {
                cout << "ERROR|PREFIX|no prefix provided" << endl << flush;
                continue;
            }

            pair<vector<string>, string> pswpResult = trie.prefixSearchWithPath(prefix);
            vector<string> names = pswpResult.first;
            string         tpath = pswpResult.second;
            QueryResult qr = classify(prefix + "*");

            cout << "RESULT|PREFIX"
                 << "|" << joinStrings(names)
                 << "|" << safe(tpath)
                 << "|" << names.size()
                 << "|" << safe(qr.reason)
                 << endl << flush;
        }

        // ── SIZE_RANGE <min_kb> <max_kb> ─────────────────────────────
        else if (cmd == "SIZE_RANGE") {
            long minKB = 0, maxKB = 0;
            if (!(iss >> minKB >> maxKB) || minKB < 0 || maxKB < 0) {
                cout << "ERROR|SIZE_RANGE|invalid arguments" << endl << flush;
                continue;
            }

            vector<FileNode> results = bpt.searchBySize(minKB * 1024, maxKB * 1024);
            vector<string> names;
            for (const auto& f : results) names.push_back(f.name);

            QueryResult qr = classify("size:" + to_string(minKB) + "-" + to_string(maxKB));

            cout << "RESULT|SIZE"
                 << "|" << joinStrings(names)
                 << "|" << results.size()
                 << "|SegmentTree"
                 << "|" << safe(qr.reason)
                 << endl << flush;
        }

        // ── DATE_RANGE <month_start> <month_end> ─────────────────────
        else if (cmd == "DATE_RANGE") {
            int m1 = 0, m2 = 0;
            if (!(iss >> m1 >> m2) || m1 < 1 || m1 > 12 || m2 < 1 || m2 > 12 || m1 > m2) {
                cout << "ERROR|DATE_RANGE|month values must be 1-12 and start<=end" << endl << flush;
                continue;
            }

            int dayStart = MONTH_START_DAYS[m1 - 1];
            int dayEnd   = MONTH_END_DAYS[m2 - 1];
            long bytes   = segTree.queryRange(dayStart, dayEnd);
            double mb    = bytes / 1048576.0;

            cout << "RESULT|DATE"
                 << "|" << fixed << setprecision(2) << mb
                 << "|" << m1
                 << "|" << m2
                 << "|SegmentTree"
                 << endl << flush;
        }

        // ── DUPLICATES ───────────────────────────────────────────────
        else if (cmd == "DUPLICATES") {
            vector<FileNode> all = bpt.getAllLeaves();
            map<string, vector<FileNode>> groups;
            for (const auto& f : all) groups[f.checksum].push_back(f);

            int groupCount = 0;
            double totalWastedMB = 0.0;

            for (const auto& kv : groups) {
                if (kv.second.size() <= 1) continue;
                groupCount++;
                const auto& files = kv.second;
                long wastedKB = (long)(files.size() - 1) * files[0].size / 1024;
                totalWastedMB += wastedKB / 1024.0;

                vector<string> paths;
                for (const auto& f : files) paths.push_back(f.path);

                cout << "DUP"
                     << "|" << safe(files[0].name)
                     << "|" << safe(kv.first)
                     << "|" << joinStrings(paths)
                     << "|" << wastedKB
                     << endl << flush;
            }

            cout << "DUP_DONE"
                 << "|" << groupCount
                 << "|" << fixed << setprecision(2) << totalWastedMB
                 << endl << flush;
        }

        // ── ORPHANS ───────────────────────────────────────────────────
        else if (cmd == "ORPHANS") {
            vector<int> orphans = uf.findAllOrphans(0, allFileIds);
            vector<FileNode> allLeaves = bpt.getAllLeaves();
            long totalSize = 0;

            for (int id : orphans) {
                for (const auto& f : allLeaves) {
                    if (f.id == id) {
                        cout << "ORPHAN"
                             << "|" << f.id
                             << "|" << safe(f.name)
                             << "|" << safe(f.path)
                             << "|" << f.size / 1024
                             << endl << flush;
                        totalSize += f.size;
                        break;
                    }
                }
            }

            cout << "ORPHAN_DONE"
                 << "|" << orphans.size()
                 << "|" << fixed << setprecision(2) << (totalSize / 1048576.0)
                 << endl << flush;
        }

        // ── ANALYTICS ─────────────────────────────────────────────────
        else if (cmd == "ANALYTICS") {
            double totalMB = 0.0;
            for (int i = 0; i < 12; i++) {
                long bytes = segTree.queryRange(MONTH_START_DAYS[i], MONTH_END_DAYS[i]);
                double mb = bytes / 1048576.0;
                totalMB += mb;
                cout << "MONTH"
                     << "|" << MONTH_NAMES[i]
                     << "|" << fixed << setprecision(2) << mb
                     << endl << flush;
            }
            cout << "ANALYTICS_DONE"
                 << "|" << fixed << setprecision(2) << totalMB
                 << endl << flush;
        }

        // ── VERSION_HISTORY <file_id> ─────────────────────────────────
        else if (cmd == "VERSION_HISTORY") {
            int fid = -1;
            iss >> fid;
            if (fid < 0) {
                cout << "ERROR|VERSION_HISTORY|invalid file id" << endl << flush;
                continue;
            }
            if (!pds.hasHistory(fid)) {
                cout << "VERSION_DONE|0" << endl << flush;
                continue;
            }

            int count = 0;
            // Access versions via getVersion(fid, verNum) — iterate until not found
            for (int v = 1; ; v++) {
                FileNode fn = pds.getVersion(fid, v);
                if (fn.id == -1) break;
                cout << "RESULT|VERSION"
                     << "|" << fid
                     << "|" << v
                     << "|" << safe(fn.name)
                     << "|" << safe(fn.path)
                     << "|" << fn.parentId
                     << "|" << fn.size
                     << "|" << fn.modifiedAt
                     << endl << flush;
                count++;
            }
            cout << "VERSION_DONE|" << count << endl << flush;
        }

        // ── BENCHMARK ─────────────────────────────────────────────────
        else if (cmd == "BENCHMARK") {
            using namespace std::chrono;

            // Suppress generateData output during benchmark
            streambuf* oldBuf = cout.rdbuf(nullptr);
            generateData(10000);
            cout.rdbuf(oldBuf);

            vector<FileNode> all = bpt.getAllLeaves();
            if (all.empty()) {
                cout << "ERROR|BENCHMARK|no data available" << endl << flush;
                continue;
            }

            // TEST 1 — Search
            auto t0 = high_resolution_clock::now();
            for (int i = 0; i < 1000; i++) bpt.search(all[rand() % all.size()].name);
            auto t1 = high_resolution_clock::now();
            long bptSearch = duration_cast<microseconds>(t1 - t0).count();

            t0 = high_resolution_clock::now();
            for (int i = 0; i < 1000; i++) {
                string sn = all[rand() % all.size()].name;
                for (const auto& f : all) { if (f.name == sn) break; }
            }
            t1 = high_resolution_clock::now();
            long linSearch = duration_cast<microseconds>(t1 - t0).count();

            double sp1 = bptSearch > 0 ? (double)linSearch / bptSearch : 0.0;
            cout << "BENCH|Search(1000ops)|" << bptSearch << "|" << linSearch
                 << "|" << fixed << setprecision(2) << sp1 << endl << flush;

            // TEST 2 — Range search
            t0 = high_resolution_clock::now();
            for (int i = 0; i < 100; i++) bpt.rangeSearch("a", "m");
            t1 = high_resolution_clock::now();
            long bptRange = duration_cast<microseconds>(t1 - t0).count();

            t0 = high_resolution_clock::now();
            for (int i = 0; i < 100; i++) {
                vector<FileNode> r;
                for (const auto& f : all) if (f.name >= "a" && f.name <= "m") r.push_back(f);
            }
            t1 = high_resolution_clock::now();
            long linRange = duration_cast<microseconds>(t1 - t0).count();

            double sp2 = bptRange > 0 ? (double)linRange / bptRange : 0.0;
            cout << "BENCH|Range(100ops)|" << bptRange << "|" << linRange
                 << "|" << fixed << setprecision(2) << sp2 << endl << flush;

            // TEST 3 — Prefix search
            t0 = high_resolution_clock::now();
            for (int i = 0; i < 1000; i++) trie.prefixSearch("rep");
            t1 = high_resolution_clock::now();
            long triePrefix = duration_cast<microseconds>(t1 - t0).count();

            t0 = high_resolution_clock::now();
            for (int i = 0; i < 1000; i++) {
                vector<FileNode> r;
                for (const auto& f : all) if (f.name.size() >= 3 && f.name.substr(0,3) == "rep") r.push_back(f);
            }
            t1 = high_resolution_clock::now();
            long linPrefix = duration_cast<microseconds>(t1 - t0).count();

            double sp3 = triePrefix > 0 ? (double)linPrefix / triePrefix : 0.0;
            cout << "BENCH|Prefix(1000ops)|" << triePrefix << "|" << linPrefix
                 << "|" << fixed << setprecision(2) << sp3 << endl << flush;

            cout << "BENCH_DONE" << endl << flush;
        }

        // ── UNKNOWN ───────────────────────────────────────────────────
        else {
            cout << "ERROR|" << safe(cmd) << "|unknown command" << endl << flush;
        }
    }
}

// ═══════════════════════════════════
// MAIN FUNCTION
// ═══════════════════════════════════
int main(int argc, char* argv[]) {
    // Folder nodes in UnionFind setup
    uf.addNode(0);
    uf.addNode(1); uf.unite(1, 0);
    uf.addNode(2); uf.unite(2, 0);
    uf.addNode(3); uf.unite(3, 0);
    uf.addNode(4); uf.unite(4, 0);

    // Check for --machine flag
    bool machineMode = false;
    for (int i = 1; i < argc; i++) {
        if (string(argv[i]) == "--machine") {
            machineMode = true;
            break;
        }
    }

    if (machineMode) {
        // Suppress all DS couts in machine mode by redirecting stderr-only
        // (generateData / pds.saveVersion print to cout — handled per command)
        runMachineMode();
        return 0;
    }

    // ── Normal menu mode (unchanged behaviour) ───────────────────────
    printBanner();
    cout << "Welcome! Type 11 to generate test data first." << endl;
    
    int choice = -1;
    do {
        printMenu();
        if (!(cin >> choice)) {
            cin.clear();
            cin.ignore(1000, '\n');
            cout << "Invalid input. Enter a number." << endl;
            continue;
        }
        cin.ignore(1000, '\n'); // clear newline after valid integer input
        cout << endl; // space for readability
        
        switch(choice) {
            case 1:  addFile(); break;
            case 2:  deleteFile(); break;
            case 3:  searchPrefix(); break;
            case 4:  searchBySize(); break;
            case 5:  storageAnalytics(); break;
            case 6:  findDuplicates(); break;
            case 7:  showVersionHistory(); break;
            case 8:  editFile(); break;
            case 9:  rollbackFile(); break;
            case 10: detectOrphans(); break;
            case 11: generateData(10000); break;
            case 12: benchmark(); break;
            case 13: stressTest(); break;
            case 14: showSystemInfo(); break;
            case 0:  cout << "Goodbye!" << endl; break;
            default: cout << "Invalid option. Try again." << endl;
        }
    } while(choice != 0);

    return 0;
}
