#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace idlgen {

enum class ParamDirection
{
    In,
    Out,
    InOut,
    OutRetval
};

struct ParamDecl {
    std::string cpp_type;
    std::string idl_type;
    std::string name;
    ParamDirection direction{ParamDirection::In};
    bool is_const{false};
};

struct MethodDecl {
    std::string name;
    std::string cpp_return_type;
    std::string idl_return_type;
    std::vector<ParamDecl> params;
    bool is_const{false};
    bool is_noexcept{false};
    bool needs_transform{false};// true if non-HRESULT return → auto-transform
};

struct InterfaceDecl {
    std::string name;
    std::string guid;          // stripped of braces, e.g. "AB9A7AF1-6792-..."
    std::string base_interface;// "IUnknown" or custom base
    std::vector<MethodDecl> methods;
    std::filesystem::path source_file;
};

enum class ParseError
{
    FileReadError,
    MacroNotFound,
    MalformedMacro,
    BraceMismatch,
    MethodParseFailure,
};

struct ErrorInfo {
    ParseError code;
    std::string message;
};

template<typename T>
using ParseResult = std::expected<T, ErrorInfo>;

struct Warning {
    std::string source;
    std::string message;
};

}// namespace idlgen
