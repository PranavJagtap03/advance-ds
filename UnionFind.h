#pragma once
#include <unordered_map>
#include <vector>
#include <iostream>

using namespace std;

class UnionFind {
private:
    unordered_map<int, int> parent;
    unordered_map<int, int> rankMap;

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
};
