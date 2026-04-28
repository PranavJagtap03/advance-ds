#include "UnionFind.h"
#include <iostream>
using namespace std;

void UnionFind::initDense(int minId, int maxId) {
    idOffset = minId;
    idMax = maxId;
    int sz = maxId - minId + 1;
    parent_vec.resize(sz);
    rank_vec.resize(sz, 0);
    for (int i = 0; i < sz; i++) parent_vec[i] = i + minId;
    // Migrate any existing map entries into the dense vectors
    for (int id = minId; id <= maxId; id++) {
        if (parent.count(id)) {
            parent_vec[id - idOffset] = parent[id];
            rank_vec[id - idOffset] = rankMap[id];
            parent.erase(id);
            rankMap.erase(id);
        }
    }
}

void UnionFind::addNode(int id) {
    if (inDenseRange(id)) {
        // Already initialized via initDense
        return;
    }
    if (parent.count(id)) return;
    parent[id] = id; rankMap[id] = 0;
}

int UnionFind::find(int x) {
    if (inDenseRange(x)) {
        int idx = x - idOffset;
        if (parent_vec[idx] != x) parent_vec[idx] = find(parent_vec[idx]);
        return parent_vec[idx];
    }
    if (!parent.count(x)) return -1;
    if (parent[x] != x) parent[x] = find(parent[x]);
    return parent[x];
}

void UnionFind::unite(int childId, int parentId) {
    // Ensure both nodes exist
    if (!inDenseRange(childId) && !parent.count(childId)) addNode(childId);
    if (!inDenseRange(parentId) && !parent.count(parentId)) addNode(parentId);
    int pc = find(childId), pp = find(parentId);
    if (pc == -1 || pp == -1 || pc == pp) return;

    int rcP, rcC;
    if (inDenseRange(pc)) rcC = rank_vec[pc - idOffset];
    else rcC = rankMap[pc];
    if (inDenseRange(pp)) rcP = rank_vec[pp - idOffset];
    else rcP = rankMap[pp];

    auto setParent = [&](int child, int par) {
        if (inDenseRange(child)) parent_vec[child - idOffset] = par;
        else parent[child] = par;
    };
    auto incRank = [&](int id) {
        if (inDenseRange(id)) rank_vec[id - idOffset]++;
        else rankMap[id]++;
    };

    if (rcC < rcP) setParent(pc, pp);
    else if (rcC > rcP) setParent(pp, pc);
    else { setParent(pp, pc); incRank(pc); }
}

bool UnionFind::isConnected(int a, int b) {
    int fa = find(a), fb = find(b);
    if (fa == -1 || fb == -1) return false;
    return fa == fb;
}

vector<int> UnionFind::findAllOrphans(int rootId, vector<int>& allIds) {
    vector<int> orphans;
    if (!parent.count(rootId) && !inDenseRange(rootId)) return orphans;
    int rootRep = find(rootId);
    for (int id : allIds) {
        int rep = find(id);
        if (rep != -1 && rep != rootRep) orphans.push_back(id);
    }
    return orphans;
}
