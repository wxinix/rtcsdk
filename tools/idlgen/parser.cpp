// Copyright (c) 2022-2026 Wuping Xin. All rights reserved.
// MIT License. See LICENSE.md for details.

#include "parser.h"

#include <algorithm>
#include <regex>

namespace idlgen {

namespace {

// Trim whitespace from both ends
std::string trim(const std::string &s)
{
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Find matching closing brace, accounting for nesting
size_t find_matching_brace(const std::string &src, size_t open_pos)
{
    int depth = 1;
    for (size_t i = open_pos + 1; i < src.size(); ++i) {
        if (src[i] == '{') ++depth;
        else if (src[i] == '}') {
            --depth;
            if (depth == 0) return i;
        }
    }
    return std::string::npos;
}

// Split parameter list by commas, respecting angle brackets and parentheses
std::vector<std::string> split_params(const std::string &param_list)
{
    std::vector<std::string> result;
    int angle_depth = 0;
    int paren_depth = 0;
    bool in_block_comment = false;
    size_t start = 0;

    for (size_t i = 0; i < param_list.size(); ++i) {
        char c = param_list[i];
        if (in_block_comment) {
            if (c == '*' && i + 1 < param_list.size() && param_list[i + 1] == '/') {
                in_block_comment = false;
                ++i;
            }
            continue;
        }
        if (c == '/' && i + 1 < param_list.size() && param_list[i + 1] == '*') {
            in_block_comment = true;
            ++i;
            continue;
        }
        if (c == '<') ++angle_depth;
        else if (c == '>')
            --angle_depth;
        else if (c == '(')
            ++paren_depth;
        else if (c == ')')
            --paren_depth;
        else if (c == ',' && angle_depth == 0 && paren_depth == 0) {
            result.emplace_back(trim(param_list.substr(start, i - start)));
            start = i + 1;
        }
    }

    if (auto last = trim(param_list.substr(start)); !last.empty())
        result.emplace_back(std::move(last));

    return result;
}

// Extract direction annotation from parameter string (e.g., "/*[out,retval]*/")
// Returns the direction and the param string with annotation removed
std::pair<ParamDirection, std::string> extract_annotation(const std::string &param)
{
    static const std::regex anno_re{R"(/\*\s*\[((?:in|out|retval)[,\s]*(?:in|out|retval)?)\s*\]\s*\*/)"};
    std::smatch match;

    if (std::regex_search(param, match, anno_re)) {
        const auto anno = match[1].str();
        const auto cleaned = param.substr(0, match.position()) + param.substr(match.position() + match.length());

        const auto trimmed_cleaned = trim(cleaned);
        if (anno.contains("out") && anno.contains("retval"))
            return {ParamDirection::OutRetval, trimmed_cleaned};
        if (anno.contains("in") && anno.contains("out"))
            return {ParamDirection::InOut, trimmed_cleaned};
        if (anno.contains("out"))
            return {ParamDirection::Out, trimmed_cleaned};
        return {ParamDirection::In, trimmed_cleaned};
    }

    return {ParamDirection::In, param};// default, will be overridden by heuristics
}

// Parse a type + name from a cleaned parameter like "const int* pVal"
// Returns {type, name}
std::pair<std::string, std::string> split_type_name(const std::string &param)
{
    auto trimmed = trim(param);
    if (trimmed.empty()) return {"", ""};

    // Find the last identifier (the parameter name)
    // Walk backwards from the end to find the name
    const auto last_space = trimmed.rfind(' ');

    size_t name_start = std::string::npos;

    if (last_space != std::string::npos) {
        // Check if there's something after the last space that's an identifier
        const auto after = trim(trimmed.substr(last_space + 1));
        if (!after.empty() && after[0] != '*' && after[0] != '&') {
            name_start = last_space + 1;
        }
    }

    // If last char before name is * or &, the name is everything after the last pointer/ref
    if (name_start == std::string::npos) {
        // No clear name — treat whole thing as type with no name
        return {trimmed, ""};
    }

    // Skip whitespace to find actual name start
    while (name_start < trimmed.size() && trimmed[name_start] == ' ')
        ++name_start;

    auto type_str = trim(trimmed.substr(0, name_start));
    auto name_str = trim(trimmed.substr(name_start));

    return {type_str, name_str};
}

bool has_annotation(const std::string &original_param)
{
    return original_param.contains("/*[");
}

}// anonymous namespace

ParseResult<ParamDecl> parse_param(const std::string &param_str)
{
    auto trimmed = trim(param_str);
    if (trimmed.empty()) {
        return std::unexpected(ErrorInfo{ParseError::MethodParseFailure, "Empty parameter"});
    }

    ParamDecl param;
    const bool had_annotation = has_annotation(trimmed);

    auto [direction, cleaned] = extract_annotation(trimmed);
    param.direction = direction;

    auto [type_str, name] = split_type_name(cleaned);
    param.cpp_type = type_str;
    param.name = name;
    param.is_const = type_str.starts_with("const ");

    // If no annotation was present, mark as In (heuristics applied later)
    if (!had_annotation) {
        param.direction = ParamDirection::In;// placeholder
    }

    return param;
}

void apply_direction_heuristics(MethodDecl &method)
{
    const bool returns_hresult = (method.cpp_return_type == "HRESULT");

    for (size_t i = 0; i < method.params.size(); ++i) {
        auto &p = method.params[i];
        const auto &type = p.cpp_type;

        // Skip if already annotated explicitly (direction was set from comment)
        // We check by looking for pointer patterns

        // const T* or const T& → [in]
        if (p.is_const) {
            p.direction = ParamDirection::In;
            continue;
        }

        // Primitive by value (no * or &) → [in]
        if (!type.contains('*') && !type.contains('&')) {
            p.direction = ParamDirection::In;
            continue;
        }

        // T** → [out]
        if (type.contains("**")) {
            p.direction = ParamDirection::Out;
            continue;
        }

        // Non-const T* with HRESULT return
        if (type.contains('*') && returns_hresult) {
            // Last pointer param → [out, retval]
            bool is_last_ptr = true;
            for (size_t j = i + 1; j < method.params.size(); ++j) {
                if (method.params[j].cpp_type.contains('*')) {
                    is_last_ptr = false;
                    break;
                }
            }
            p.direction = is_last_ptr ? ParamDirection::OutRetval : ParamDirection::InOut;
            continue;
        }

        // Non-const T* with non-HRESULT return → [in,out]
        if (type.contains('*')) {
            p.direction = ParamDirection::InOut;
        }
    }
}

ParseResult<MethodDecl> parse_method(const std::string &line)
{
    auto trimmed = trim(line);

    // Remove 'virtual' prefix
    if (trimmed.starts_with("virtual "))
        trimmed = trim(trimmed.substr(8));

    // Remove trailing '= 0;' or '= 0 ;'
    static const std::regex pure_virtual_re{R"(\s*=\s*0\s*;?\s*$)"};
    trimmed = std::regex_replace(trimmed, pure_virtual_re, "");

    // Remove qualifiers from the end: const, noexcept, override, STDMETHODCALLTYPE
    MethodDecl method;

    // Extract qualifiers from after the closing paren
    auto last_paren = trimmed.rfind(')');
    if (last_paren == std::string::npos) {
        return std::unexpected(ErrorInfo{ParseError::MethodParseFailure,
                                         "No closing parenthesis in: " + line});
    }

    const auto qualifiers = trimmed.substr(last_paren + 1);
    trimmed = trimmed.substr(0, last_paren + 1);

    method.is_const = qualifiers.contains("const");
    method.is_noexcept = qualifiers.contains("noexcept");

    // Now trimmed is: "RETURN_TYPE [CALLCONV] NAME(PARAMS)"
    // Remove STDMETHODCALLTYPE if present
    static const std::regex callconv_re{R"(\bSTDMETHODCALLTYPE\b)"};
    trimmed = trim(std::regex_replace(trimmed, callconv_re, ""));

    // Find the opening paren
    auto open_paren = trimmed.find('(');
    if (open_paren == std::string::npos) {
        return std::unexpected(ErrorInfo{ParseError::MethodParseFailure,
                                         "No opening parenthesis in: " + line});
    }

    // Extract parameter list (trimmed ends at ')', so strip it)
    auto params_str = trimmed.substr(open_paren + 1);
    if (params_str.ends_with(')'))
        params_str.pop_back();

    // Everything before open_paren is "RETURN_TYPE NAME"
    const auto prefix = trim(trimmed.substr(0, open_paren));

    // Split prefix into return type and method name
    // The method name is the last token
    const auto last_space = prefix.rfind(' ');
    if (last_space == std::string::npos) {
        return std::unexpected(ErrorInfo{ParseError::MethodParseFailure,
                                         "Cannot parse return type and name from: " + prefix});
    }

    method.cpp_return_type = trim(prefix.substr(0, last_space));
    method.name = trim(prefix.substr(last_space + 1));
    method.needs_transform = (method.cpp_return_type != "HRESULT" && method.cpp_return_type != "void");

    // Parse parameters
    for (const auto param_strings = split_params(trim(params_str)); const auto &ps : param_strings) {
        auto param_result = parse_param(ps);
        if (!param_result) return std::unexpected(param_result.error());
        method.params.emplace_back(std::move(*param_result));
    }

    // Apply heuristics to unannotated params
    apply_direction_heuristics(method);

    return method;
}

ParseResult<std::vector<InterfaceDecl>> parse_interfaces(
    const std::string &preprocessed_source,
    const std::filesystem::path &source_file)
{
    std::vector<InterfaceDecl> interfaces;

    static const std::regex iface_re( // NOLINT
        R"---((?:COM_INTERFACE|RTCSDK_DEFINE_INTERFACE)\s*\(\s*(\w+)\s*,\s*"([^"]+)"\s*\))---");
    static const std::regex iface_base_re( // NOLINT
        R"---((?:COM_INTERFACE_BASE|RTCSDK_DEFINE_INTERFACE_BASE)\s*\(\s*(\w+)\s*,\s*(\w+)\s*,\s*"([^"]+)"\s*\))---");

    auto process_interface = [&](const std::string &name,
                                 const std::string &base,
                                 const std::string &guid_str,
                                 size_t match_end) -> ParseResult<InterfaceDecl> {
        InterfaceDecl iface;
        iface.name = name;
        iface.base_interface = base;
        iface.source_file = source_file;

        // Strip braces from GUID
        std::string guid = guid_str;
        if (guid.starts_with("{")) guid = guid.substr(1);
        if (guid.ends_with("}")) guid = guid.substr(0, guid.size() - 1);
        iface.guid = guid;

        // Find the opening brace of the interface body
        auto brace_pos = preprocessed_source.find('{', match_end);
        if (brace_pos == std::string::npos) {
            return std::unexpected(ErrorInfo{ParseError::BraceMismatch,
                                             "No opening brace found for interface " + name});
        }

        auto close_brace = find_matching_brace(preprocessed_source, brace_pos);
        if (close_brace == std::string::npos) {
            return std::unexpected(ErrorInfo{ParseError::BraceMismatch,
                                             "No matching closing brace for interface " + name});
        }

        // Extract body and parse methods
        auto body = preprocessed_source.substr(brace_pos + 1, close_brace - brace_pos - 1);

        // Find virtual method declarations
        static const std::regex method_re{R"(virtual\s+.+?=\s*0\s*;)"};
        auto methods_begin = std::sregex_iterator(body.begin(), body.end(), method_re);
        auto methods_end = std::sregex_iterator();

        for (auto it = methods_begin; it != methods_end; ++it) {
            auto method_result = parse_method(it->str());
            if (!method_result) {
                return std::unexpected(method_result.error());
            }
            iface.methods.emplace_back(std::move(*method_result));
        }

        return iface;
    };

    // Process RTCSDK_DEFINE_INTERFACE
    {
        auto it = std::sregex_iterator(preprocessed_source.begin(), preprocessed_source.end(), iface_re);
        for (auto end = std::sregex_iterator(); it != end; ++it) {
            auto result = process_interface((*it)[1].str(), "IUnknown", (*it)[2].str(),
                                            it->position() + it->length());
            if (!result) return std::unexpected(result.error());
            interfaces.emplace_back(std::move(*result));
        }
    }

    // Process RTCSDK_DEFINE_INTERFACE_BASE
    {
        auto it = std::sregex_iterator(preprocessed_source.begin(), preprocessed_source.end(), iface_base_re);
        for (auto end = std::sregex_iterator(); it != end; ++it) {
            auto result = process_interface((*it)[1].str(), (*it)[2].str(), (*it)[3].str(),
                                            it->position() + it->length());
            if (!result) return std::unexpected(result.error());
            interfaces.emplace_back(std::move(*result));
        }
    }

    return interfaces;
}

}// namespace idlgen
