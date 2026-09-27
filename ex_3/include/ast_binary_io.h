#pragma once

#include <cstdint>
#include <string>

// Defined by the AST/expressions component.
class ExprArray;

// Metadata stored in the binary file header.
struct AstBinaryHeader {
  uint8_t majorVersion;
  uint8_t minorVersion;
  uint32_t blockSize;
  uint8_t flags;
  float tolerance;
  uint16_t expressionCount;
};

// Saves the header followed by the serialized array of AST expressions.
// blockSize is the number of floating-point values reconstructed by each AST.
void saveExprArrayAstBinary(
    const std::string& filename,
    const ExprArray& expressions,
    uint32_t blockSize,
    float tolerance,
    uint8_t majorVersion,
    uint8_t minorVersion
);

// Loads the serialized AST array. The caller owns the returned pointer and
// must eventually delete it. outHeader may be nullptr when metadata is not
// needed.
ExprArray* loadExprArrayAstBinary(
    const std::string& filename,
    AstBinaryHeader* outHeader = nullptr
);
