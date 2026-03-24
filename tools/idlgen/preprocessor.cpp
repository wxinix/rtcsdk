// Copyright (c) 2022-2026 Wuping Xin. All rights reserved.
// MIT License. See LICENSE.md for details.

#include "preprocessor.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace idlgen {

namespace {

// Try to find clang-format executable
std::filesystem::path find_clang_format(const std::filesystem::path &hint)
{
    if (!hint.empty() && std::filesystem::exists(hint))
        return hint;

    // Try common locations on Windows
    char *path_env = nullptr;
    size_t path_len = 0;
    if (_dupenv_s(&path_env, &path_len, "PATH") != 0 || !path_env) return {};

    const std::string path_str{path_env};
    free(path_env);
    std::istringstream paths{path_str};
    std::string dir;

    while (std::getline(paths, dir, ';')) {
        if (const auto candidate = std::filesystem::path{dir} / "clang-format.exe"; std::filesystem::exists(candidate))
            return candidate;
    }

    return {};
}

// Run clang-format on source text, return formatted text or original on failure
std::string run_clang_format(const std::string &source,
                             const std::filesystem::path &clang_format_exe,
                             std::vector<std::string> &warnings)
{
    namespace fs = std::filesystem;

    const auto temp_dir = fs::temp_directory_path();
    const auto input_file = temp_dir / "idlgen_input.tmp.h";
    const auto config_file = temp_dir / ".clang-format";
    const auto output_file = temp_dir / "idlgen_output.tmp.h";

    // Write config
    {
        std::ofstream cfg{config_file};
        cfg << clang_format_config;
    }

    // Write input
    {
        std::ofstream in{input_file};
        in << source;
    }

    // Run clang-format
    std::string cmd = "\"" + clang_format_exe.string() + "\" "
        + "--style=file:\"" + config_file.string() + "\" "
        + "\"" + input_file.string() + "\" "
        + "> \"" + output_file.string() + "\" 2>&1";

    const int ret = std::system(cmd.c_str());

    // Cleanup config and input
    std::error_code ec;
    fs::remove(config_file, ec);
    fs::remove(input_file, ec);

    if (ret != 0) {
        warnings.emplace_back("clang-format returned non-zero exit code, falling back to manual normalization");
        fs::remove(output_file, ec);
        return source;
    }

    // Read output
    std::ifstream out{output_file};
    std::string result{std::istreambuf_iterator<char>{out}, std::istreambuf_iterator<char>{}};
    out.close();
    fs::remove(output_file, ec);

    return result;
}

// Check if a comment is a direction annotation we should preserve
bool is_direction_annotation(const std::string &comment)
{
    return comment.contains("[in]") || comment.contains("[out]")
        || comment.contains("[in,out]") || comment.contains("[out,retval]");
}

// Strip comments, preserving direction annotations inline
std::string strip_comments(const std::string &source)
{
    std::string result;
    result.reserve(source.size());

    size_t i = 0;
    while (i < source.size()) {
        if (i + 1 < source.size() && source[i] == '/' && source[i + 1] == '/') {
            // Line comment — skip to end of line
            while (i < source.size() && source[i] != '\n')
                ++i;
        } else if (i + 1 < source.size() && source[i] == '/' && source[i + 1] == '*') {
            // Block comment — check if it's a direction annotation
            const size_t start = i;
            i += 2;
            while (i + 1 < source.size() && !(source[i] == '*' && source[i + 1] == '/'))
                ++i;
            if (i + 1 < source.size())
                i += 2;// skip */

            if (const auto comment = source.substr(start, i - start); is_direction_annotation(comment)) {
                result += comment;// preserve
            } else {
                result += ' ';// replace with space
            }
        } else {
            result += source[i];
            ++i;
        }
    }

    return result;
}

// Strip C++ attributes like [[nodiscard]], [[maybe_unused]]
std::string strip_attributes(const std::string &source)
{
    // Match [[...]] attributes
    static const std::regex attr_re{R"(\[\[[^\]]*\]\])"};
    return std::regex_replace(source, attr_re, "");
}

// Strip preprocessor directives inside interface bodies
std::string strip_preprocessor(const std::string &source)
{
    std::string result;
    std::istringstream stream{source};
    std::string line;

    while (std::getline(stream, line)) {
        // Skip lines starting with # (after whitespace)
        if (const auto pos = line.find_first_not_of(" \t"); pos != std::string::npos && line[pos] == '#') {
            result += '\n';
            continue;
        }
        result += line + '\n';
    }

    return result;
}

// Collapse multiple whitespace into single spaces, trim lines
std::string normalize_whitespace(const std::string &source)
{
    std::string result;
    result.reserve(source.size());

    bool in_space = false;
    for (char c : source) {
        if (c == '\n') {
            result += '\n';
            in_space = false;
        } else if (c == ' ' || c == '\t' || c == '\r') {
            if (!in_space) {
                result += ' ';
                in_space = true;
            }
        } else {
            result += c;
            in_space = false;
        }
    }

    return result;
}

}// anonymous namespace

std::string preprocess(const std::string &source,
                       const PreprocessorConfig &config,
                       std::vector<std::string> &warnings)
{
    std::string result = source;

    // Phase A: optional clang-format
    if (!config.no_clang_format) {
        if (const auto cf_path = find_clang_format(config.clang_format_path); !cf_path.empty()) {
            if (config.verbose) {
                warnings.emplace_back("Using clang-format: " + cf_path.string());
            }
            result = run_clang_format(result, cf_path, warnings);
        } else if (config.verbose) {
            warnings.emplace_back("clang-format not found, using manual normalization");
        }
    }

    // Phase B: custom cleanup
    result = strip_comments(result);
    result = strip_attributes(result);
    result = strip_preprocessor(result);
    result = normalize_whitespace(result);

    return result;
}

}// namespace idlgen
