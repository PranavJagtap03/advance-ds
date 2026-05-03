#pragma once
#include <vector>
#include <iostream>
#include <algorithm>

using namespace std;

class SegmentTree {
private:
    int n;
    vector<long> tree;
    vector<long> maxTree;

    // Recursive helper for updates
    void updateHelper(int node, int start, int end, int idx, long val);
    
    // Recursive helper for range sum queries
    long queryHelper(int node, int start, int end, int l, int r);

    // Recursive helper for range max queries
    long queryMaxHelper(int node, int start, int end, int l, int r);

public:
    // Constructor to initialize trees
    SegmentTree(int size);
    
    // Bounds checked add size for a specific day
    void addFile(int dayIndex, long fileSize);
    
    // Bounds checked remove size for a specific day
    void removeFile(int dayIndex, long fileSize);
    
    // Range query yielding sum within startDay and endDay
    long queryRange(int startDay, int endDay);
    
    // Range query yielding max on a day within startDay and endDay — O(log n)
    long queryMax(int startDay, int endDay);
};
