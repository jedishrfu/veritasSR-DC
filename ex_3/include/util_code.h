#pragma once

double randomDouble(double minVal, double maxVal);
double randomDouble(double minVal, double maxVal, double defaultVal);

struct CPUTimer;

void logTrace(const char* fmt, ...);
void logPrint(const char* fmt, ...);
void logNote(const char* fmt, ...);
void logWarn(const char* fmt, ...);
void logError(const char* fmt, ...);


