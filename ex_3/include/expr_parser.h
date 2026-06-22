#pragma once

#include <string>
#include <vector>

#include "ast_nodes.h"
#include "expr_parser.h"

const std::string& getLastParseError();

class ExprParser {
private:
  const std::string& text;
  const std::vector<std::string>& vars;
  size_t pos;

public:
  ExprParser(
    const std::string& s,
    const std::vector<std::string>& validVars);

  Node* parse();

  size_t getPos() const { return pos; }
  const std::string& getText() const { return text; }
  const std::vector<std::string>& getVars() const { return vars; }

  void setPos(size_t newPos) { pos = newPos; }

  static void setError(const std::string& msg);

private:
  void skipSpaces();
  bool match(char c);
  char peek();

  Node* parseAddSub();
  Node* parseMulDiv();
  Node* parseUnary();
  Node* parsePrimary();
  Node* parseNumber();
  Node* parseVariable();

  std::string parseIdentifier();
};

Node* parseExpression(
  const std::string& exprText,
  const std::vector<std::string>& validVariableNames);
