// Copyright (c) 2022-2026 Wuping Xin. All rights reserved.
// MIT License. See LICENSE.md for details.

#include "idl_generator.h"

#include <format>

namespace idlgen {

namespace {

std::string direction_string(const ParamDirection dir)
{
    switch (dir) {
        case ParamDirection::In:
            return "in";
        case ParamDirection::Out:
            return "out";
        case ParamDirection::InOut:
            return "in, out";
        case ParamDirection::OutRetval:
            return "out, retval";
    }
    return "in";
}

void generate_method(std::ostream &out, const MethodDecl &method)
{
    if (method.needs_transform) {
        // Non-HRESULT return → transform to HRESULT with [out, retval] param
        out << std::format("    HRESULT {}(", method.name);

        bool first = true;
        for (auto &param : method.params) {
            if (!first) out << ", ";
            first = false;
            out << std::format("[{}] {} {}", direction_string(param.direction),
                               param.idl_type, param.name);
        }

        // Add the return value as an [out, retval] parameter
        if (!first) out << ", ";
        out << std::format("[out, retval] {}* pResult", method.idl_return_type);

        out << ");\n";
    } else if (method.cpp_return_type == "void") {
        // void return → just emit params
        out << std::format("    void {}(", method.name);

        bool first = true;
        for (auto &param : method.params) {
            if (!first) out << ", ";
            first = false;
            out << std::format("[{}] {} {}", direction_string(param.direction),
                               param.idl_type, param.name);
        }

        out << ");\n";
    } else {
        // HRESULT return — standard COM pattern
        out << std::format("    HRESULT {}(", method.name);

        bool first = true;
        for (auto &param : method.params) {
            if (!first) out << ", ";
            first = false;
            out << std::format("[{}] {} {}", direction_string(param.direction),
                               param.idl_type, param.name);
        }

        out << ");\n";
    }
}

}// anonymous namespace

void generate_idl(std::ostream &out,
                  const std::vector<InterfaceDecl> &interfaces,
                  const GeneratorConfig &config)
{
    // Imports
    out << "import \"oaidl.idl\";\n";
    out << "import \"ocidl.idl\";\n";
    out << "\n";

    // Interface declarations
    for (auto &iface : interfaces) {
        out << std::format("[\n  object,\n  uuid({}),\n  pointer_default(unique)\n]\n",
                           iface.guid);
        out << std::format("interface {} : {}\n", iface.name, iface.base_interface);
        out << "{\n";

        for (auto &method : iface.methods) {
            generate_method(out, method);
        }

        out << "};\n\n";
    }

    // Optional library block
    if (config.generate_library_block && !config.library_name.empty()) {
        out << std::format("[\n  uuid({}),\n  version(1.0)\n]\n", config.library_uuid);
        out << std::format("library {}\n", config.library_name);
        out << "{\n";
        out << "    importlib(\"stdole2.tlb\");\n";

        for (auto &iface : interfaces) {
            out << std::format("    interface {};\n", iface.name);
        }

        out << "};\n";
    }
}

}// namespace idlgen
