#include "AnalyticsEngine.h"
#include <iostream>
#include <iomanip>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <ctime>

using namespace std;

// Pipe-safe string encoding
static string safe(const string& s) {
    string r = s;
    for (size_t i = 0; i < r.length(); i++) if (r[i] == '|') r[i] = '\x01';
    return r;
}

AnalyticsEngine::AnalyticsEngine(FileSystemEngine& engine) : engine_(engine) {}

// ═══════════════════════════════════════════════════════════════════
// DUPLICATES
// ═══════════════════════════════════════════════════════════════════

void AnalyticsEngine::detectDuplicates() {
    vector<FileNode> all = engine_.getAllFiles();
    engine_.performDeepAnalysis(all);
    map<string, vector<FileNode>> groups;
    for (size_t i = 0; i < all.size(); i++) groups[all[i].checksum].push_back(all[i]);
    int count = 0; long long total = 0;
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        if (it->second.size() > 1) {
            count++; total += (long long)(it->second.size()-1) * it->second[0].size;
            string paths = "";
            for (size_t j = 0; j < it->second.size(); j++)
                paths += safe(it->second[j].path) + ":" + to_string(it->second[j].id) + ",";
            cout << "DUP|" << safe(it->second[0].name) << "|" << safe(it->first)
                 << "|" << paths << "|" << it->second[0].size/1024 << endl;
        }
    }
    cout << "DUP_DONE|" << count << "|" << total/1048576.0 << "|FAST" << endl << flush;
}

// ═══════════════════════════════════════════════════════════════════
// SUSPICIOUS
// ═══════════════════════════════════════════════════════════════════

void AnalyticsEngine::detectSuspicious() {
    vector<FileNode> all = engine_.getAllFiles();
    engine_.performDeepAnalysis(all);
    int count = 0;
    for (size_t i = 0; i < all.size(); i++) {
        if (all[i].entropy > 7.5) {
            cout << "SUSPICIOUS|" << safe(all[i].name) << "|" << safe(all[i].path)
                 << "|" << fixed << setprecision(2) << all[i].entropy
                 << "|" << all[i].id << endl;
            count++;
        }
    }
    cout << "SUSPICIOUS_DONE|" << count << endl << flush;
}

// ═══════════════════════════════════════════════════════════════════
// TYPE DISTRIBUTION
// ═══════════════════════════════════════════════════════════════════

void AnalyticsEngine::typeDistribution() {
    vector<FileNode> all = engine_.getAllFiles();
    engine_.performDeepAnalysis(all);
    map<string, long long> types;
    for (size_t i = 0; i < all.size(); i++) types[all[i].trueType] += all[i].size;

    struct Comp {
        bool operator()(const pair<string, long long>& a, const pair<string, long long>& b) {
            return a.second > b.second;
        }
    };
    vector<pair<string, long long>> sortedTypes(types.begin(), types.end());
    sort(sortedTypes.begin(), sortedTypes.end(), Comp());

    for (int i = 0; i < min((int)sortedTypes.size(), 6); i++) {
        cout << "TYPE|" << sortedTypes[i].first << "|" << sortedTypes[i].second << endl;
    }
    cout << "TYPE_DONE" << endl << flush;
}

// ═══════════════════════════════════════════════════════════════════
// WARNINGS
// ═══════════════════════════════════════════════════════════════════

void AnalyticsEngine::computeWarnings() {
    vector<FileNode> all = engine_.getAllFiles();
    map<string, vector<FileNode>> groups;
    for (size_t i = 0; i < all.size(); i++) groups[all[i].checksum].push_back(all[i]);
    long long dupeWaste = 0;
    for (auto it = groups.begin(); it != groups.end(); ++it)
        if (it->second.size() > 1) dupeWaste += (long long)(it->second.size() - 1) * it->second[0].size;
    if (dupeWaste > 500 * 1048576LL)
        cout << "WARN|DUPLICATES|Duplicate files wasting " << (dupeWaste / 1048576) << " MB|" << dupeWaste << endl;
    long long oldWaste = 0;
    long long twoYearsAgo = (long long)time(0) - (2LL * 365 * 86400);
    for (size_t i = 0; i < all.size(); i++)
        if ((long long)all[i].modifiedAt < twoYearsAgo) oldWaste += all[i].size;
    if (oldWaste > 1024 * 1048576LL)
        cout << "WARN|OLD_FILES|Files >2yrs occupy " << (oldWaste / 1048576) << " MB|" << oldWaste << endl;
    cout << "WARNINGS_DONE" << endl << flush;
}

// ═══════════════════════════════════════════════════════════════════
// RECLAIM
// ═══════════════════════════════════════════════════════════════════

void AnalyticsEngine::computeReclaim() {
    vector<FileNode> all = engine_.getAllFiles();
    long long oneYearAgo = (long long)time(0) - (365LL * 86400);
    long long totalRec = 0;
    map<string, vector<FileNode>> groups;
    for (size_t i = 0; i < all.size(); i++) groups[all[i].checksum].push_back(all[i]);
    long long dupeRec = 0;
    for (auto it = groups.begin(); it != groups.end(); ++it)
        if (it->second.size() > 1) dupeRec += (long long)(it->second.size() - 1) * it->second[0].size;
    cout << "REC|DUPLICATES|" << dupeRec << endl; totalRec += dupeRec;
    long long oldRec = 0;
    for (size_t i = 0; i < all.size(); i++)
        if ((long long)all[i].modifiedAt < oneYearAgo) oldRec += all[i].size;
    cout << "REC|OLD_FILES|" << oldRec << endl; totalRec += oldRec;
    for (size_t i = 0; i < all.size(); i++)
        if (all[i].size > 100 * 1048576LL)
            cout << "REC|LARGE_FILE|" << safe(all[i].name) << "|" << all[i].size << "|" << safe(all[i].path) << endl;
    cout << "RECLAIM_DONE|" << totalRec << endl << flush;
}

// ═══════════════════════════════════════════════════════════════════
// ORPHANS
// ═══════════════════════════════════════════════════════════════════

void AnalyticsEngine::findOrphans() {
    vector<int> ids = engine_.allFileIds();
    vector<int> orphans = engine_.uf().findAllOrphans(0, ids);
    vector<FileNode> all = engine_.getAllFiles();
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

void AnalyticsEngine::deleteOrphans() {
    vector<int> ids = engine_.allFileIds();
    vector<int> orphans = engine_.uf().findAllOrphans(0, ids);
    vector<FileNode> all = engine_.getAllFiles();
    int count = 0;
    for (size_t i = 0; i < orphans.size(); i++) {
        for (size_t j = 0; j < all.size(); j++) {
            if (all[j].id == orphans[i]) {
                if (remove(all[j].path.c_str()) == 0) {
                    engine_.bpt().remove(all[j].path);
                    engine_.trie().remove(all[j].name);
                    count++;
                }
                break;
            }
        }
    }
    cout << "ORPHAN_DONE|" << count << "|0" << endl << flush;
}

// ═══════════════════════════════════════════════════════════════════
// MONTHLY ANALYTICS
// ═══════════════════════════════════════════════════════════════════

void AnalyticsEngine::monthlyAnalytics() {
    // C-5: Use year-aware day offsets
    time_t now = time(0);
    struct tm tnow{};
    safeLocaltime(&now, &tnow);
    int yearOffset = tnow.tm_year - 120;
    if (yearOffset < 0) yearOffset = 0;
    if (yearOffset > 9) yearOffset = 9;
    int base = yearOffset * 365;
    const string months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    const int starts[] = {0,31,59,90,120,151,181,212,243,273,304,334};
    const int ends[] = {30,58,89,119,150,180,211,242,272,303,333,364};
    for (int i = 0; i < 12; i++) {
        cout << "MONTH|" << months[i] << "|" << fixed << setprecision(2)
             << engine_.segTree().queryRange(base + starts[i], base + ends[i])/1048576.0 << endl;
    }
    cout << "ANALYTICS_DONE|0" << endl << flush;
}
