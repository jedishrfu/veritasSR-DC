#pragma once

#include <set>
#include <string>

#include "ast_nodes.h"

bool addUniqueTree(
    ExprArray* result,
    Node* tree,
    std::set<std::string>& seen);

bool addUniqueExprStats(
    ExprArray* result,
    ExprStats* src,
    std::set<std::string>& seen);