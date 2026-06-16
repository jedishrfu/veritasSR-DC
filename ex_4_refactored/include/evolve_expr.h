#pragma once

#include <string>
#include <vector>

ExprArray* generateBasicExpressions();

const std::string& getLastParseError();

Node* parseExpression(
    const std::string& exprText,
    const std::vector<std::string>& validVariableNames
);

ExprArray* evolveExpressions(ExprArray* input);

OpKind randomUnaryOp();
OpKind randomBinaryOp();

Node* mutateRandomConstant(const Node* src);
Node* mutateRandomOperator(const Node* src);
Node* deleteRandomNode(const Node* src);
Node* mutateRandomVariableToAffine(const Node* src);

Node* crossoverSubtrees(const Node* a, const Node* b);

ExprArray* filterPool(
    ExprArray* input,
    double cutoffScore);