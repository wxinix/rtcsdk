// Copyright (c) 2022-2026 Wuping Xin. All rights reserved.
// MIT License. See LICENSE.md for details.

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace idlgen {

// Embedded clang-format config tuned for maximum parseability (not style)
inline constexpr auto clang_format_config = R"(
BasedOnStyle: LLVM
ColumnLimit: 0
BinPackParameters: false
AllowShortFunctionsOnASingleLine: None
AllowAllParametersOfDeclarationOnNextLine: false
BreakBeforeBraces: Allman
IndentWidth: 2
AlignAfterOpenBracket: DontAlign
)";

struct PreprocessorConfig {
    std::filesystem::path clang_format_path;// empty = search PATH
    bool no_clang_format{false};
    bool verbose{false};
};

// Normalize C++ source for parsing.
// Phase A: optional clang-format pass.
// Phase B: strip comments (preserving /*[in]*/ etc.), attributes, preprocessor directives.
//          Collapse multiline, normalize whitespace.
std::string preprocess(const std::string &source,
                       const PreprocessorConfig &config,
                       std::vector<std::string> &warnings);

}// namespace idlgen
