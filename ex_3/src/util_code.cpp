#include <string>
#include <random>
#include <iostream>
#include <iomanip>

#include "../include/util_code.h"

#include <fstream>

double randomDouble(double minVal, double maxVal) {
    static std::random_device rd;
    static std::mt19937 gen(rd());

    std::uniform_real_distribution<double> dist(minVal, maxVal);

    return dist(gen);
};


void appendFormat(char* buffer, int maxLen, const char* fmt, ...) {
    if (buffer ==
        nullptr || maxLen <= 0)
        return;

    int len = static_cast<int>(strlen(buffer));
    if (len >= maxLen - 1)
        return;

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer + len, maxLen - len, fmt, args);
    va_end(args);

    buffer[maxLen - 1] = '\0';
}

void writeCsvText(const std::string& filename, const std::string& text) {
    std::ofstream fout(filename);

    if (!fout) throw std::runtime_error("Cannot open for writing: " + filename);
    fout << std::fixed << std::setprecision(6);
    fout << "# start,end,degree,a0,a1,a2,a3,a4\n";
    fout << text;
    // for (const auto& s : segs) {
    //     fout << s.start << "," << s.end << "," << s.degree;
    //     for (int j=0; j<COEFF_CAP; ++j) fout << "," << s.coeff[j];
    //     fout << "\n";
    // }
}

void vlogPrint(const char* color, const char* fmt, va_list args) {
    if (!fmt)
        return;
    if (!color)
        color = "";

    if (color[0] != 0 && fmt[0] != '#' && fmt[0] != '-' && fmt[0] != '|') {
        printf("<span style='color:%s;'>", color);
        vprintf(fmt, args);
        printf("</span>\n\n");
    }
    else {
        vprintf(fmt, args);
        printf("\n");
    }
}

void logTrace(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vlogPrint("darkgreen", fmt, args);
    va_end(args);
}

void logPrint(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vlogPrint("", fmt, args);
    va_end(args);
}

void logNote(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vlogPrint("blue", fmt, args);
    va_end(args);
}

void logWarn(const char* fmt, ...) {
    printf("\n\nWarning: ");

    va_list args;
    va_start(args, fmt);
    vlogPrint("orange", fmt, args);
    va_end(args);
}

void logError(const char* fmt, ...) {
    printf("\n\nERROR: ");

    va_list args;
    va_start(args, fmt);
    vlogPrint("red", fmt, args);
    va_end(args);
}
