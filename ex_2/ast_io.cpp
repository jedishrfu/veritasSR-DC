#include "ast_io.h"

#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>

static constexpr uint32_t AST_MAGIC = 0x56535241; // VSRA
static constexpr uint32_t STAT_MAGIC = 0x56535253; // VSRS
static constexpr uint32_t AST_VERSION = 1;

struct NodeRecord
{
    int32_t kind;
    int32_t op;
    double cv;
    int32_t vi;
};

static std::string statsFilename(const std::string& filename)
{
    return filename + ".stats";
}

static void writeNode(std::ofstream& out, const Node* n)
{
    uint8_t exists = (n != nullptr);
    out.write(reinterpret_cast<const char*>(&exists), sizeof(exists));

    if (!exists)
        return;

    NodeRecord r;
    r.kind = static_cast<int32_t>(n->getKind());
    r.op = static_cast<int32_t>(n->getOp());
    r.cv = n->getNodeCoeff();
    r.vi = static_cast<int32_t>(n->getVarIndex());

    out.write(reinterpret_cast<const char*>(&r), sizeof(r));

    writeNode(out, n->getLeftChild());
    writeNode(out, n->getRightChild());
}

static Node* readNode(std::ifstream& in)
{
    uint8_t exists = 0;
    in.read(reinterpret_cast<char*>(&exists), sizeof(exists));

    if (!in)
        throw std::runtime_error("Unexpected EOF while reading node flag.");

    if (!exists)
        return nullptr;

    NodeRecord r;
    in.read(reinterpret_cast<char*>(&r), sizeof(r));

    if (!in)
        throw std::runtime_error("Unexpected EOF while reading node record.");

    NodeKind kind = static_cast<NodeKind>(r.kind);
    OpKind op = static_cast<OpKind>(r.op);

    switch (kind)
    {
    case NODE_VALUE:
        return Node::makeCoeffValue(r.cv);

    case NODE_VARIABLE:
        return Node::makeVariable(r.vi);

    case NODE_UNARY:
        return Node::makeUnary(op, readNode(in));

    case NODE_BINARY: {
        Node* left = readNode(in);
        Node* right = readNode(in);
        return Node::makeBinary(op, left, right);
    }
    }

    throw std::runtime_error("Invalid NodeKind in AST file.");
}

static void writeNodeStats(std::ofstream& out, const NodeStats* ns)
{
    NodeStats temp;

    if (ns != nullptr)
        temp = *ns;

    out.write(reinterpret_cast<const char*>(&temp.count), sizeof(temp.count));
    out.write(reinterpret_cast<const char*>(&temp.numWithinTol), sizeof(temp.numWithinTol));
    out.write(reinterpret_cast<const char*>(&temp.numOutsideTol), sizeof(temp.numOutsideTol));

    out.write(reinterpret_cast<const char*>(&temp.sumError), sizeof(temp.sumError));
    out.write(reinterpret_cast<const char*>(&temp.sumSquaredError), sizeof(temp.sumSquaredError));
    out.write(reinterpret_cast<const char*>(&temp.maxAbsError), sizeof(temp.maxAbsError));
    out.write(reinterpret_cast<const char*>(&temp.meanOriginal), sizeof(temp.meanOriginal));

    out.write(reinterpret_cast<const char*>(&temp.mae), sizeof(temp.mae));
    out.write(reinterpret_cast<const char*>(&temp.mse), sizeof(temp.mse));
    out.write(reinterpret_cast<const char*>(&temp.rmse), sizeof(temp.rmse));
    out.write(reinterpret_cast<const char*>(&temp.psnr), sizeof(temp.psnr));

    out.write(reinterpret_cast<const char*>(&temp.nodeCount), sizeof(temp.nodeCount));
    out.write(reinterpret_cast<const char*>(&temp.depth), sizeof(temp.depth));
}

static NodeStats* readNodeStats(std::ifstream& in)
{
    NodeStats* ns = new NodeStats();

    in.read(reinterpret_cast<char*>(&ns->count), sizeof(ns->count));
    in.read(reinterpret_cast<char*>(&ns->numWithinTol), sizeof(ns->numWithinTol));
    in.read(reinterpret_cast<char*>(&ns->numOutsideTol), sizeof(ns->numOutsideTol));

    in.read(reinterpret_cast<char*>(&ns->sumError), sizeof(ns->sumError));
    in.read(reinterpret_cast<char*>(&ns->sumSquaredError), sizeof(ns->sumSquaredError));
    in.read(reinterpret_cast<char*>(&ns->maxAbsError), sizeof(ns->maxAbsError));
    in.read(reinterpret_cast<char*>(&ns->meanOriginal), sizeof(ns->meanOriginal));

    in.read(reinterpret_cast<char*>(&ns->mae), sizeof(ns->mae));
    in.read(reinterpret_cast<char*>(&ns->mse), sizeof(ns->mse));
    in.read(reinterpret_cast<char*>(&ns->rmse), sizeof(ns->rmse));
    in.read(reinterpret_cast<char*>(&ns->psnr), sizeof(ns->psnr));

    in.read(reinterpret_cast<char*>(&ns->nodeCount), sizeof(ns->nodeCount));
    in.read(reinterpret_cast<char*>(&ns->depth), sizeof(ns->depth));

    if (!in)
    {
        delete ns;
        throw std::runtime_error("Unexpected EOF while reading NodeStats.");
    }

    return ns;
}

static void saveStatsFile(
    const std::string& filename,
    const ExprArray& expressions)
{
    std::ofstream out(statsFilename(filename), std::ios::binary);

    if (!out)
        throw std::runtime_error("Could not open NodeStats file for writing.");

    uint32_t magic = STAT_MAGIC;
    uint32_t version = AST_VERSION;
    uint64_t count = static_cast<uint64_t>(expressions.items.size());

    out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (int i = 0; i < expressions.size(); i++)
    {
        ExprStats* es = expressions.items[i];
        writeNodeStats(out, es ? es->ns : nullptr);
    }
}

static NodeStats* loadOneStatsOrNew(
    std::ifstream* statsIn,
    bool loadNodeStats)
{
    if (!loadNodeStats)
        return new NodeStats();

    return readNodeStats(*statsIn);
}

void saveExpressions(
    const std::string& filename,
    const ExprArray& expressions)
{
    std::ofstream out(filename, std::ios::binary);

    if (!out)
        throw std::runtime_error("Could not open AST file for writing.");

    uint32_t magic = AST_MAGIC;
    uint32_t version = AST_VERSION;
    uint64_t count = static_cast<uint64_t>(expressions.items.size());

    out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (int i = 0; i < expressions.size(); i++)
    {
        ExprStats* es = expressions.items[i];
        writeNode(out, es ? es->n : nullptr);
    }

    if (!out)
        throw std::runtime_error("Failed while writing AST file.");

    saveStatsFile(filename, expressions);
}

ExprArray loadExpressions(
    const std::string& filename,
    bool loadNodeStats)
{
    std::ifstream in(filename, std::ios::binary);

    if (!in)
        throw std::runtime_error("Could not open AST file for reading.");

    uint32_t magic = 0;
    uint32_t version = 0;
    uint64_t count = 0;

    in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    in.read(reinterpret_cast<char*>(&count), sizeof(count));

    if (magic != AST_MAGIC)
        throw std::runtime_error("Invalid AST file.");

    if (version != AST_VERSION)
        throw std::runtime_error("Unsupported AST file version.");

    std::ifstream statsIn;

    if (loadNodeStats)
    {
        statsIn.open(statsFilename(filename), std::ios::binary);

        if (!statsIn)
            throw std::runtime_error("Could not open NodeStats file for reading.");

        uint32_t smagic = 0;
        uint32_t sversion = 0;
        uint64_t scount = 0;

        statsIn.read(reinterpret_cast<char*>(&smagic), sizeof(smagic));
        statsIn.read(reinterpret_cast<char*>(&sversion), sizeof(sversion));
        statsIn.read(reinterpret_cast<char*>(&scount), sizeof(scount));

        if (smagic != STAT_MAGIC)
            throw std::runtime_error("Invalid NodeStats file.");

        if (sversion != AST_VERSION)
            throw std::runtime_error("Unsupported NodeStats file version.");

        if (scount != count)
            throw std::runtime_error("NodeStats count does not match AST count.");
    }

    ExprArray result;

    for (uint64_t i = 0; i < count; i++)
    {
        Node* n = readNode(in);
        NodeStats* ns = loadOneStatsOrNew(
            loadNodeStats ? &statsIn : nullptr,
            loadNodeStats);

        result.add(new ExprStats(n, ns));
    }

    return result;
}