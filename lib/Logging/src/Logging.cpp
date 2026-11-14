#include "Logging.h"

#include <Arduino.h>

void logPrintf(const char* level, const char* origin, const char* format, ...) {
  char buf[256];
  int len = snprintf(buf, sizeof(buf), "[%lu] %s [%s] ", millis(), level, origin);
  if (len < 0 || len >= static_cast<int>(sizeof(buf))) return;

  va_list args;
  va_start(args, format);
  vsnprintf(buf + len, sizeof(buf) - len, format, args);
  va_end(args);
  Serial.print(buf);
}
