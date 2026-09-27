#include "../include/ast_binary_io.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

#include "../include/expr_array.h"
#include "../include/ast_nodes.h"

namespace {

enum PackedNodeKind : uint8_t {
    PK_VARIABLE = 0,
    PK_VALUE    = 1,
    PK_UNARY    = 2,
    PK_BINARY   = 3
};

constexpr uint32_t kMaxNodeCount =
    static_cast<uint32_t>(std::numeric_limits<uint16_t>::max());

size_t exprArraySize(const ExprArray& arr) {
    return arr.size();
}

const Node* exprArrayGet(const ExprArray& arr, size_t i) {
    return arr.items[i]->n;
}

void exprArrayAdd(ExprArray& arr, std::unique_ptr<Node> root) {
    // Keep ownership local until both allocations/operations succeed.
    std::unique_ptr<ExprStats> stats(new ExprStats(root.get(), new NodeStats()));
    root.release(); // ExprStats now owns the tree, as in the existing API.
    arr.add(stats.get());
    stats.release(); // ExprArray now owns ExprStats.
}

void writeU8(std::ostream& out, uint8_t value) {
    out.put(static_cast<char>(value));
}

uint8_t readU8(std::istream& in) {
    const int value = in.get();
    if (value == std::char_traits<char>::eof()) {
        throw std::runtime_error("Unexpected EOF reading uint8");
    }
    return static_cast<uint8_t>(value);
}

void writeU16(std::ostream& out, uint16_t value) {
    writeU8(out, static_cast<uint8_t>(value & 0xffu));
    writeU8(out, static_cast<uint8_t>((value >> 8) & 0xffu));
}

uint16_t readU16(std::istream& in) {
    const uint16_t b0 = readU8(in);
    const uint16_t b1 = readU8(in);
    return static_cast<uint16_t>(b0 | static_cast<uint16_t>(b1 << 8));
}

void writeU32(std::ostream& out, uint32_t value) {
    writeU8(out, static_cast<uint8_t>(value & 0xffu));
    writeU8(out, static_cast<uint8_t>((value >> 8) & 0xffu));
    writeU8(out, static_cast<uint8_t>((value >> 16) & 0xffu));
    writeU8(out, static_cast<uint8_t>((value >> 24) & 0xffu));
}

uint32_t readU32(std::istream& in) {
    const uint32_t b0 = readU8(in);
    const uint32_t b1 = readU8(in);
    const uint32_t b2 = readU8(in);
    const uint32_t b3 = readU8(in);
    return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

void writeF32(std::ostream& out, float value) {
    static_assert(sizeof(float) == sizeof(uint32_t),
                  "Binary format requires 32-bit float");
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    writeU32(out, bits);
}

float readF32(std::istream& in) {
    const uint32_t bits = readU32(in);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

uint8_t packVersion(uint8_t major, uint8_t minor) {
    if (major > 15 || minor > 15) {
        throw std::runtime_error("Version fields must fit in 4 bits");
    }
    return static_cast<uint8_t>((major << 4) | minor);
}

uint8_t codeFromBlockSize(uint32_t blockSize) {
    switch (blockSize) {
        case 250:  return 0;
        case 500:  return 1;
        case 1000: return 2;
        case 2000: return 3;
        default:
            throw std::runtime_error(
                "Block size must be 250, 500, 1000, or 2000");
    }
}

uint16_t blockSizeFromCode(uint8_t code) {
    switch (code) {
        case 0: return 250;
        case 1: return 500;
        case 2: return 1000;
        case 3: return 2000;
        default: throw std::runtime_error("Invalid block-size code");
    }
}

uint8_t packBlockInfo(uint32_t blockSize, uint8_t flags) {
    if (flags > 15) {
        throw std::runtime_error("Flags must fit in 4 bits");
    }
    return static_cast<uint8_t>((codeFromBlockSize(blockSize) << 4) | flags);
}

uint32_t countAstNodes(const Node* node) {
    if (!node) {
        return 0;
    }

    uint32_t count = 1;
    const uint32_t leftCount = countAstNodes(node->getLeftChild());
    if (leftCount > kMaxNodeCount - count) {
        throw std::runtime_error("AST exceeds uint16 node count");
    }
    count += leftCount;

    const uint32_t rightCount = countAstNodes(node->getRightChild());
    if (rightCount > kMaxNodeCount - count) {
        throw std::runtime_error("AST exceeds uint16 node count");
    }
    return count + rightCount;
}

void writeNode(std::ostream& out, const Node* node) {
    if (!node) {
        throw std::runtime_error("Cannot write a null AST node");
    }

    switch (node->getKind()) {
        case NODE_VARIABLE:
            // This format represents the single independent variable x.
            writeU8(out, static_cast<uint8_t>(PK_VARIABLE << 6));
            return;

        case NODE_VALUE:
            writeU8(out, static_cast<uint8_t>(PK_VALUE << 6));
            writeF32(out, static_cast<float>(node->getNodeCoeff()));
            return;

        case NODE_UNARY: {
            if (!node->getLeftChild()) {
                throw std::runtime_error("Unary node is missing its child");
            }
            const uint8_t op = static_cast<uint8_t>(node->getOp());
            if (op > 63) {
                throw std::runtime_error("OpKind exceeds the 6-bit encoding");
            }
            writeU8(out, static_cast<uint8_t>((PK_UNARY << 6) | op));
            writeNode(out, node->getLeftChild());
            return;
        }

        case NODE_BINARY: {
            if (!node->getLeftChild() || !node->getRightChild()) {
                throw std::runtime_error("Binary node is missing a child");
            }
            const uint8_t op = static_cast<uint8_t>(node->getOp());
            if (op > 63) {
                throw std::runtime_error("OpKind exceeds the 6-bit encoding");
            }
            writeU8(out, static_cast<uint8_t>((PK_BINARY << 6) | op));
            writeNode(out, node->getLeftChild());
            writeNode(out, node->getRightChild());
            return;
        }

        default:
            throw std::runtime_error("Unknown NodeKind while writing");
    }
}

std::unique_ptr<Node> readNode(
    std::istream& in,
    uint32_t expectedNodes,
    uint32_t& nodesRead
) {
    if (nodesRead >= expectedNodes) {
        throw std::runtime_error("Node stream exceeded its declared node count");
    }

    const uint8_t code = readU8(in);
    ++nodesRead;

    const uint8_t kind = static_cast<uint8_t>(code >> 6);
    const uint8_t op = static_cast<uint8_t>(code & 0x3fu);

    switch (kind) {
        case PK_VARIABLE:
            if (op != 0) {
                throw std::runtime_error("Invalid variable-node encoding");
            }
            return std::unique_ptr<Node>(Node::makeVariable(0));

        case PK_VALUE: {
            if (op != 0) {
                throw std::runtime_error("Invalid value-node encoding");
            }
            const float value = readF32(in);
            return std::unique_ptr<Node>(
                Node::makeCoeffValue(static_cast<double>(value)));
        }

        case PK_UNARY: {
            std::unique_ptr<Node> child =
                readNode(in, expectedNodes, nodesRead);
            Node* result = Node::makeUnary(
                static_cast<OpKind>(op), child.get());
            child.release();
            return std::unique_ptr<Node>(result);
        }

        case PK_BINARY: {
            std::unique_ptr<Node> left =
                readNode(in, expectedNodes, nodesRead);
            std::unique_ptr<Node> right =
                readNode(in, expectedNodes, nodesRead);
            Node* result = Node::makeBinary(
                static_cast<OpKind>(op), left.get(), right.get());
            left.release();
            right.release();
            return std::unique_ptr<Node>(result);
        }

        default:
            throw std::runtime_error("Invalid packed node kind");
    }
}

} // namespace

void saveExprArrayAstBinary(
    const std::string& filename,
    const ExprArray& expressions,
    uint32_t blockSize,
    float tolerance,
    uint8_t majorVersion,
    uint8_t minorVersion
) {
    const size_t expressionCount = exprArraySize(expressions);
    if (expressionCount > std::numeric_limits<uint16_t>::max()) {
        throw std::runtime_error("Too many expressions for the file format");
    }

    // Validate all header values before creating/truncating the destination.
    const uint8_t versionByte = packVersion(majorVersion, minorVersion);
    if (blockSize == 0) {
        throw std::runtime_error("Block size must be greater than zero");
    }
    if (majorVersion != 1 && majorVersion != 2) {
        throw std::runtime_error("Unsupported AST binary format version");
    }
    const uint8_t flags = 0;

    std::ofstream out(filename, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Could not open file for writing: " + filename);
    }

    // Version 1 has the legacy compact 8-byte header and supports four block
    // sizes. Version 2 stores the block size directly in a 12-byte header.
    writeU8(out, versionByte);
    if (majorVersion == 1) {
        writeU8(out, packBlockInfo(blockSize, flags));
    } else {
        writeU8(out, flags);
        writeU32(out, blockSize);
    }
    writeF32(out, tolerance);
    writeU16(out, static_cast<uint16_t>(expressionCount));

    for (size_t i = 0; i < expressionCount; ++i) {
        const Node* root = exprArrayGet(expressions, i);
        if (!root) {
            throw std::runtime_error("Cannot save a null expression");
        }

        const uint32_t nodeCount = countAstNodes(root);
        writeU16(out, static_cast<uint16_t>(nodeCount));
        writeNode(out, root);

        if (!out) {
            throw std::runtime_error("Error while writing expression data");
        }
    }

    out.flush();
    if (!out) {
        throw std::runtime_error("Error finalizing AST binary file");
    }
}

ExprArray* loadExprArrayAstBinary(
    const std::string& filename,
    AstBinaryHeader* outHeader
) {
    std::ifstream in(filename, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Could not open file for reading: " + filename);
    }

    const uint8_t versionByte = readU8(in);
    const uint8_t majorVersion = static_cast<uint8_t>(versionByte >> 4);
    const uint8_t minorVersion = static_cast<uint8_t>(versionByte & 0x0fu);

    AstBinaryHeader header{};
    header.majorVersion = majorVersion;
    header.minorVersion = minorVersion;
    if (majorVersion == 1) {
        const uint8_t blockInfo = readU8(in);
        header.blockSize =
            blockSizeFromCode(static_cast<uint8_t>(blockInfo >> 4));
        header.flags = static_cast<uint8_t>(blockInfo & 0x0fu);
    } else if (majorVersion == 2) {
        header.flags = readU8(in);
        header.blockSize = readU32(in);
        if (header.blockSize == 0) {
            throw std::runtime_error("Invalid zero block size");
        }
    } else {
        throw std::runtime_error("Unsupported AST binary format version");
    }
    header.tolerance = readF32(in);
    header.expressionCount = readU16(in);

    std::unique_ptr<ExprArray> expressions(new ExprArray());
    for (uint32_t i = 0; i < header.expressionCount; ++i) {
        const uint32_t expectedNodes = readU16(in);
        if (expectedNodes == 0) {
            throw std::runtime_error("An expression cannot contain zero nodes");
        }

        uint32_t nodesRead = 0;
        std::unique_ptr<Node> root =
            readNode(in, expectedNodes, nodesRead);
        if (nodesRead != expectedNodes) {
            throw std::runtime_error(
                "Node count mismatch while loading expression");
        }
        exprArrayAdd(*expressions, std::move(root));
    }

    // Reject appended/corrupt data instead of silently ignoring it.
    if (in.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error("Unexpected trailing data after AST array");
    }
    if (in.bad()) {
        throw std::runtime_error("I/O error while reading AST binary file");
    }

    if (outHeader) {
        *outHeader = header;
    }
    return expressions.release();
}
