ExprArray* generateBasicExpressions()
{
  ExprArray* result = new ExprArray();

  logPrint("VarCount = %d", varTable.getCount());

  for (int i = 0; i < varTable.getCount(); i++)
  {
    Node* un = Node::makeUnary(OP_SIN, Node::makeVariable(i));
    NodeStats* uns = new NodeStats();
    ExprStats* es = new ExprStats(un, uns);
    result->add(es);

    un = Node::makeUnary(OP_COS, Node::makeVariable(i));
    uns = new NodeStats();
    es = new ExprStats(un, uns);
    result->add(es);

    un = Node::makeUnary(OP_EXP, Node::makeVariable(i));
    uns = new NodeStats();
    es = new ExprStats(un, uns);
    result->add(es);

    un = Node::makeUnary(OP_LOG, Node::makeVariable(i));
    uns = new NodeStats();
    es = new ExprStats(un, uns);
    result->add(es);

    Node* bn = Node::makeBinary(OP_ADD, Node::makeVariable(i), Node::makeCoeffValue(0));
    NodeStats* bns = new NodeStats();
    es = new ExprStats(bn, bns);
    result->add(es);

    bn = Node::makeBinary(OP_SUB, Node::makeVariable(i), Node::makeCoeffValue(0));
    bns = new NodeStats();
    es = new ExprStats(bn, bns);
    result->add(es);

    bn = Node::makeBinary(OP_MUL, Node::makeVariable(i), Node::makeCoeffValue(0));
    bns = new NodeStats();
    es = new ExprStats(bn, bns);
    result->add(es);

    bn = Node::makeBinary(OP_DIV, Node::makeVariable(i), Node::makeCoeffValue(0));
    bns = new NodeStats();
    es = new ExprStats(bn, bns);
    result->add(es);
  }

  return result;
}

ExprArray* evolveExpressions(ExprArray* input)
{
  ExprArray* result = new ExprArray();

  for (int i = 0; i < input->size(); i += 3) {
    Node* child = input->items[i]->n;

    if (!child) continue;

    Node* un = Node::makeUnary(OP_SIN, child->clone());
    NodeStats* uns = new NodeStats();
    ExprStats* es = new ExprStats(un, uns);
    result->add(es);

    un = Node::makeUnary(OP_COS, child->clone());
    uns = new NodeStats();
    es = new ExprStats(un, uns);
    result->add(es);

    ... 
  }

  for (int i = 0; i < input->size(); i += 3) {
    Node* lc = input->items[i]->n;

    if (!lc)
      continue;

    for (int j = i++; j < input->size(); j += 5) {
      Node* rc = input->items[j]->n;

      if (!rc) continue;

      Node* bn = Node::makeBinary(OP_ADD, lc->clone(), rc->clone());
      NodeStats* bns = new NodeStats();
      ExprStats* es = new ExprStats(bn, bns);
      result->add(es);

      bn = Node::makeBinary(OP_SUB, lc->clone(), rc->clone());
      bns = new NodeStats();
      es = new ExprStats(bn, bns);
      result->add(es);

      ...
    }
  }

  return result;
}