#pragma once

/******************************************************************************
 *
 * VeritasSR
 * Symbolic Regression and Equation Discovery System
 *
 * File:
 *     generate_basic.h
 *
 * Purpose:
 *     Public interface for generating the initial expression population.
 *
 ******************************************************************************/

#include "ast_nodes.h"

 /**
  * Generate the initial pool of symbolic expressions.
  *
  * The generated expressions are used as the starting population
  * for later evolutionary search.
  *
  * Returns:
  *     Newly allocated ExprArray containing initial expressions.
  *
  * Ownership:
  *     Caller owns the returned ExprArray and is responsible
  *     for deleting it.
  *
  * Errors:
  *     May return nullptr if allocation fails or generation fails.
  */
ExprArray* generateBasicExpressions();