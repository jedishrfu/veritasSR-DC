#include "../include/expr_parser.h"

static std::string g_parseError;

const std::string& getLastParseError() {
  return g_parseError;
}

ExprParser::ExprParser(
  const std::string& s,
  const std::vector<std::string>& validVars)
  : text(s), vars(validVars), pos(0) {}

Node* ExprParser::parse() {
  Node* root = parseAddSub();
  skipSpaces();

  if (!root)
    return nullptr;

  if (pos != text.size()) {
    setError("Unexpected text at position " + std::to_string(pos));
    delete root;
    return nullptr;
  }

  return root;
}

void ExprParser::setError(const std::string& msg) {
  if (g_parseError.empty())
    g_parseError = msg;
}

void ExprParser::skipSpaces() {
  while (pos < text.size() &&
    std::isspace((unsigned char)text[pos])) {
    pos++;
  }
}

bool ExprParser::match(char c) {
  skipSpaces();

  if (pos < text.size() && text[pos] == c) {
    pos++;
    return true;
  }

  return false;
}

char ExprParser::peek() {
  skipSpaces();

  if (pos >= text.size())
    return '\0';

  return text[pos];
}

Node* ExprParser::parseAddSub() {
  Node* left = parseMulDiv();

  if (!left)
    return nullptr;

  while (true) {
    if (match('+')) {
      Node* right = parseMulDiv();

      if (!right) {
        delete left;
        setError("Missing right operand after +");
        return nullptr;
      }

      left = Node::makeBinary(OP_ADD, left, right);
    }
    else if (match('-')) {
      Node* right = parseMulDiv();

      if (!right) {
        delete left;
        setError("Missing right operand after -");
        return nullptr;
      }

      left = Node::makeBinary(OP_SUB, left, right);
    }
    else {
      break;
    }
  }

  return left;
}

Node* ExprParser::parseMulDiv() {
  Node* left = parseUnary();

  if (!left)
    return nullptr;

  while (true) {
    if (match('*')) {
      Node* right = parseUnary();

      if (!right) {
        delete left;
        setError("Missing right operand after *");
        return nullptr;
      }

      left = Node::makeBinary(OP_MUL, left, right);
    }
    else if (match('/')) {
      Node* right = parseUnary();

      if (!right) {
        delete left;
        setError("Missing right operand after /");
        return nullptr;
      }

      left = Node::makeBinary(OP_DIV, left, right);
    }
    else {
      break;
    }
  }

  return left;
}

Node* ExprParser::parseUnary() {
  skipSpaces();

  if (match('-')) {
    Node* child = parseUnary();

    if (!child) {
      setError("Missing operand after unary -");
      return nullptr;
    }

    return Node::makeBinary(
      OP_SUB,
      Node::makeCoeffValue(0.0),
      child
    );
  }

  if (std::isalpha((unsigned char)peek()) || peek() == '_') {
    size_t savePos = pos;
    std::string name = parseIdentifier();

    skipSpaces();

    if (match('(')) {
      Node* arg = parseAddSub();

      if (!arg) {
        setError("Missing argument for function " + name);
        return nullptr;
      }

      if (!match(')')) {
        delete arg;
        setError("Missing closing parenthesis after function " + name);
        return nullptr;
      }

      OpKind op = OP_NONE;

      if (name == "sin")
        op = OP_SIN;
      else if (name == "cos")
        op = OP_COS;
      else if (name == "exp")
        op = OP_EXP;
      else if (name == "log")
        op = OP_LOG;
      else {
        delete arg;
        setError("Unknown function: " + name);
        return nullptr;
      }

      return Node::makeUnary(op, arg);
    }

    pos = savePos;
  }

  return parsePrimary();
}

Node* ExprParser::parsePrimary() {
  skipSpaces();

  if (match('(')) {
    Node* inside = parseAddSub();

    if (!inside) {
      setError("Missing expression after '('");
      return nullptr;
    }

    if (!match(')')) {
      delete inside;
      setError("Mismatched parentheses: missing ')'");
      return nullptr;
    }

    return inside;
  }

  if (std::isdigit((unsigned char)peek()) || peek() == '.')
    return parseNumber();

  if (std::isalpha((unsigned char)peek()) || peek() == '_')
    return parseVariable();

  setError(
    "Expected number, variable, function, or '(' at position "
    + std::to_string(pos)
  );

  return nullptr;
}

Node* ExprParser::parseNumber() {
  skipSpaces();

  const char* start = text.c_str() + pos;
  char* end = nullptr;

  double value = std::strtod(start, &end);

  if (end == start) {
    setError("Invalid number at position " + std::to_string(pos));
    return nullptr;
  }

  pos += (size_t)(end - start);

  return Node::makeCoeffValue(value);
}

std::string ExprParser::parseIdentifier() {
  skipSpaces();

  size_t start = pos;

  if (pos < text.size() &&
    (std::isalpha((unsigned char)text[pos]) || text[pos] == '_')) {
    pos++;

    while (pos < text.size() &&
      (std::isalnum((unsigned char)text[pos]) ||
        text[pos] == '_')) {
      pos++;
    }
  }

  return text.substr(start, pos - start);
}

Node* ExprParser::parseVariable() {
  std::string name = parseIdentifier();

  for (size_t i = 0; i < vars.size(); i++) {
    if (vars[i] == name)
      return Node::makeVariable((int)i);
  }

  setError("Unknown variable: " + name);
  return nullptr;
}

Node* parseExpression(
  const std::string& exprText,
  const std::vector<std::string>& validVariableNames) {
  g_parseError.clear();

  ExprParser parser(exprText, validVariableNames);
  return parser.parse();
}
