#pragma once
#include <unordered_map>
#include <vector>
#include <iostream>

using namespace std;

class UnionFind {
private:
    unordered_map<int, int> parent;
    unordered_map<int, int> rankMap;

    // Dense vector storage for sequential file IDs
    int idOffset = 0;
    int idMax = -1;
    vector<int> parent_vec;
    vector<int> rank_vec;

    bool inDenseRange(int id) const {
        return idMax >= 0 && id >= idOffset && id <= idMax;
    }

public:
    // Adds a new node, setting parent to itself and rank to 0
    void addNode(int id);
    
    // Finds the representative parent of a set using path compression
    int find(int x);
    
    // Unites two sets by rank
    void unite(int childId, int parentId);
    
    // Checks if two nodes are in the same set
    bool isConnected(int a, int b);
    
    // Finds all nodes that are not connected to the rootId
    vector<int> findAllOrphans(int rootId, vector<int>& allIds);

    // Initialize dense vector storage for sequential IDs [minId, maxId]
    void initDense(int minId, int maxId);
};
