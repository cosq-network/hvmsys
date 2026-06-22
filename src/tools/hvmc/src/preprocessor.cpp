#include "hvmc/preprocessor.h"
#include <sstream>
#include <unordered_map>
#include <vector>
#include <cctype>
#include <stdexcept>
#include <fstream>

namespace hvmc {

// Forward declarations
static std::string read_file(const std::string& filename, 
                            const std::vector<std::string>& include_paths);
static bool evaluate_constant(const std::string& expr, 
                            const std::unordered_map<std::string, std::string>& defines);

// Preprocessor state
struct PreprocessorState {
    std::stringstream output;
    std::vector<std::string> include_paths;
    std::unordered_map<std::string, std::string> defines;
    std::vector<std::string> include_stack; // Track included files to detect cycles
    bool in_conditional = false;
    bool conditional_active = true;
};

// Forward declaration for process_line (needs PreprocessorState)
static std::string process_line(const std::string& line, PreprocessorState& state);

// Trim whitespace from both ends
static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos || end == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

// Check if a character is a valid identifier character
static bool is_ident_char(char c) {
    return std::isalnum(c) || c == '_';
}

// Split a string by whitespace
static std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> result;
    std::istringstream iss(s);
    std::string token;
    while (iss >> token) {
        result.push_back(token);
    }
    return result;
}

// Expand a macro (simple text substitution for now)
static std::string expand_macro(const std::string& text, 
                               const std::unordered_map<std::string, std::string>& defines) {
    std::string result = text;
    for (const auto& [name, value] : defines) {
        size_t pos = 0;
        while ((pos = result.find(name, pos)) != std::string::npos) {
            // Check if it's a whole word
            if ((pos == 0 || !is_ident_char(result[pos - 1])) &&
                (pos + name.length() == result.length() || !is_ident_char(result[pos + name.length()]))) {
                result.replace(pos, name.length(), value);
                pos += value.length();
            } else {
                pos += name.length();
            }
        }
    }
    return result;
}

// Internal preprocess function
static std::string preprocess_internal(const std::string& source, PreprocessorState& state) {
    std::istringstream iss(source);
    std::string line;
    
    while (std::getline(iss, line)) {
        std::string processed = process_line(line, state);
        if (!processed.empty() && !(state.in_conditional && !state.conditional_active)) {
            state.output << processed;
        }
    }
    
    return state.output.str();
}

// Process a single line of preprocessor input
static std::string process_line(const std::string& line, PreprocessorState& state) {
    std::string trimmed = trim(line);
    
    // Handle empty lines
    if (trimmed.empty()) {
        return line;
    }
    
    // Check for preprocessor directive
    if (trimmed[0] == '#') {
        // Extract the directive
        std::string directive = trimmed.substr(1);
        directive = trim(directive);
        
        auto args = split(directive);
        if (args.empty()) return "";
        
        std::string cmd = args[0];
        
        // Handle #define
        if (cmd == "define") {
            if (args.size() >= 2) {
                std::string name = args[1];
                std::string value = "";
                if (args.size() >= 3) {
                    // Reconstruct value from remaining args
                    for (size_t i = 2; i < args.size(); i++) {
                        if (i > 2) value += " ";
                        value += args[i];
                    }
                }
                state.defines[name] = value;
            }
            return "";
        }
        
        // Handle #undef
        if (cmd == "undef") {
            if (args.size() >= 2) {
                state.defines.erase(args[1]);
            }
            return "";
        }
        
        // Handle #include
        if (cmd == "include") {
            if (args.size() >= 2) {
                std::string filepath = args[1];
                // Remove quotes if present
                if (filepath.front() == '"' && filepath.back() == '"') {
                    filepath = filepath.substr(1, filepath.size() - 2);
                } else if (filepath.front() == '<' && filepath.back() == '>') {
                    filepath = filepath.substr(1, filepath.size() - 2);
                }
                
                // Check for include cycles
                for (const auto& included : state.include_stack) {
                    if (included == filepath) {
                        throw std::runtime_error("Circular include: " + filepath);
                    }
                }
                
                // Try to read the file
                std::string content = read_file(filepath, state.include_paths);
                state.include_stack.push_back(filepath);
                // Create a child state for processing the included file
                PreprocessorState child_state;
                child_state.include_paths = state.include_paths;
                child_state.defines = state.defines;
                child_state.include_stack = state.include_stack;
                child_state.in_conditional = state.in_conditional;
                child_state.conditional_active = state.conditional_active;
                std::string processed = preprocess_internal(content, child_state);
                state.include_stack.pop_back();
                state.defines = child_state.defines; // Preserve defines from included file
                return processed + "\n";
            }
            return "";
        }
        
        // Handle #if, #ifdef, #ifndef
        if (cmd == "if" || cmd == "ifdef" || cmd == "ifndef") {
            bool condition = false;
            if (cmd == "if") {
                if (args.size() >= 2) {
                    // Simple constant expression evaluation
                    condition = evaluate_constant(args[1], state.defines);
                }
            } else if (cmd == "ifdef") {
                if (args.size() >= 2) {
                    condition = state.defines.find(args[1]) != state.defines.end();
                }
            } else if (cmd == "ifndef") {
                if (args.size() >= 2) {
                    condition = state.defines.find(args[1]) == state.defines.end();
                }
            }
            
            state.in_conditional = true;
            state.conditional_active = state.conditional_active && condition;
            return "";
        }
        
        // Handle #else
        if (cmd == "else") {
            if (state.in_conditional) {
                state.conditional_active = !state.conditional_active;
            }
            return "";
        }
        
        // Handle #elif
        if (cmd == "elif") {
            if (state.in_conditional) {
                bool condition = false;
                if (args.size() >= 2) {
                    condition = evaluate_constant(args[1], state.defines);
                }
                state.conditional_active = !state.conditional_active && condition;
            }
            return "";
        }
        
        // Handle #endif
        if (cmd == "endif") {
            state.in_conditional = false;
            state.conditional_active = true;
            return "";
        }
        
        // Handle #error
        if (cmd == "error") {
            std::string msg = "Preprocessor error";
            if (args.size() >= 2) {
                msg = args[1];
            }
            throw std::runtime_error(msg);
        }
        
        // Handle #pragma (ignore for now)
        if (cmd == "pragma") {
            return "";
        }
        
        // Unknown directive - pass through
        return line;
    }
    
    // Not a directive - expand macros if conditional is active
    if (!state.conditional_active) {
        return "";
    }
    
    return expand_macro(line, state.defines);
}

// Evaluate a constant expression (simple version)
static bool evaluate_constant(const std::string& expr, 
                            const std::unordered_map<std::string, std::string>& defines) {
    // Check if it's a defined macro
    auto it = defines.find(expr);
    if (it != defines.end()) {
        return !it->second.empty(); // Non-empty value is true
    }
    
    // Try to parse as integer
    try {
        int val = std::stoi(expr);
        return val != 0;
    } catch (...) {
        return false; // Unknown is false
    }
}

// Read a file with include path search
static std::string read_file(const std::string& filename, 
                            const std::vector<std::string>& include_paths) {
    // Try direct path first
    {
        std::ifstream f(filename, std::ios::binary);
        if (f) {
            std::stringstream ss;
            ss << f.rdbuf();
            return ss.str();
        }
    }
    
    // Try with include paths
    for (const auto& path : include_paths) {
        std::string full_path = path;
        if (!full_path.empty() && full_path.back() != '/') {
            full_path += '/';
        }
        full_path += filename;
        
        std::ifstream f(full_path, std::ios::binary);
        if (f) {
            std::stringstream ss;
            ss << f.rdbuf();
            return ss.str();
        }
    }
    
    throw std::runtime_error("Cannot open file: " + filename);
}

// Main preprocessor function
std::string preprocess(const std::string& source, 
                      const std::vector<std::string>& include_paths) {
    PreprocessorState state;
    state.include_paths = include_paths;
    return preprocess_internal(source, state);
}

} // namespace hvmc
