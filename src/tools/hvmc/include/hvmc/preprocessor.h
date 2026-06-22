#pragma once

#include <string>
#include <vector>

namespace hvmc {

// Preprocess C source code
// Handles: #include, #define, #undef, #if, #ifdef, #ifndef, #else, #elif, #endif, #error
// Returns the preprocessed source code
std::string preprocess(const std::string& source, 
                      const std::vector<std::string>& include_paths = {});

} // namespace hvmc
