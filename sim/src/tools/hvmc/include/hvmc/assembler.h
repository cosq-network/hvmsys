#pragma once
#include "types.h"

namespace hvmc {

// Parse assembly source into an ObjectFile
// Supports: label:, instruction [operands], .section, .global, .byte/.word/.quad, .ascii/.asciz
ObjectFile assemble(const std::string& source, const std::string& filename = "<stdin>");

// Parse assembly source and produce raw binary bytes (for .text section)
std::vector<u8> assemble_text(const std::string& source);

} // namespace hvmc
