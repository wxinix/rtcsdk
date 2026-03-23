#include "type_mapper.h"

#include <unordered_map>

namespace idlgen {

namespace {

const std::unordered_map<std::string, std::string> &type_map()
{
    static const std::unordered_map<std::string, std::string> map{
        // Integer types
        {"int", "long"},
        {"unsigned int", "unsigned long"},
        {"long", "long"},
        {"unsigned long", "unsigned long"},
        {"short", "short"},
        {"unsigned short", "unsigned short"},
        {"char", "char"},
        {"unsigned char", "unsigned char"},
        {"signed char", "char"},
        {"__int64", "hyper"},
        {"long long", "hyper"},
        {"unsigned __int64", "unsigned hyper"},
        {"unsigned long long", "unsigned hyper"},

        // Windows integer types
        {"LONG", "long"},
        {"ULONG", "unsigned long"},
        {"DWORD", "unsigned long"},
        {"WORD", "unsigned short"},
        {"BYTE", "byte"},
        {"INT", "long"},
        {"UINT", "unsigned long"},
        {"BOOL", "long"},
        {"LONG_PTR", "long"},
        {"ULONG_PTR", "unsigned long"},
        {"INT_PTR", "long"},
        {"UINT_PTR", "unsigned long"},
        {"SIZE_T", "unsigned long"},

        // Floating point
        {"float", "float"},
        {"double", "double"},

        // COM standard types
        {"HRESULT", "HRESULT"},
        {"BSTR", "BSTR"},
        {"VARIANT", "VARIANT"},
        {"VARIANT_BOOL", "VARIANT_BOOL"},
        {"SAFEARRAY", "SAFEARRAY"},
        {"CY", "CY"},
        {"DATE", "DATE"},

        // GUID types
        {"GUID", "GUID"},
        {"IID", "GUID"},
        {"CLSID", "GUID"},
        {"REFIID", "GUID"},
        {"REFCLSID", "GUID"},
        {"REFGUID", "GUID"},

        // String types
        {"LPWSTR", "LPWSTR"},
        {"LPCWSTR", "LPCWSTR"},
        {"LPSTR", "LPSTR"},
        {"LPCSTR", "LPCSTR"},
        {"wchar_t", "wchar_t"},

        // Void
        {"void", "void"},

        // COM interfaces (pass through)
        {"IUnknown", "IUnknown"},
        {"IDispatch", "IDispatch"},
        {"IClassFactory", "IClassFactory"},
        {"IStream", "IStream"},
        {"IStorage", "IStorage"},
        {"IMoniker", "IMoniker"},
        {"IEnumVARIANT", "IEnumVARIANT"},
    };

    return map;
}

// Strip leading 'const ' and trailing pointer/ref indicators for base type lookup
std::string extract_base_type(const std::string &cpp_type)
{
    std::string base = cpp_type;

    // Remove const
    if (base.starts_with("const "))
        base = base.substr(6);

    // Remove trailing whitespace, *, &
    while (!base.empty() && (base.back() == '*' || base.back() == '&' || base.back() == ' '))
        base.pop_back();

    return base;
}

// Count pointer indirection levels
int pointer_depth(const std::string &cpp_type)
{
    int depth = 0;
    for (char c : cpp_type) {
        if (c == '*') ++depth;
    }
    return depth;
}

bool has_reference(const std::string &cpp_type)
{
    return cpp_type.contains('&');
}

}// anonymous namespace

std::string map_type(const std::string &cpp_type, std::vector<Warning> &warnings)
{
    auto trimmed = cpp_type;

    // Trim
    auto start = trimmed.find_first_not_of(" \t");
    if (start != std::string::npos)
        trimmed = trimmed.substr(start);
    auto end = trimmed.find_last_not_of(" \t");
    if (end != std::string::npos)
        trimmed = trimmed.substr(0, end + 1);

    if (trimmed.empty()) return "void";

    const auto base = extract_base_type(trimmed);
    const int ptrs = pointer_depth(trimmed);
    const bool is_ref = has_reference(trimmed);

    // Look up base type
    const auto &map = type_map();
    auto it = map.find(base);

    std::string idl_base;
    if (it != map.end()) {
        idl_base = it->second;
    } else {
        // Unknown type — could be a user-defined interface, pass through with warning
        if (base.starts_with("I") && base.size() > 1 && std::isupper(base[1])) {
            // Likely a COM interface name, pass through
            idl_base = base;
        } else if (base.starts_with("std::")) {
            warnings.emplace_back(cpp_type, "C++ standard library type '" + base + "' has no IDL equivalent");
            idl_base = base;// pass through, will fail in MIDL
        } else {
            warnings.emplace_back(cpp_type, "Unknown type '" + base + "', passing through as-is");
            idl_base = base;
        }
    }

    // Reconstruct with pointer indirection
    std::string result = idl_base;
    for (int i = 0; i < ptrs; ++i)
        result += '*';

    // References become pointers in IDL
    if (is_ref)
        result += '*';

    return result;
}

void map_interface_types(InterfaceDecl &iface, std::vector<Warning> &warnings)
{
    for (auto &method : iface.methods) {
        method.idl_return_type = map_type(method.cpp_return_type, warnings);

        for (auto &param : method.params) {
            param.idl_type = map_type(param.cpp_type, warnings);
        }
    }
}

}// namespace idlgen
