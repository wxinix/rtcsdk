#pragma once

#include "types.h"
#include <ostream>
#include <string>
#include <vector>

namespace idlgen {

struct GeneratorConfig {
    std::string library_name;
    std::string library_uuid;
    bool generate_library_block{false};
};

// Generate IDL text for a set of interfaces.
void generate_idl(std::ostream &out,
                  const std::vector<InterfaceDecl> &interfaces,
                  const GeneratorConfig &config);

}// namespace idlgen
