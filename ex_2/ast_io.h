#pragma once

#include <string>

#include "ast_nodes.h"

void saveExpressions(
    const std::string& filename,
    const ExprArray& expressions);

ExprArray loadExpressions(
    const std::string& filename,
    bool loadNodeStats);
