#pragma once

#include "ast_nodes.h"

OpKind randomUnaryOp();
OpKind randomBinaryOp();

Node* mutateRandomConstant(const Node* src);
Node* mutateRandomOperator(const Node* src);
Node* deleteRandomNode(const Node* src);
Node* mutateRandomVariableToAffine(const Node* src);