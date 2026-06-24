#pragma once

#include "ast_nodes.h"

class ExprSimplifier {
public:

  static void simplifyExpressionArray(ExprArray& expressions,
                                bool useCoefficientValues);

  static Node* simplifyExpression(const Node* root,
                                  bool useCoefficientValues);

  static bool structurallyEqual(const Node* a, const Node* b);

private:
  static Node* simplifyNode(const Node* root,
                            bool useCoefficientValues);

  static Node* simplifyUnary(OpKind op,
                             Node* child,
                             bool useCoefficientValues);

  static Node* simplifyBinary(OpKind op,
                              Node* left,
                              Node* right,
                              bool useCoefficientValues);

  static bool isCoeffNode(const Node* n);
  static bool isVariableNode(const Node* n);
  static bool isUnaryNode(const Node* n);
  static bool isBinaryNode(const Node* n);

  static bool isZeroCoeff(const Node* n);
  static bool isOneCoeff(const Node* n);
  static bool isSafeDenominator(double x);
};