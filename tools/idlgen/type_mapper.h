// Copyright (c) 2022-2026 Wuping Xin. All rights reserved.
// MIT License. See LICENSE.md for details.

#pragma once

#include "types.h"
#include <string>
#include <vector>

namespace idlgen {

// Map a C++ type to its IDL equivalent.
// Returns the mapped type and appends warnings for unmappable types.
std::string map_type(const std::string &cpp_type, std::vector<Warning> &warnings);

// Apply type mapping to all parameters and return types in an interface.
void map_interface_types(InterfaceDecl &iface, std::vector<Warning> &warnings);

}// namespace idlgen
