#include <stdio.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include <string>
#include <vector>

#include "helper_code.h"
#ifndef AST_NODES_H
#define AST_NODES_H

enum NodeKind
{
  NODE_VALUE,
  NODE_VARIABLE,
  NODE_UNARY,
  NODE_BINARY
};

enum OpKind
{
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_SIN,
  OP_COS,
  OP_POW,
  OP_EXP,
  OP_LN,
  OP_LOG
};

inline void appendFormat(char *buffer, int maxLen, const char *fmt, ...)
{
  if (buffer == 0 || maxLen <= 0)
    return;

  int len = (int)strlen(buffer);
  if (len >= maxLen - 1)
    return;

  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer + len, maxLen - len, fmt, args);
  va_end(args);

  buffer[maxLen - 1] = '\0';
}

// GLOBAL expression enviroment holds variable values e.g. for x,y,z, and t
class ExprContext 
{
public:
  std::vector<std::string> varNames;
  std::vector<double> varRegisters;

  ExprContext()
  {
    varNames.push_back("x");
    varRegisters.push_back(0.0);
  }

  int getVarCount() const
  {
    return (int)varNames.size();
  }

  const char *getName(int index) const
  {
    if (index < 0 ||
        index >= (int)varNames.size())
      return 0;

    return varNames[index].c_str();
  }

  void setName(int index,
               const std::string &name)
  {
    if (index < 0)
      return;

    if (index >= (int)varNames.size())
      varNames.resize(index + 1);

    varNames[index] = name;
  }

  double getVarRegister(int index) const
  {
    if (index < 0 ||
        index >= (int)varRegisters.size())
      return 0.0;

    return varRegisters[index];
  }

  void setVarRegister(int index,
                      double value)
  {
    if (index < 0)
      return;

    if (index >= (int)varRegisters.size())
      varRegisters.resize(index + 1, 0.0);

    varRegisters[index] = value;
  }

  std::vector<double> &getVarRegisters()
  {
    return varRegisters;
  }

  const std::vector<double> &getVarRegisters() const
  {
    return varRegisters;
  }
};

// FORWARD References 
class Node;
class ScoreStats;

// LOCAL expressiom environment holds expression coefficient values
struct ExprEntry
{
  Node *expr;
  ScoreStats *score;
  std::vector<double> valRegisters;

  ExprEntry(Node *e = 0, ScoreStats *s = 0)
      : expr(e), score(s) {}

  double getValRegister(int index) const
  {
    if (index < 0 || index >= (int)valRegisters.size())
      return 0.0;

    return valRegisters[index];
  }

  void setValRegister(int index, double value)
  {
    if (index < 0)
      return;

    if (index >= (int)valRegisters.size())
      valRegisters.resize(index + 1, 0.0);

    valRegisters[index] = value;
  }
};

class Node
{
public:
  NodeKind kind;
  Node *left;
  Node *right;

  Node(NodeKind k)
      : kind(k), left(0), right(0) {}

  virtual ~Node()
  {
    delete left;
    delete right;

    left = 0;
    right = 0;
  }

  virtual double eval(const ExprContext &ctx, const ExprEntry &entry) = 0;

  virtual void toString(
      char *buffer,
      int maxLen,
      const ExprContext &ctx,
      const ExprEntry &entry) = 0; // needed in VariableNode's toString()

  virtual void writeBinary(FILE *fp) = 0;

  virtual Node *clone() = 0;

  static Node *readBinary(FILE *fp);
};

class ValueNode : public Node
{
public:
  int index;

  ValueNode(int index)
      : Node(NODE_VALUE), index(index) {}

  int getIndex() const
  {
    return index;
  }

  void setIndex(int newIndex)
  {
    index = newIndex;
  }

  double eval(const ExprContext &ctx, const ExprEntry &entry)
  {
    return entry.getValRegister(index);
  }

  void toString(
      char *buffer,
      int maxLen,
      const ExprContext &ctx,
      const ExprEntry &entry)
  {
    appendFormat(buffer, maxLen, "c%d:%g", index, entry.getValRegister(index));
  }

  void writeBinary(FILE *fp)
  {
    fwrite(&kind, sizeof(NodeKind), 1, fp);
    fwrite(&index, sizeof(int), 1, fp);
  }

  Node *clone()
  {
    return new ValueNode(index);
  }
};

class VariableNode : public Node
{
public:
  int index;

  VariableNode(int i)
      : Node(NODE_VARIABLE), index(i) {}

  int getIndex() const
  {
    return index;
  }

  void setIndex(int newIndex)
  {
    index = newIndex;
  }

  double eval(const ExprContext &ctx, const ExprEntry &entry)
  {
    return ctx.getVarRegister(index);
  }

  void toString(
      char *buffer,
      int maxLen,
      const ExprContext &ctx,
      const ExprEntry &entry)
  {
    const char *name = ctx.getName(index);

    if (name)
      appendFormat(buffer, maxLen, "%s", name);
    else
      appendFormat(buffer, maxLen, "%s%d", ctx.getName(index));
  }

  void writeBinary(FILE *fp)
  {
    fwrite(&kind, sizeof(NodeKind), 1, fp);
    fwrite(&index, sizeof(int), 1, fp);
  }

  Node *clone()
  {
    return new VariableNode(index);
  }
};

class UnaryOpNode : public Node
{
public:
  OpKind op;

  UnaryOpNode(OpKind o, Node *child)
      : Node(NODE_UNARY), op(o)
  {
    left = child;
  }

  double deg2rad(double degrees)
  {
    const double PI = 3.14159265358979323846;
    return degrees * PI / 180.0;
  }

  double eval(const ExprContext &ctx, const ExprEntry &entry)
  {
    if (!left)
      return 0.0;

    double x = left->eval(ctx, entry);

    switch (op)
    {
    case OP_SIN:
      return sin(deg2rad(x));
    case OP_COS:
      return cos(deg2rad(x));
    case OP_EXP:
      return exp(x);

    case OP_LN:
      if (x <= 0.0)
        return 0.0;
      return log(x);

    case OP_LOG:
      if (x <= 0.0)
        return 0.0;
      return log10(x);

    default:
      return 0.0;
    }
  }

  void toString(
      char *buffer,
      int maxLen,
      const ExprContext &ctx,
      const ExprEntry &entry)
  {
    switch (op)
    {
    case OP_SIN:
      appendFormat(buffer, maxLen, "sin( ");
      break;
    case OP_COS:
      appendFormat(buffer, maxLen, "cos( ");
      break;
    case OP_EXP:
      appendFormat(buffer, maxLen, "exp( ");
      break;
    case OP_LN:
      appendFormat(buffer, maxLen, "ln( ");
      break;
    case OP_LOG:
      appendFormat(buffer, maxLen, "log( ");
      break;
    default:
      appendFormat(buffer, maxLen, "?( ");
      break;
    }

    if (left)
      left->toString(buffer, maxLen, ctx, entry);
    else
      appendFormat(buffer, maxLen, "NULL");

    appendFormat(buffer, maxLen, " )");
  }

  void writeBinary(FILE *fp)
  {
    fwrite(&kind, sizeof(NodeKind), 1, fp);
    fwrite(&op, sizeof(OpKind), 1, fp);

    if (left)
      left->writeBinary(fp);
  }

  Node *clone()
  {
    return new UnaryOpNode(op, left ? left->clone() : 0);
  }
};

class BinaryOpNode : public Node
{
public:
  OpKind op;

  BinaryOpNode(OpKind o, Node *l = 0, Node *r = 0)
      : Node(NODE_BINARY), op(o)
  {
    left = l;
    right = r;
  }

  double eval(const ExprContext &ctx, const ExprEntry &entry)
  {
    if (!left || !right)
      return 0.0;

    double a = left->eval(ctx, entry);
    double b = right->eval(ctx, entry);

    switch (op)
    {
    case OP_ADD:
      return a + b;
    case OP_SUB:
      return a - b;
    case OP_MUL:
      return a * b;

    case OP_DIV:
      if (b == 0.0)
        return 0.0;
      return a / b;

    case OP_POW:
      return pow(a, b);

    default:
      return 0.0;
    }
  }

  void toString(
      char *buffer,
      int maxLen,
      const ExprContext &ctx,
      const ExprEntry &entry)
  {
    appendFormat(buffer, maxLen, "( ");

    if (left)
      left->toString(buffer, maxLen, ctx, entry);
    else
      appendFormat(buffer, maxLen, "NULL");

    switch (op)
    {
    case OP_ADD:
      appendFormat(buffer, maxLen, " + ");
      break;
    case OP_SUB:
      appendFormat(buffer, maxLen, " - ");
      break;
    case OP_MUL:
      appendFormat(buffer, maxLen, " * ");
      break;
    case OP_DIV:
      appendFormat(buffer, maxLen, " / ");
      break;
    case OP_POW:
      appendFormat(buffer, maxLen, " ^ ");
      break;
    default:
      appendFormat(buffer, maxLen, " ? ");
      break;
    }

    if (right)
      right->toString(buffer, maxLen, ctx, entry);
    else
      appendFormat(buffer, maxLen, "NULL");

    appendFormat(buffer, maxLen, " )");
  }

  void writeBinary(FILE *fp)
  {
    fwrite(&kind, sizeof(NodeKind), 1, fp);
    fwrite(&op, sizeof(OpKind), 1, fp);

    if (left)
      left->writeBinary(fp);

    if (right)
      right->writeBinary(fp);
  }

  Node *clone()
  {
    return new BinaryOpNode(op, left ? left->clone() : 0, right ? right->clone() : 0);
  }
};

inline Node *Node::readBinary(FILE *fp)
{
  NodeKind kind;

  if (fread(&kind, sizeof(NodeKind), 1, fp) != 1)
  {
    return 0;
  }

  if (kind == NODE_VALUE)
  {
    int index;
    fread(&index, sizeof(int), 1, fp);
    return new ValueNode(index);
  }

  if (kind == NODE_VARIABLE)
  {
    int index;
    fread(&index, sizeof(int), 1, fp);
    return new VariableNode(index);
  }

  if (kind == NODE_UNARY)
  {
    OpKind op;
    fread(&op, sizeof(OpKind), 1, fp);

    Node *left = Node::readBinary(fp);
    return new UnaryOpNode(op, left);
  }

  if (kind == NODE_BINARY)
  {
    OpKind op;
    fread(&op, sizeof(OpKind), 1, fp);

    Node *left = Node::readBinary(fp);
    Node *right = Node::readBinary(fp);

    return new BinaryOpNode(op, left, right);
  }

  return 0;
}

inline int countValueNodes(Node *node)
{
  if (node == 0)
    return 0;

  if (node->kind == NODE_VALUE)
    return 1;

  if (node->kind == NODE_VARIABLE)
    return 0;

  if (node->kind == NODE_UNARY)
    return countValueNodes(node->left);

  if (node->kind == NODE_BINARY)
    return countValueNodes(node->left) + countValueNodes(node->right);

  return 0;
}

inline int countNodes(Node *node)
{
  if (node == 0)
    return 0;

  if (node->kind == NODE_VALUE)
    return 1;

  if (node->kind == NODE_VARIABLE)
    return 1;

  if (node->kind == NODE_UNARY)
    return 1 + countNodes(node->left);

  if (node->kind == NODE_BINARY)
    return 1 + countNodes(node->left) + countNodes(node->right);

  return 0;
}

inline int treeDepth(Node *node)
{
  if (node == 0)
    return 0;

  if (node->kind == NODE_VALUE ||
      node->kind == NODE_VARIABLE)
  {
    return 1;
  }

  if (node->kind == NODE_UNARY)
  {
    UnaryOpNode *u = (UnaryOpNode *)node;
    return 1 + treeDepth(u->left);
  }

  if (node->kind == NODE_BINARY)
  {
    BinaryOpNode *b = (BinaryOpNode *)node;

    int leftDepth = treeDepth(b->left);
    int rightDepth = treeDepth(b->right);

    return 1 + (leftDepth > rightDepth ? leftDepth : rightDepth);
  }

  return 0;
}

class ScoreStats
{
public:
  long count;

  long numWithinTol;
  long numOutsideTol;

  double sumError;
  double sumSquaredError;
  double maxAbsError;
  double meanOriginal;

  double mae;
  double mse;
  double rmse;
  double psnr;

  int nodeCount;
  int depth;

  ScoreStats()
      : count(0),
        numWithinTol(0),
        numOutsideTol(0),
        sumError(0.0),
        sumSquaredError(0.0),
        maxAbsError(0.0),
        meanOriginal(0.0),
        mae(0.0),
        mse(0.0),
        rmse(0.0),
        psnr(0.0),
        nodeCount(0),
        depth(0) {}
  void toString(char *buffer, int maxLen) const
  {
    buffer[0] = '\0';

    appendFormat(buffer, maxLen,
                 "count=%d "
                 "nodes=%d "
                 "depth=%d "
                 "mae=%g "
                 "mse=%g "
                 "rmse=%g "
                 "psnr=%g "
                 "maxAbs=%g "
                 "within=%d "
                 "outside=%d",
                 count,
                 nodeCount,
                 depth,
                 mae,
                 mse,
                 rmse,
                 psnr,
                 maxAbsError,
                 numWithinTol,
                 numOutsideTol);
  }
};

inline ScoreStats *computeScore(
    double tol,
    float *data,
    int numFloats,
    Node *expr,
    const ExprContext &ctx,
    const ExprEntry &entry)
{
  ScoreStats *s = new ScoreStats();

  s->count = numFloats;
  s->nodeCount = countNodes(expr);
  s->depth = treeDepth(expr);

  double peakValue = 0.0;

  for (int i = 0; i < numFloats; i++)
  {

    double computed = expr->eval(ctx, entry);

    if (computed != computed)
    {
      computed = 1.0e99;
    }

    double error = computed - data[i];
    double absError = fabs(error);

    s->sumError += absError;
    s->sumSquaredError += error * error;
    s->meanOriginal += data[i];

    if (absError > s->maxAbsError)
    {
      s->maxAbsError = absError;
    }

    if (absError <= tol)
    {
      s->numWithinTol++;
    }
    else
    {
      s->numOutsideTol++;
    }

    if (fabs(data[i]) > peakValue)
    {
      peakValue = fabs(data[i]);
    }
  }

  if (numFloats > 0)
  {
    s->meanOriginal /= numFloats;
    s->mae = s->sumError / numFloats;
    s->mse = s->sumSquaredError / numFloats;
    s->rmse = sqrt(s->mse);

    if (s->rmse > 0.0 && peakValue > 0.0)
    {
      s->psnr = 20.0 * log10(peakValue / s->rmse);
    }
    else
    {
      s->psnr = 999.0;
    }
  }

  return s;
}

class ExprArray
{
public:
  std::vector<ExprEntry> items;

  ExprArray() {}

  ~ExprArray()
  {
    clear();
  }

  int size() const
  {
    return (int)items.size();
  }

  void clear()
  {
    // for (size_t i = 0; i < items.size(); i++)
    // {
    //   delete items[i].expr;
    //   delete items[i].score;

    //   items[i].expr = 0;
    //   items[i].score = 0;
    // }

    items.clear();
  }

  void add(Node *n)
  {
    items.push_back(ExprEntry(n, 0));
  }

  void take(Node *n, ScoreStats *s)
  {
    items.push_back(ExprEntry(n, s));
  }

  Node *get(int index)
  {
    if (index < 0 || index >= (int)items.size())
      return 0;

    return items[index].expr;
  }

private:
  ExprArray(const ExprArray &);
  ExprArray &operator=(const ExprArray &);
};

#endif