#pragma once

/******************************************************************************
 *
 * VeritasSR
 * Symbolic Regression and Equation Discovery System
 *
 * File:
 *     filter.h
 *
 * Purpose:
 *     Public interface for filtering expression pools by score.
 *
 ******************************************************************************/

#include "ast_nodes.h"

 /**
  * Filter an expression pool using a cutoff score.
  *
  * Expressions with scores better than or equal to the cutoff
  * are copied into a new expression pool.
  *
  * Parameters:
  *     input
  *         Source expression pool.
  *
  *     cutoffScore
  *         Maximum allowed score.
  *
  * Returns:
  *     Newly allocated filtered ExprArray.
  *
  * Ownership:
  *     Caller owns the returned ExprArray and must delete it.
  *
  * Errors:
  *     Returns nullptr if input is nullptr or allocation fails.
  */
ExprArray* filterPool(
    ExprArray* input,
    double cutoffScore);