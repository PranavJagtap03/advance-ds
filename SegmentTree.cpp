#include "SegmentTree.h"
#include <iostream>
#include <algorithm>
using namespace std;

SegmentTree::SegmentTree(int size) : n(size), tree(4*size, 0), maxTree(4*size, 0) {}

void SegmentTree::updateHelper(int node, int start, int end, int idx, long val) {
    if (start == end) {
        tree[node] += val;
        if (tree[node] > maxTree[node]) maxTree[node] = tree[node];
        return;
    }
    int mid = (start+end)/2;
    if (idx <= mid) updateHelper(2*node, start, mid, idx, val);
    else            updateHelper(2*node+1, mid+1, end, idx, val);
    tree[node] = tree[2*node] + tree[2*node+1];
    maxTree[node] = max(maxTree[2*node], maxTree[2*node+1]);
}

long SegmentTree::queryHelper(int node, int start, int end, int l, int r) {
    if (r < start || end < l) return 0;
    if (l <= start && end <= r) return tree[node];
    int mid = (start+end)/2;
    return queryHelper(2*node, start, mid, l, r)
         + queryHelper(2*node+1, mid+1, end, l, r);
}

long SegmentTree::queryMaxHelper(int node, int start, int end, int l, int r) {
    if (r < start || end < l) return 0;
    if (l <= start && end <= r) return maxTree[node];
    int mid = (start+end)/2;
    return max(queryMaxHelper(2*node, start, mid, l, r),
               queryMaxHelper(2*node+1, mid+1, end, l, r));
}

void SegmentTree::addFile(int dayIndex, long fileSize) {
    if (dayIndex < 0 || dayIndex >= n) return;
    updateHelper(1, 0, n-1, dayIndex, fileSize);
}

void SegmentTree::removeFile(int dayIndex, long fileSize) {
    if (dayIndex < 0 || dayIndex >= n) return;
    updateHelper(1, 0, n-1, dayIndex, -fileSize);
}

long SegmentTree::queryRange(int startDay, int endDay) {
    if (startDay < 0) startDay = 0;
    if (endDay >= n) endDay = n-1;
    if (startDay > endDay) return 0;
    return queryHelper(1, 0, n-1, startDay, endDay);
}

long SegmentTree::queryMax(int startDay, int endDay) {
    if (startDay < 0) startDay = 0;
    if (endDay >= n) endDay = n-1;
    if (startDay > endDay) return 0;
    return queryMaxHelper(1, 0, n-1, startDay, endDay);
}
