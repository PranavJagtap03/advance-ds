#include "SnapshotManager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>

using namespace std;

void SnapshotManager::load() {
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
        snapshots_.push_back(s);
        if (s.id >= nextId_) nextId_ = s.id + 1;
    }
}

void SnapshotManager::save() {
    ofstream ofs("snapshots.dat", ios::trunc);
    for (size_t i = 0; i < snapshots_.size(); i++) {
        const auto& s = snapshots_[i];
        ofs << "SNAPSHOT_V1" << "\n";
        ofs << s.id << " " << s.timestamp << " " << s.fileCount << " " << s.totalBytes << "\n";
        for (auto it = s.fileMap.begin(); it != s.fileMap.end(); ++it) {
            ofs << it->first << " " << it->second << "\n";
        }
        ofs << "END" << "\n";
    }
}

void SnapshotManager::createSnapshot(const vector<FileNode>& files) {
    Snapshot s;
    s.id = nextId_++;
    s.timestamp = (long long)time(0);
    s.fileCount = (int)files.size();
    s.totalBytes = 0;
    for (size_t i = 0; i < files.size(); i++) {
        s.totalBytes += files[i].size;
        s.fileMap[files[i].path] = files[i].size;
    }
    snapshots_.push_back(s);
    save();
    cout << "SNAPSHOT_OK|" << s.id << "|" << s.fileCount << "|" << s.totalBytes << endl << flush;
}

void SnapshotManager::diff(int id1, int id2) {
    Snapshot* s1 = nullptr;
    Snapshot* s2 = nullptr;
    for (size_t i = 0; i < snapshots_.size(); i++) {
        if (snapshots_[i].id == id1) s1 = &snapshots_[i];
        if (snapshots_[i].id == id2) s2 = &snapshots_[i];
    }
    if (!s1 || !s2) {
        cout << "ERROR|DIFF|not_found" << endl << flush;
        return;
    }

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
