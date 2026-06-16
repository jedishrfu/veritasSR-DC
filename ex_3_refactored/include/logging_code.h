/*
 * VeritasSR
 * Symbolic Regression and Equation Discovery System
 *
 * Copyright (c) 2026 James M. McArdle
 * All Rights Reserved.
 *
 * File: logging_code.h
 */

#ifndef LOGGING_CODE_H
#define LOGGING_CODE_H

#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>

struct CPUTimer
{
  timeval beg, end;

  CPUTimer();
  ~CPUTimer();

  void start();
  double elapsed();
};

void appendFormat(char* buffer, int maxLen, const char* fmt, ...);

void vlogPrint(const char* color, const char* fmt, va_list args);

void logPrint(const char* fmt, ...);
void logNote(const char* fmt, ...);
void logWarning(const char* fmt, ...);
void logError(const char* fmt, ...);

#endif