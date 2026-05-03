#include "PersistentDS.h"
#include <iostream>
#include <iomanip>
#include <fstream>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/stat.h>
    #include <sys/types.h>
#endif

using namespace std;

PersistentDS::PersistentDS() {
    ensureBackupDir();
}

void PersistentDS::ensureBackupDir() {
#ifdef _WIN32
    CreateDirectoryA(backupDir.c_str(), NULL);
#else
    mkdir(backupDir.c_str(), 0777);
#endif
}

bool PersistentDS::copyFile(const string& source, const string& destination) {
    ifstream src(source, ios::binary);
    if (!src) return false;
    ofstream dst(destination, ios::binary);
    if (!dst) return false;
    dst << src.rdbuf();
    return true;
}

void PersistentDS::saveVersion(FileNode& f) {
    f.version = (int)history[f.id].size() + 1;
    history[f.id].push_back(f);
    // C-6: Automatically create physical backup if path is non-empty
    if (!f.path.empty()) {
        try { backupFile(f); } catch (...) { /* silently skip if file doesn't exist */ }
    }
}

FileNode PersistentDS::getVersion(int fileId, int version) {
    if (!history.count(fileId)) { cout << "File not found\n"; return FileNode(); }
    if (version < 1 || version > (int)history[fileId].size()) { cout << "Version not found\n"; return FileNode(); }
    return history[fileId][version-1];
}

void PersistentDS::showHistory(int fileId) {
    if (!history.count(fileId) || history[fileId].empty()) { cout << "No history for ID: " << fileId << endl; return; }
    cout << left << setw(5) << "Ver" << " | " << setw(25) << "Name" << " | " << setw(10) << "Size(KB)" << " | Modified\n";
    cout << string(60,'-') << "\n";
    for (auto& v : history[fileId])
        cout << left << setw(5) << v.version << " | " << setw(25) << v.name << " | " << setw(10) << v.size/1024 << " | " << v.modifiedAt << "\n";
}

bool PersistentDS::rollback(int fileId, int version, BPlusTree& bpt) {
    FileNode old = getVersion(fileId, version);
    if (old.id == -1) { cout << "ROLLBACK_ERROR|failed" << endl; return false; }
    
    bool physicalOk = false;
    // C-6: Check if physical backup exists before attempting restore
    if (physicalBackupAvailable(fileId, version)) {
        physicalOk = restoreFile(old, version);
    }

    FileNode current = history[fileId].back();
    // R-2: BPT is path-keyed after C-1 — use path, not name
    if (!current.path.empty()) {
        bpt.remove(current.path);
    }
    if (!old.path.empty()) {
        bpt.insert(old.path, old);
    }
    
    // FIX: Append the restored version as a NEW version entry in history
    // so that history stays consistent and a second rollback works correctly
    FileNode restored = old;
    restored.version = (int)history[fileId].size() + 1;
    history[fileId].push_back(restored);
    
    if (physicalOk) {
        cout << "ROLLBACK_OK|" << fileId << "|" << version << endl;
    } else {
        cout << "ROLLBACK_OK|" << fileId << "|" << version << "|METADATA_ONLY" << endl;
    }
    return true;
}

bool PersistentDS::hasHistory(int fileId) {
    return history.count(fileId) && !history[fileId].empty();
}

bool PersistentDS::backupFile(const FileNode& f) {
    ensureBackupDir();
    string dest = backupDir + "/" + to_string(f.id) + "_v" + to_string(f.version);
    return copyFile(f.path, dest);
}

bool PersistentDS::restoreFile(const FileNode& f, int version) {
    string src = backupDir + "/" + to_string(f.id) + "_v" + to_string(version);
    return copyFile(src, f.path);
}

bool PersistentDS::physicalBackupAvailable(int fileId, int version) {
    string path = backupDir + "/" + to_string(fileId) + "_v" + to_string(version);
    ifstream test(path);
    return test.good();
}
