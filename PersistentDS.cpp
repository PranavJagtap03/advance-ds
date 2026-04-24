#include "PersistentDS.h"
#include <iostream>
#include <iomanip>
using namespace std;

void PersistentDS::saveVersion(FileNode& f) {
    f.version = (int)history[f.id].size() + 1;
    history[f.id].push_back(f);
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
    if (old.id == -1) { cout << "Rollback failed.\n"; return false; }
    FileNode current = history[fileId].back();
    bpt.remove(current.name);
    bpt.insert(old.name, old);
    cout << "Successfully rolled back to version " << version << "\n";
    return true;
}
bool PersistentDS::hasHistory(int fileId) {
    return history.count(fileId) && !history[fileId].empty();
}
