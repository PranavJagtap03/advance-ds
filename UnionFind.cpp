#include "UnionFind.h"
#include <iostream>
using namespace std;

void UnionFind::addNode(int id) {
    if (parent.count(id)) return;
    parent[id] = id; rankMap[id] = 0;
}
int UnionFind::find(int x) {
    if (!parent.count(x)) { cout << "Node not found: " << x << endl; return -1; }
    if (parent[x] != x) parent[x] = find(parent[x]);
    return parent[x];
}
void UnionFind::unite(int childId, int parentId) {
    if (!parent.count(childId)) addNode(childId);
    if (!parent.count(parentId)) addNode(parentId);
    int pc = find(childId), pp = find(parentId);
    if (pc == -1 || pp == -1 || pc == pp) return;
    if (rankMap[pc] < rankMap[pp]) parent[pc] = pp;
    else if (rankMap[pc] > rankMap[pp]) parent[pp] = pc;
    else { parent[pp] = pc; rankMap[pc]++; }
}
bool UnionFind::isConnected(int a, int b) {
    int fa = find(a), fb = find(b);
    if (fa == -1 || fb == -1) return false;
    return fa == fb;
}
vector<int> UnionFind::findAllOrphans(int rootId, vector<int>& allIds) {
    vector<int> orphans;
    if (!parent.count(rootId)) return orphans;
    int rootRep = find(rootId);
    for (int id : allIds) {
        int rep = find(id);
        if (rep != -1 && rep != rootRep) orphans.push_back(id);
    }
    return orphans;
}
