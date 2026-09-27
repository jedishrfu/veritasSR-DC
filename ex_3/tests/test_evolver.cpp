#include "test_support.h"
#include <set>

namespace {
Options forced(const std::string& operation) {
    auto opts = test::options(8, 0, 1e-8);
    auto& r = opts.evolutionRates;
    r.crossoverProbability = 0;
    r.rotateTree = r.addUnaryNode = r.addBinaryNode = r.deleteNode = 0;
    r.mutateOperator = r.doNothing = r.swapOperands = r.mutateConstant = 0;
    r.insertAffine = r.simplify = r.randomize = r.optimize = 0;
    if (operation == "crossover") { r.crossoverProbability = 1; r.doNothing = 1; }
    else test::require(r.setWeight(operation, 1), "Unknown operation " + operation);
    return opts;
}
void add(ExprArray& pool, const std::string& expression) {
    pool.add(new ExprStats(test::parse(expression).release(), new NodeStats));
}
void ownedTree(const Node* node, std::set<const Node*>& addresses) {
    test::require(node && addresses.insert(node).second, "Null, shared, or cyclic AST node");
    if (node->getKind() == NODE_BINARY) {
        test::require(node->getLeftChild() && node->getRightChild(), "Binary arity");
    } else if (node->getKind() == NODE_UNARY) {
        test::require(node->getLeftChild() && !node->getRightChild(), "Unary arity");
    } else test::require(!node->getLeftChild() && !node->getRightChild(), "Leaf arity");
    if (node->getLeftChild()) ownedTree(node->getLeftChild(), addresses);
    if (node->getRightChild()) ownedTree(node->getRightChild(), addresses);
}
std::string exactTree(const Node* node) {
    if (!node) return "null";
    std::ostringstream out;
    out << node->getKind() << ':' << node->getOp() << ':' << node->getVarIndex();
    if (node->isCoeffNode()) out << ':' << std::hexfloat << node->getNodeCoeff();
    out << '(' << exactTree(node->getLeftChild()) << ',' << exactTree(node->getRightChild()) << ')';
    return out.str();
}
std::vector<std::string> snapshot(const ExprArray& pool) {
    std::vector<std::string> result;
    for (auto* es : pool.items) result.push_back(exactTree(es->n));
    return result;
}
void operation(const std::string& name) {
    ExprArray parents;
    add(parents, "(x+2)+3"); add(parents, "x*2");
    const auto before = snapshot(parents);
    const auto opts = forced(name);
    auto data = test::samples(8, [](double x) { return 2*x+1; });
    seedRandom(24680);
    std::unique_ptr<ExprArray> children(evolveExpressions(&parents,0,1,0,0,opts,data.data()));
    test::require(children->size() >= parents.size() && children->size() <= 2*parents.size(), "Population bounds");
    test::require(snapshot(parents) == before, "Evolution modified parents");
    std::set<const Node*> addresses;
    for (auto* es : parents.items) ownedTree(es->n, addresses);
    const auto after = snapshot(*children);
    for (const auto& parent : before)
        test::require(std::find(after.begin(), after.end(), parent) != after.end(), "Parent not retained");
    for (auto* es : children->items) {
        test::require(es && es->ns, "Missing child stats");
        ownedTree(es->n, addresses);
    }
    seedRandom(24680);
    std::unique_ptr<ExprArray> replay(evolveExpressions(&parents,0,1,0,0,opts,data.data()));
    test::require(snapshot(*replay) == after, "Same seed did not reproduce offspring");
    // Destroy parents first: all offspring must remain independently owned.
    parents.clear();
    test::require(snapshot(*children) == after, "Offspring depended on parent lifetime");
}
void noOp() {
    ExprArray parents; add(parents, "x");
    auto data = test::samples(8, [](double x) { return x; });
    auto opts = forced("do_nothing");
    std::unique_ptr<ExprArray> result(evolveExpressions(&parents,0,1,0,0,opts,data.data()));
    test::require(result->size() == 1, "No-op duplicate was not removed");
    test::near(test::maxError(*result->items[0]->n,data,8), 0, 0, "No-op changed evaluation");
}
void optimize() {
    ExprArray parents; add(parents,"1*x");
    auto data = test::samples(8, [](double x) { return 3*x; });
    auto opts = forced("optimize");
    std::unique_ptr<ExprArray> result(evolveExpressions(&parents,0,1,0,0,opts,data.data()));
    double best = INFINITY;
    for (auto* es : result->items) best = std::min(best,test::maxError(*es->n,data,8));
    test::require(best < 1e-5, "Optimize operator failed to fit slope");
    test::require(test::maxError(*parents.items[0]->n,data,8) == 14, "Parent coefficients changed");
}
void filtering() {
    ExprArray pool;
    for (const char* expression : {"4", "1", "3", "2", "exp(1000)"}) add(pool,expression);
    pool.add(new ExprStats(Node::makeCoeffValue(INFINITY), new NodeStats));
    pool.add(new ExprStats(Node::makeCoeffValue(NAN), new NodeStats));
    auto data = test::samples(8, [](double) { return 0; });
    auto opts = test::options(8, 0, 1e-8);
    for (auto* es : pool.items) NodeStats::computeScore(0,0,0,0,opts,data.data(),es->n,es->ns);
    pool.add(nullptr);
    std::unique_ptr<ExprArray> result(filterPool(&pool));
    test::require(result->size() == 2, "Must retain best half of finite candidates");
    test::near(result->items[0]->ns->maxAbsError, 1, 0, "Best candidate");
    test::near(result->items[1]->ns->maxAbsError, 2, 0, "Second-best candidate");
    pool.clear();
    test::near(test::maxError(*result->items[0]->n,data,8), 1, 0, "Filter must clone retained trees");
}
void empty() {
    auto opts = forced("do_nothing"); float data = 0;
    ExprArray pool;
    std::unique_ptr<ExprArray> a(evolveExpressions(nullptr,0,0,0,0,opts,&data));
    std::unique_ptr<ExprArray> b(evolveExpressions(&pool,0,0,0,0,opts,&data));
    std::unique_ptr<ExprArray> c(filterPool(nullptr));
    test::require(a->size()==0 && b->size()==0 && c->size()==0, "Empty inputs");
    pool.add(nullptr); pool.add(new ExprStats(nullptr,new NodeStats));
    std::unique_ptr<ExprArray> d(evolveExpressions(&pool,0,0,0,0,opts,&data));
    test::require(d->size()==0, "Invalid entries must be skipped");
}
}
int main(int argc, char** argv) {
    return test::run(argc, argv, {
        {"rotate_tree", [] { operation("rotate_tree"); }},
        {"add_unary_node", [] { operation("add_unary_node"); }},
        {"add_binary_node", [] { operation("add_binary_node"); }},
        {"delete_node", [] { operation("delete_node"); }},
        {"mutate_operator", [] { operation("mutate_operator"); }},
        {"do_nothing", noOp},
        {"swap_operands", [] { operation("swap_operands"); }},
        {"mutate_constant", [] { operation("mutate_constant"); }},
        {"insert_affine", [] { operation("insert_affine"); }},
        {"simplify", [] { operation("simplify"); }},
        {"randomize", [] { operation("randomize"); }},
        {"optimize", optimize},
        {"crossover", [] { operation("crossover"); }},
        {"filtering_and_ownership", filtering}, {"empty_and_invalid_entries", empty}
    });
}
