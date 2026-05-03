#include "CommandProcessor.h"
#include "Utils.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <unordered_map>
#include <regex>

using namespace std;

// ═══════════════════════════════════════════════════════════════════
// PIPE-SAFE ENCODING
// ═══════════════════════════════════════════════════════════════════

static string safe(const string& s) {
    string r = s;
    for (size_t i = 0; i < r.length(); i++) if (r[i] == '|') r[i] = '\x01';
    return r;
}

// ═══════════════════════════════════════════════════════════════════
// CONSTRUCTION
// ═══════════════════════════════════════════════════════════════════

CommandProcessor::CommandProcessor(FileSystemEngine& engine, SnapshotManager& snapshots)
    : engine_(engine), snapshots_(snapshots), analytics_(engine) {}

// ═══════════════════════════════════════════════════════════════════
// MAIN COMMAND LOOP
// ═══════════════════════════════════════════════════════════════════

void CommandProcessor::run() {
    string line;
    while (getline(cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        istringstream iss(line);
        string cmd; iss >> cmd;

        if (cmd == "EXIT") { cout << "BYE" << endl << flush; return; }
        else if (cmd == "LOAD_BATCH_BEGIN") { handleLoadBatch(); }
        else if (cmd == "DISK_INFO") { engine_.sendDiskInfo(); }
        else if (cmd == "SNAPSHOT") { handleSnapshot(); }
        else if (cmd == "DIFF") { handleDiff(iss); }
        else if (cmd == "WARNINGS") { analytics_.computeWarnings(); }
        else if (cmd == "RECLAIM") { analytics_.computeReclaim(); }
        else if (cmd == "DELETE") { handleDelete(iss); }
        else if (cmd == "DELETE_ORPHANS") { analytics_.deleteOrphans(); }
        else if (cmd == "DUMP_CACHE") { handleDumpCache(); }
        else if (cmd == "TYPE_DISTRIBUTION") { analytics_.typeDistribution(); }
        else if (cmd == "SUSPICIOUS") { analytics_.detectSuspicious(); }
        else if (cmd == "LOAD_DIR") { handleLoadDir(iss); }
        else if (cmd == "SCAN_PARALLEL") { handleScanParallel(iss); }
        else if (cmd == "SCAN_INCREMENTAL") { handleScanIncremental(iss); }
        else if (cmd == "ROLLBACK") { handleRollback(iss); }
        else if (cmd == "SEARCH") { handleSearch(iss); }
        else if (cmd == "REGEX") { handleRegex(iss); }
        else if (cmd == "PREFIX") { handlePrefix(iss); }
        else if (cmd == "DUPLICATES") { analytics_.detectDuplicates(); }
        else if (cmd == "ORPHANS") { analytics_.findOrphans(); }
        else if (cmd == "ANALYTICS") { analytics_.monthlyAnalytics(); }
        else if (cmd == "VERSION_HISTORY") { handleVersionHistory(iss); }
    }
}

// ═══════════════════════════════════════════════════════════════════
// COMMAND HANDLERS
// ═══════════════════════════════════════════════════════════════════

void CommandProcessor::handleLoadDir(istringstream& args) {
    string path; getline(args >> ws, path);
    if (path.empty()) { cout << "ERROR|LOAD_DIR|no path" << endl << flush; return; }
    pair<int, long> res = engine_.loadDirectory(path);
    engine_.initDenseRange();
    cout << "LOADED|" << res.first << "|" << res.second << endl << flush;
}

void CommandProcessor::handleScanParallel(istringstream& args) {
    string path; getline(args >> ws, path);
    if (path.empty()) { cout << "ERROR|SCAN_PARALLEL|no path" << endl << flush; return; }
    pair<int, long> res = engine_.scanDirectoryParallel(path, 8);
    // SCAN_DONE is emitted by scanDirectoryParallel
    // Also emit LOADED for backward compatibility
    cout << "LOADED|" << res.first << "|" << res.second << endl << flush;
}

void CommandProcessor::handleScanIncremental(istringstream& args) {
    string path;
    long lastScanTime = 0;
    args >> ws;
    // Parse: SCAN_INCREMENTAL <path> <lastScanTime>
    string token;
    if (getline(args, token)) {
        // Find last space to separate path from timestamp
        size_t lastSpace = token.rfind(' ');
        if (lastSpace != string::npos) {
            path = token.substr(0, lastSpace);
            try {
                lastScanTime = stol(token.substr(lastSpace + 1));
            } catch (...) {
                // If parsing fails, treat entire token as path
                path = token;
                lastScanTime = 0;
            }
        } else {
            path = token;
        }
    }
    if (path.empty()) { cout << "ERROR|SCAN_INCREMENTAL|no path" << endl << flush; return; }
    pair<int, long> res = engine_.scanDirectoryIncremental(path, lastScanTime, 8);
    // SCAN_DONE is emitted by scanDirectoryIncremental
    cout << "LOADED|" << res.first << "|" << res.second << endl << flush;
}

void CommandProcessor::handleLoadBatch() {
    int count = 0;
    string batchLine;
    while (getline(cin, batchLine)) {
        if (!batchLine.empty() && batchLine.back() == '\r') batchLine.pop_back();
        if (batchLine == "LOAD_BATCH_END") break;
        istringstream biss(batchLine);
        string fname, pdir, sz_str, mt_str, ttype, ent_str;
        if (getline(biss, fname, '|') && getline(biss, pdir, '|') &&
            getline(biss, sz_str, '|') && getline(biss, mt_str, '|') &&
            getline(biss, ttype, '|') && getline(biss, ent_str)) {
            try {
                long sz = stol(sz_str);
                long mt = stol(mt_str);
                double ent = stod(ent_str);
                int parentId = engine_.getOrCreateDirId(pdir);
                string fullPath = pdir + "/" + fname;
                int id = engine_.addFile(fullPath, fname, parentId, sz, mt, false, ttype, ent);
                engine_.pds().saveVersion(
                    *engine_.search(fullPath)
                );
                count++;
            } catch (...) {}
        }
    }
    cout << "BATCH_DONE|" << count << endl << flush;
}

void CommandProcessor::handleSearch(istringstream& args) {
    string query; getline(args >> ws, query);
    string filename = query;
    long minSize = -1;
    size_t pos = query.find(" --size-gt ");
    if (pos != string::npos) {
        filename = query.substr(0, pos);
        try { minSize = stol(query.substr(pos + 11)); } catch(...) {}
    }

    FileNode* node = engine_.search(filename);
    if (node) {
        if (minSize == -1 || node->size > minSize) {
            cout << "RESULT|FOUND|" << safe(node->name) << "|" << safe(node->path) << "|"
                 << node->size / 1024 << "|Engine|" << safe(node->path) << "|0|Direct" << endl << flush;
        } else {
            cout << "RESULT|NOT_FOUND|" << safe(filename) << " (Filtered)|| |Engine||0|Direct" << endl << flush;
        }
    } else {
        cout << "RESULT|NOT_FOUND|" << safe(filename) << "|||Engine||0|Direct" << endl << flush;
    }
}

void CommandProcessor::handlePrefix(istringstream& args) {
    string prefix; getline(args >> ws, prefix);
    vector<string> names = engine_.prefixSearch(prefix);
    if (!names.empty()) {
        // H-5: BPT is path-keyed after C-1, so build a name lookup from all leaves
        vector<FileNode> all = engine_.getAllFiles();
        unordered_map<string, const FileNode*> nameToNode;
        for (size_t i = 0; i < all.size(); i++) nameToNode[all[i].name] = &all[i];
        for (size_t i = 0; i < names.size(); i++) {
            auto it = nameToNode.find(names[i]);
            if (it != nameToNode.end()) {
                const FileNode* fn = it->second;
                cout << "RESULT|FOUND|" << safe(fn->name) << "|" << safe(fn->path) << "|"
                     << fn->size / 1024 << "|Engine|" << safe(fn->path) << "|0|Prefix" << endl;
            }
        }
    } else {
        cout << "RESULT|NOT_FOUND|" << safe(prefix) << "|||Engine||0|Prefix" << endl << flush;
    }
}

void CommandProcessor::handleRegex(istringstream& args) {
    string pattern; getline(args >> ws, pattern);
    try {
        regex re(pattern, regex_constants::icase);
        vector<FileNode> all = engine_.getAllFiles();
        int count = 0;
        for (size_t i = 0; i < all.size(); i++) {
            if (regex_search(all[i].name, re)) {
                cout << "RESULT|FOUND|" << safe(all[i].name) << "|" << safe(all[i].path) << "|"
                     << all[i].size / 1024 << "|Engine|" << safe(all[i].path) << "|0|Regex" << endl;
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

void CommandProcessor::handleDelete(istringstream& args) {
    int fid = -1; args >> fid;
    vector<FileNode> all = engine_.getAllFiles();
    bool found = false;
    for (size_t i = 0; i < all.size(); i++) {
        if (all[i].id == fid) {
            engine_.pds().saveVersion(all[i]);
            engine_.pds().backupFile(all[i]);
            if (remove(all[i].path.c_str()) == 0) {
                engine_.bpt().remove(all[i].path);
                engine_.trie().remove(all[i].name);
                cout << "DELETE_OK|" << fid << endl;
            } else {
                cout << "ERROR|DELETE|failed" << endl;
            }
            found = true; break;
        }
    }
    if (!found) cout << "ERROR|DELETE|not_found" << endl;
    cout << flush;
}

void CommandProcessor::handleRollback(istringstream& args) {
    int fid, ver;
    if (!(args >> fid >> ver)) {
        cout << "ROLLBACK_ERROR|args" << endl << flush;
        return;
    }
    engine_.pds().rollback(fid, ver, engine_.bpt());
    cout << flush;
}

void CommandProcessor::handleVersionHistory(istringstream& args) {
    int fid;
    if (!(args >> fid)) {
        cout << "VERSION_DONE|0" << endl << flush;
        return;
    }
    for (int v = 1; ; v++) {
        FileNode fn = engine_.pds().getVersion(fid, v);
        if (fn.id == -1) break;
        cout << "RESULT|VERSION|" << fid << "|" << v << "|" << safe(fn.name) << "|"
             << safe(fn.path) << "|" << fn.parentId << "|" << fn.size << "|" << fn.modifiedAt << endl;
    }
    cout << "VERSION_DONE|0" << endl << flush;
}

void CommandProcessor::handleSnapshot() {
    vector<FileNode> all = engine_.getAllFiles();
    snapshots_.createSnapshot(all);
}

void CommandProcessor::handleDiff(istringstream& args) {
    int id1, id2;
    if (!(args >> id1 >> id2)) {
        cout << "ERROR|DIFF|args" << endl << flush;
        return;
    }
    snapshots_.diff(id1, id2);
}

void CommandProcessor::handleDumpCache() {
    vector<FileNode> all = engine_.getAllFiles();
    for (size_t i = 0; i < all.size(); i++) {
        cout << "CACHE_ITEM|" << safe(all[i].name) << "|" << safe(all[i].path) << "|"
             << all[i].size << "|" << all[i].modifiedAt << "|" << all[i].trueType << "|"
             << fixed << setprecision(4) << all[i].entropy << endl;
    }
    cout << "CACHE_DONE" << endl << flush;
}
