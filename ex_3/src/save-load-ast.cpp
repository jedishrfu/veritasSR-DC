#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include "../include/ast_nodes.h"
#include "../include/ast_nodestats.h"
#include "../include/expr_array.h"

static constexpr uint32_t AST_MAGIC = 0x56535241;  // VSRA
static constexpr uint32_t STAT_MAGIC = 0x56535253; // VSRS
static constexpr uint32_t AST_VERSION = 1;

struct NodeRecord {
    int32_t kind;
    int32_t op;
    double cv;
    int32_t vi;
};

static std::string statsFilename(const std::string& filename) {
    return filename + ".stats";
}

template <typename T>
static void writeValue(std::ofstream& out, const T& value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

template <typename T>
static void readValue(std::ifstream& in, T& value, const char* what) {
    in.read(reinterpret_cast<char*>(&value), sizeof(value));

    if (!in)
        throw std::runtime_error(std::string("Unexpected EOF while reading ") + what + ".");
}

static void writeNode(std::ofstream& out, const Node* n) {
    const auto exists = static_cast<uint8_t>(n != nullptr);
    writeValue(out, exists);

    if (!exists)
        return;

    const NodeRecord r{
        static_cast<int32_t>(n->getKind()),
        static_cast<int32_t>(n->getOp()),
        n->getNodeCoeff(),
        static_cast<int32_t>(n->getVarIndex())
    };

    writeValue(out, r);

    // Always write both child slots so readNode can consume a stable format.
    writeNode(out, n->getLeftChild());
    writeNode(out, n->getRightChild());
}

static Node* readNode(std::ifstream& in) {
    uint8_t exists = 0;
    readValue(in, exists, "node flag");

    if (!exists)
        return nullptr;

    NodeRecord r{};
    readValue(in, r, "node record");

    const auto kind = static_cast<NodeKind>(r.kind);
    const auto op = static_cast<OpKind>(r.op);

    // Must match writeNode(): every real node is followed by two child slots.
    auto* left = readNode(in);
    auto* right = readNode(in);

    switch (kind) {
    case NODE_VALUE:
        if (left != nullptr || right != nullptr)
            throw std::runtime_error("Value node has children in AST file.");
        return Node::makeCoeffValue(r.cv);

    case NODE_VARIABLE:
        if (left != nullptr || right != nullptr)
            throw std::runtime_error("Variable node has children in AST file.");
        return Node::makeVariable(r.vi);

    case NODE_UNARY:
        if (left == nullptr)
            throw std::runtime_error("Unary node has a null child in AST file.");
        if (right != nullptr)
            throw std::runtime_error("Unary node has a non-null right child in AST file.");
        return Node::makeUnary(op, left);

    case NODE_BINARY:
        if (left == nullptr || right == nullptr)
            throw std::runtime_error("Binary node has a null child in AST file.");
        return Node::makeBinary(op, left, right);
    }

    throw std::runtime_error("Invalid NodeKind in AST file.");
}

static void writeNodeStats(std::ofstream& out, const NodeStats* ns) {
    const auto exists = static_cast<uint8_t>(ns != nullptr);
    writeValue(out, exists);

    if (!exists)
        return;

    writeValue(out, ns->count);
    writeValue(out, ns->numWithinTol);
    writeValue(out, ns->numOutsideTol);

    writeValue(out, ns->sumError);
    writeValue(out, ns->sumSquaredError);
    writeValue(out, ns->maxAbsError);
    writeValue(out, ns->meanOriginal);

    writeValue(out, ns->mae);
    writeValue(out, ns->mse);
    writeValue(out, ns->rmse);
    writeValue(out, ns->psnr);

    writeValue(out, ns->nodeCount);
    writeValue(out, ns->depth);
}

static NodeStats* readNodeStats(std::ifstream& in) {
    uint8_t exists = 0;
    readValue(in, exists, "NodeStats flag");

    if (!exists)
        return nullptr;

    auto* ns = new NodeStats{};

    readValue(in, ns->count, "NodeStats count");
    readValue(in, ns->numWithinTol, "NodeStats numWithinTol");
    readValue(in, ns->numOutsideTol, "NodeStats numOutsideTol");

    readValue(in, ns->sumError, "NodeStats sumError");
    readValue(in, ns->sumSquaredError, "NodeStats sumSquaredError");
    readValue(in, ns->maxAbsError, "NodeStats maxAbsError");
    readValue(in, ns->meanOriginal, "NodeStats meanOriginal");

    readValue(in, ns->mae, "NodeStats mae");
    readValue(in, ns->mse, "NodeStats mse");
    readValue(in, ns->rmse, "NodeStats rmse");
    readValue(in, ns->psnr, "NodeStats psnr");

    readValue(in, ns->nodeCount, "NodeStats nodeCount");
    readValue(in, ns->depth, "NodeStats depth");

    return ns;
}

static void writeStatsHeader(std::ofstream& out, uint64_t count) {
    constexpr auto magic = STAT_MAGIC;
    constexpr auto version = AST_VERSION;

    writeValue(out, magic);
    writeValue(out, version);
    writeValue(out, count);
}

static bool readStatsHeader(std::ifstream& in,
                            const std::string& filename,
                            uint64_t expectedCount) {
    uint32_t magic = 0;
    uint32_t version = 0;
    uint64_t count = 0;

    readValue(in, magic, "NodeStats magic");
    readValue(in, version, "NodeStats version");
    readValue(in, count, "NodeStats count");

    if (magic != STAT_MAGIC) {
        std::cerr << "Bad NodeStats magic number in file: " << filename << '\n';
        return false;
    }

    if (version != AST_VERSION) {
        std::cerr << "Unsupported NodeStats version in file: " << filename << '\n';
        return false;
    }

    if (count != expectedCount) {
        std::cerr << "NodeStats count mismatch in file: " << filename
                  << " expected " << expectedCount
                  << " but found " << count << '\n';
        return false;
    }

    return true;
}

static void saveStatsFile(const std::string& filename,
                          const ExprArray& expressions) {
    std::ofstream out(statsFilename(filename), std::ios::binary);

    if (!out)
        throw std::runtime_error("Could not open NodeStats file for writing.");

    const auto count = static_cast<uint64_t>(expressions.items.size());
    writeStatsHeader(out, count);

    for (auto i = 0; i < expressions.size(); ++i) {
        const auto* es = expressions.items[i];
        writeNodeStats(out, es ? es->ns : nullptr);
    }

    if (!out)
        throw std::runtime_error("Failed while writing NodeStats file.");
}

static NodeStats* loadOneStatsOrNew(std::ifstream* statsIn,
                                    bool loadNodeStats) {
    if (!loadNodeStats || statsIn == nullptr)
        return new NodeStats{};

    auto* ns = readNodeStats(*statsIn);

    if (ns == nullptr)
        return new NodeStats{};

    return ns;
}

void saveExpressions(const std::string& filename,
                     const ExprArray& expressions) {
    std::ofstream out(filename, std::ios::binary);

    if (!out)
        throw std::runtime_error("Could not open AST file for writing.");

    constexpr auto magic = AST_MAGIC;
    constexpr auto version = AST_VERSION;
    const auto count = static_cast<uint64_t>(expressions.items.size());

    writeValue(out, magic);
    writeValue(out, version);
    writeValue(out, count);

    for (auto i = 0; i < expressions.size(); ++i) {
        const auto* es = expressions.items[i];
        writeNode(out, es ? es->n : nullptr);
    }

    if (!out)
        throw std::runtime_error("Failed while writing AST file.");

    saveStatsFile(filename, expressions);
}

ExprArray* loadExpressions(const std::string& filename,
                           bool loadNodeStats) {
    std::ifstream in(filename, std::ios::binary);

    if (!in) {
        std::cerr << "Could not open file: " << filename << '\n';
        return nullptr;
    }

    uint32_t magic = 0;
    uint32_t version = 0;
    uint64_t count = 0;

    try {
        readValue(in, magic, "AST magic");
        readValue(in, version, "AST version");
        readValue(in, count, "AST count");
    } catch (const std::runtime_error& e) {
        std::cerr << "Failed reading AST file header: " << filename
                  << ": " << e.what() << '\n';
        return nullptr;
    }

    if (magic != AST_MAGIC) {
        std::cerr << "Bad AST magic number in file: " << filename << '\n';
        return nullptr;
    }

    if (version != AST_VERSION) {
        std::cerr << "Unsupported AST version in file: " << filename << '\n';
        return nullptr;
    }

    std::cout << "Loading: " << count
              << " expressions from file: " << filename << '\n';

    auto* result = new ExprArray();

    std::ifstream statsIn;
    auto statsAvailable = false;

    if (loadNodeStats) {
        const auto statFile = statsFilename(filename);
        statsIn.open(statFile, std::ios::binary);

        if (!statsIn) {
            std::cerr << "Could not open stats file: " << statFile << '\n';
        } else {
            try {
                statsAvailable = readStatsHeader(statsIn, statFile, count);
            } catch (const std::runtime_error& e) {
                std::cerr << "Failed reading stats file header: " << statFile
                          << ": " << e.what() << '\n';
                statsAvailable = false;
            }
        }
    }

    try {
        for (uint64_t i = 0; i < count; ++i) {
            auto* n = readNode(in);
            auto* ns = loadOneStatsOrNew(statsAvailable ? &statsIn : nullptr,
                                         statsAvailable);
            std::cout << "- Loading expression: " << i << " -> " << n->toString() << '\n';
            result->add(new ExprStats(n, ns));
        }
    } catch (const std::runtime_error& e) {
        delete result;
        std::cerr << "Failed reading expression file: " << filename
                  << ": " << e.what() << '\n';
        return nullptr;
    }

    return result;
}
