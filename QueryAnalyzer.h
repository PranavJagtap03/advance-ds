#pragma once
#include <string>
#include <algorithm>
#include <cctype>

using namespace std;

// ─────────────────────────────────────────
// QueryType enum — one value per query kind
// ─────────────────────────────────────────
enum QueryType {
    EXACT_NAME,     // e.g. "report.pdf"       → B+ Tree
    PREFIX_SEARCH,  // e.g. "bud*"             → Trie
    SIZE_RANGE,     // e.g. "size:1024-10240"  → Segment Tree
    DATE_RANGE,     // e.g. "date:jan-mar"     → Segment Tree
    DUPLICATE_FIND, // "duplicates"            → B+ Tree (checksum)
    ORPHAN_DETECT,  // "orphans"               → Union-Find
    UNKNOWN_QUERY   // unrecognised query
};

// ─────────────────────────────────────────
// QueryResult — returned by classify()
// ─────────────────────────────────────────
struct QueryResult {
    // Primary classification
    QueryType   type;

    // Short canonical DS name
    string      dsName;

    // One-sentence justification
    string      reason;

    // Parsed parameters — populated depending on type
    string      prefix;         // PREFIX_SEARCH: the prefix sans '*'
    long        minSize = -1;   // SIZE_RANGE: bytes
    long        maxSize = -1;   // SIZE_RANGE: bytes
    int         dayStart = -1;  // DATE_RANGE: 0-based day index (start of month)
    int         dayEnd   = -1;  // DATE_RANGE: 0-based day index (end of month)
    int         monthStart = -1;// DATE_RANGE: 1-based month number
    int         monthEnd   = -1;// DATE_RANGE: 1-based month number
};

// ─────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────
namespace QueryAnalyzerDetail {

    // Lowercase helper
    inline string toLower(string s) {
        transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return tolower(c); });
        return s;
    }

    // Month name → 1-based month number (0 = not found)
    inline int parseMonth(const string& m) {
        string lm = toLower(m);
        // Accept full names and 3-char abbreviations
        if (lm == "jan" || lm == "january")   return 1;
        if (lm == "feb" || lm == "february")  return 2;
        if (lm == "mar" || lm == "march")     return 3;
        if (lm == "apr" || lm == "april")     return 4;
        if (lm == "may")                       return 5;
        if (lm == "jun" || lm == "june")      return 6;
        if (lm == "jul" || lm == "july")      return 7;
        if (lm == "aug" || lm == "august")    return 8;
        if (lm == "sep" || lm == "september") return 9;
        if (lm == "oct" || lm == "october")   return 10;
        if (lm == "nov" || lm == "november")  return 11;
        if (lm == "dec" || lm == "december")  return 12;
        return 0;
    }

    // Month number → start/end day indices (0-based, non-leap year)
    // Returns false if monthNum is out of [1,12]
    inline bool monthToDays(int monthNum, int& dayStart, int& dayEnd) {
        // Start day (0-indexed) for each month
        static const int startDays[13] = {0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
        // End day (0-indexed, inclusive)
        static const int endDays[13]   = {0, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, 364};
        if (monthNum < 1 || monthNum > 12) return false;
        dayStart = startDays[monthNum];
        dayEnd   = endDays[monthNum];
        return true;
    }

} // namespace QueryAnalyzerDetail

// ─────────────────────────────────────────
// classify() — main public interface
// ─────────────────────────────────────────
inline QueryResult classify(const string& query) {
    using namespace QueryAnalyzerDetail;

    QueryResult res;
    res.type   = UNKNOWN_QUERY;
    res.dsName = "None";
    res.reason = "Query did not match any known pattern.";

    if (query.empty()) return res;

    string q = query; // working copy (preserve original case for names)

    // ── 1. Exact keywords ───────────────────────────────────────────
    {
        string lq = toLower(q);
        if (lq == "duplicates") {
            res.type   = DUPLICATE_FIND;
            res.dsName = "BPlusTree";
            res.reason = "B+ Tree is used to iterate all leaves and group files by checksum to detect duplicates.";
            return res;
        }
        if (lq == "orphans") {
            res.type   = ORPHAN_DETECT;
            res.dsName = "UnionFind";
            res.reason = "Union-Find efficiently identifies files not connected to the root directory component.";
            return res;
        }
    }

    // ── 2. SIZE_RANGE  — prefix "size:"  ────────────────────────────
    //    Format: size:<min_kb>-<max_kb>   (values in KB)
    {
        string lq = toLower(q);
        if (lq.substr(0, 5) == "size:") {
            string rest = q.substr(5); // preserve original for stol
            size_t dashPos = rest.find('-');
            if (dashPos != string::npos) {
                try {
                    long minKB = stol(rest.substr(0, dashPos));
                    long maxKB = stol(rest.substr(dashPos + 1));
                    res.type    = SIZE_RANGE;
                    res.dsName  = "SegmentTree";
                    res.minSize = minKB * 1024; // convert to bytes
                    res.maxSize = maxKB * 1024;
                    res.reason  = "Segment Tree supports O(log n) range queries over file sizes indexed by day; size filter can be applied on results.";
                    return res;
                } catch (...) {
                    // fall through to UNKNOWN
                }
            }
        }
    }

    // ── 3. DATE_RANGE  — prefix "date:" ─────────────────────────────
    //    Format: date:<month>  OR  date:<month1>-<month2>
    {
        string lq = toLower(q);
        if (lq.substr(0, 5) == "date:") {
            string rest = lq.substr(5);
            size_t dashPos = rest.find('-');
            int m1 = 0, m2 = 0;
            if (dashPos != string::npos) {
                m1 = parseMonth(rest.substr(0, dashPos));
                m2 = parseMonth(rest.substr(dashPos + 1));
            } else {
                m1 = parseMonth(rest);
                m2 = m1;
            }
            if (m1 > 0 && m2 > 0 && m1 <= m2) {
                int ds1, de1, ds2, de2;
                if (monthToDays(m1, ds1, de1) && monthToDays(m2, ds2, de2)) {
                    res.type       = DATE_RANGE;
                    res.dsName     = "SegmentTree";
                    res.dayStart   = ds1;
                    res.dayEnd     = de2;
                    res.monthStart = m1;
                    res.monthEnd   = m2;
                    res.reason     = "Segment Tree enables O(log n) range sum queries over day-indexed storage, perfect for monthly analytics.";
                    return res;
                }
            }
        }
    }

    // ── 4. PREFIX_SEARCH — query ends with '*' ────────────────────────
    if (!q.empty() && q.back() == '*') {
        res.type   = PREFIX_SEARCH;
        res.dsName = "Trie";
        res.prefix = q.substr(0, q.size() - 1); // strip trailing '*'
        res.reason = "Trie provides O(m) prefix lookups where m is the prefix length, far faster than linear scan.";
        return res;
    }

    // ── 5. EXACT_NAME — anything with an extension or no special chars ─
    //    Heuristic: contains a '.' or has no special tokens above
    {
        bool hasDot = (q.find('.') != string::npos);
        // If it contains a dot it looks like "report.pdf" → exact search
        // Otherwise treat it as prefix search (user omitted the '*')
        if (hasDot) {
            res.type   = EXACT_NAME;
            res.dsName = "BPlusTree";
            res.prefix = q;
            res.reason = "B+ Tree allows O(log n) exact key lookup using the filename as the sorted key.";
        } else {
            // No dot, no special prefix → treat as prefix search on plain token
            res.type   = PREFIX_SEARCH;
            res.dsName = "Trie";
            res.prefix = q;
            res.reason = "Trie provides O(m) prefix lookups; query treated as prefix because no extension or wildcard was found.";
        }
        return res;
    }
}
