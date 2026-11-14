#pragma once

/*
Define ENABLE_SERIAL_LOG to enable logging.

LOG_LEVEL sets the verbosity:
0 = ERR only
1 = ERR + WRN + INF
2 = ERR + WRN + INF + DBG

Output format: [millis] [LVL] [origin] message
*/

#ifndef LOG_LEVEL
#define LOG_LEVEL 0
#endif

void logPrintf(const char* level, const char* origin, const char* format, ...)
    __attribute__((format(printf, 3, 4)));

#ifdef ENABLE_SERIAL_LOG
#define LOG_ERR(origin, format, ...) logPrintf("[ERR]", origin, format "\n", ##__VA_ARGS__)

#if LOG_LEVEL >= 1
#define LOG_INF(origin, format, ...) logPrintf("[INF]", origin, format "\n", ##__VA_ARGS__)
#define LOG_WRN(origin, format, ...) logPrintf("[WRN]", origin, format "\n", ##__VA_ARGS__)
#else
#define LOG_INF(origin, format, ...)
#define LOG_WRN(origin, format, ...)
#endif

#if LOG_LEVEL >= 2
#define LOG_DBG(origin, format, ...) logPrintf("[DBG]", origin, format "\n", ##__VA_ARGS__)
#else
#define LOG_DBG(origin, format, ...)
#endif
#else
#define LOG_ERR(origin, format, ...)
#define LOG_WRN(origin, format, ...)
#define LOG_INF(origin, format, ...)
#define LOG_DBG(origin, format, ...)
#endif
