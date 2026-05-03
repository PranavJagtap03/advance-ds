#pragma once
#include <ctime>

// R-1/O-1: Centralized thread-safe localtime wrapper using proper time_t type
inline bool safeLocaltime(const time_t* ts, struct tm* out) {
#if defined(_MSC_VER)
    return localtime_s(out, ts) == 0;
#elif defined(_WIN32)
    // MinGW: no localtime_r, but single-threaded engine is safe
    struct tm* tmp = localtime(ts);
    if (!tmp) return false;
    *out = *tmp;
    return true;
#else
    return localtime_r(ts, out) != nullptr;
#endif
}

// N-2: Clamp leap-year day 365 to 364 to stay within 3650-slot design
inline int computeDayIndex(time_t ts) {
    struct tm ti{};
    if (!safeLocaltime(&ts, &ti)) return 0;
    int yearOff = ti.tm_year - 120; // years since 2020
    if (yearOff < 0) yearOff = 0;
    if (yearOff > 9) yearOff = 9;
    int yday = (ti.tm_yday > 364) ? 364 : ti.tm_yday;
    return yearOff * 365 + yday;
}
