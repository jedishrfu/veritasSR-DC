#include <fstream>
#include <stdexcept>
#include <string>

#include "../include/new_ast_binary_io.h"
#include "../include/expr_array.h"
#include "../include/ast_nodes.h"

static void writeFloat32(std::ostream& out, float v) {
  out.write(reinterpret_cast<const char*>(&v), sizeof(float));
  if (!out) {
    throw std::runtime_error("Error writing float32 output");
  }
}

void decompressExprFileToFp32(
    const std::string& compressedFilename,
    const std::string& outputFilename
) {
  AstBinaryHeader header;

  ExprArray expressions = loadExprArrayAstBinary(
      compressedFilename,
      &header
  );

  std::ofstream out(outputFilename, std::ios::binary);
  if (!out) {
    throw std::runtime_error("Could not open output file: " + outputFilename);
  }

  const uint16_t blockSize = header.blockSize;

  for (size_t exprIndex = 0; exprIndex < expressions.size(); ++exprIndex) {
    Node* expr = expressions.get(exprIndex);

    if (!expr) {
      throw std::runtime_error("Null expression during decompression");
    }

    for (uint16_t x = 0; x < blockSize; ++x) {
      double y = expr->eval(static_cast<double>(x));
      float yf = static_cast<float>(y);

      writeFloat32(out, yf);
    }
  }
}//
// Created by jamesmcardle on 7/6/26.
//
