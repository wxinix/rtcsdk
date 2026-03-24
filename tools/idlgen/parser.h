// Copyright (c) 2022-2026 Wuping Xin. All rights reserved.
// MIT License. See LICENSE.md for details.

#pragma once

#include "types.h"
#include <string>
#include <vector>

namespace idlgen {

// Parse preprocessed C++ source to extract interface declarations.
ParseResult<std::vector<InterfaceDecl>> parse_interfaces(
    const std::string &preprocessed_source,
    const std::filesystem::path &source_file);

// Parse a single method declaration line (for testing).
ParseResult<MethodDecl> parse_method(const std::string &line);

// Parse a parameter string like "/*[out,retval]*/ BSTR* result" (for testing).
ParseResult<ParamDecl> parse_param(const std::string &param_str);

// Apply direction heuristics to unannotated parameters.
void apply_direction_heuristics(MethodDecl &method);

}// namespace idlgen
