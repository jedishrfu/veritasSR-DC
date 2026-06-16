/*
 * VeritasSR
 * Symbolic Regression and Equation Discovery System
 *
 * Copyright (c) 2026 James M. McArdle
 * All Rights Reserved.
 *
 * File: logging_code.cpp
 */

#include "logging_code.h"

#include <string.h>

CPUTimer::CPUTimer()
{
}

CPUTimer::~CPUTimer()
{
}

void CPUTimer::start()
{
    gettimeofday(&beg, NULL);
}

double CPUTimer::elapsed()
{
    gettimeofday(&end, NULL);

    return end.tv_sec - beg.tv_sec +
        (end.tv_usec - beg.tv_usec) / 1000000.0;
}

void appendFormat(char* buffer, int maxLen, const char* fmt, ...)
{
    if (buffer == nullptr || maxLen <= 0)
        return;

    int len = (int)strlen(buffer);

    if (len >= maxLen - 1)
        return;

    va_list args;
    va_start(args, fmt);

    vsnprintf(buffer + len,
        maxLen - len,
        fmt,
        args);

    va_end(args);

    buffer[maxLen - 1] = '\0';
}

void vlogPrint(const char* color,
    const char* fmt,
    va_list args)
{
    if (!fmt)
        return;

    if (!color)
        color = "";

    if (color[0] != 0 &&
        fmt[0] != '#' &&
        fmt[0] != '-' &&
        fmt[0] != '|')
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

void logPrint(const char* fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vlogPrint("", fmt, args);
    va_end(args);
}

void logNote(const char* fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vlogPrint("blue", fmt, args);
    va_end(args);
}

void logWarning(const char* fmt, ...)
{
    printf("\n\nWarning: ");

    va_list args;

    va_start(args, fmt);
    vlogPrint("orange", fmt, args);
    va_end(args);
}

void logError(const char* fmt, ...)
{
    printf("\n\nERROR: ");

    va_list args;

    va_start(args, fmt);
    vlogPrint("red", fmt, args);
    va_end(args);
}