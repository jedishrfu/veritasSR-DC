#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>
#include <string.h>
struct CPUTimer
{
  timeval beg, end;
  CPUTimer() {}
  ~CPUTimer() {}
  void start() { gettimeofday(&beg, NULL); }
  double elapsed()
  {
    gettimeofday(&end, NULL);
    return end.tv_sec - beg.tv_sec + (end.tv_usec - beg.tv_usec) / 1000000.0;
  }
};

inline void appendFormat(char* buffer, int maxLen, const char* fmt, ...)
{
  if (buffer == 0 || maxLen <= 0)
    return;

  int len = (int)strlen(buffer);
  if (len >= maxLen - 1)
    return;

  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer + len, maxLen - len, fmt, args);
  va_end(args);

  buffer[maxLen - 1] = '\0';
}

inline void vlogPrint(const char* color, const char* fmt, va_list args)
{
  if (!fmt)
    return;
  if (!color)
    color = "";

  if (color[0] != 0 && fmt[0] != '#' && fmt[0] != '-' && fmt[0] != '|')
  {
    printf("<span style='color:%s;'>", color);
    vprintf(fmt, args);
    printf("</span>\n\n");
  }
  else
  {
    vprintf(fmt, args);
    printf("\n");
  }
}

inline void logPrint(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vlogPrint("", fmt, args);
  va_end(args);
}

inline void logNote(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vlogPrint("blue", fmt, args);
  va_end(args);
}

inline void logWarning(const char* fmt, ...)
{
  printf("\n\nWarning: ");

  va_list args;
  va_start(args, fmt);
  vlogPrint("orange", fmt, args);
  va_end(args);
}

inline void logError(const char* fmt, ...)
{
  printf("\n\nERROR: ");

  va_list args;
  va_start(args, fmt);
  vlogPrint("red", fmt, args);
  va_end(args);
}