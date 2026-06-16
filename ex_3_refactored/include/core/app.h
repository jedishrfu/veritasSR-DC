#include <string>
#include <vector>

#include "core/options.h"

#pragma once

/******************************************************************************
 *
 * VeritasSR
 * Symbolic Regression and Equation Discovery System
 *
 * File:
 *     app.h
 *
 * Purpose:
 *     Application-level initialization and command-line processing.
 *
 ******************************************************************************/

#include "core/options.h"

 /**
  * Display application banner, version information,
  * copyright notice, and build information.
  */
void printHeader();

/**
 * Parse command-line arguments.
 *
 * Parameters:
 *     argc
 *         Command-line argument count.
 *
 *     argv
 *         Command-line argument vector.
 *
 *     opts
 *         Options structure to populate.
 *
 * Returns:
 *     true
 *         Continue execution.
 *
 *     false
 *         Exit immediately (e.g. --help, --version).
 *
 * Errors:
 *     Invalid command-line arguments.
 *     Missing required parameter values.
 */
bool processArgs(
    int argc,
    char* argv[],
    Options& opts);

/**
 * Ensure the default variables exist.
 *
 * If no variables have been explicitly registered,
 * this function creates a default set such as:
 *
 *     x
 *     y
 *     z
 *     t
 *
 * Safe to call multiple times.
 */
void ensureDefaultVariables();

/**
 * Generate parser-visible variable names
 * from the currently registered variables.
 *
 * Returns:
 *     Vector of variable names suitable for
 *     parser initialization.
 */
std::vector<std::string> makeParserVariableNames();

/******************************************************************************
 *
 * VeritasSR
 * Symbolic Regression and Equation Discovery System
 *
 * File:
 *     app.h
 *
 * Purpose:
 *     Application-level initialization and command-line processing.
 *
 ******************************************************************************/

#include "core/options.h"

 /**
  * Display application banner, version information,
  * copyright notice, and build information.
  */
void printHeader();

/**
 * Parse command-line arguments.
 *
 * Parameters:
 *     argc
 *         Command-line argument count.
 *
 *     argv
 *         Command-line argument vector.
 *
 *     opts
 *         Options structure to populate.
 *
 * Returns:
 *     true
 *         Continue execution.
 *
 *     false
 *         Exit immediately (e.g. --help, --version).
 *
 * Errors:
 *     Invalid command-line arguments.
 *     Missing required parameter values.
 */
bool processArgs(
    int argc,
    char* argv[],
    Options& opts);

/**
 * Ensure the default variables exist.
 *
 * If no variables have been explicitly registered,
 * this function creates a default set such as:
 *
 *     x
 *     y
 *     z
 *     t
 *
 * Safe to call multiple times.
 */
void ensureDefaultVariables();

/**
 * Generate parser-visible variable names
 * from the currently registered variables.
 *
 * Returns:
 *     Vector of variable names suitable for
 *     parser initialization.
 */
std::vector<std::string> makeParserVariableNames();