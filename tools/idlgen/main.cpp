#include "idl_generator.h"
#include "parser.h"
#include "preprocessor.h"
#include "type_mapper.h"
#include "types.h"

#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <print>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct CliConfig {
    std::vector<fs::path> input_files;
    fs::path output_dir{"."};
    std::string library_name;
    std::string library_uuid;
    fs::path clang_format_path;
    bool no_clang_format{false};
    fs::path midl_path;
    bool run_midl{false};
    bool verbose{false};
};

void print_usage()
{
    std::println("Usage: idlgen [options] <input-files...>");
    std::println("");
    std::println("Options:");
    std::println("  -o <dir>                   Output directory (default: cwd)");
    std::println("  --library-name <name>      Library name for .tlb generation");
    std::println("  --library-uuid <uuid>      Library UUID for the library block");
    std::println("  --clang-format-path <path> Path to clang-format executable");
    std::println("  --no-clang-format          Disable clang-format preprocessing");
    std::println("  --midl                     Invoke MIDL to compile .idl to .tlb");
    std::println("  --midl-path <path>         Path to midl.exe");
    std::println("  --verbose                  Print parsing details and warnings");
    std::println("  --help                     Show this help");
}

std::expected<CliConfig, std::string> parse_args(int argc, char *argv[])
{
    CliConfig config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_usage();
            std::exit(0);
        } else if (arg == "-o" && i + 1 < argc) {
            config.output_dir = argv[++i];
        } else if (arg == "--library-name" && i + 1 < argc) {
            config.library_name = argv[++i];
        } else if (arg == "--library-uuid" && i + 1 < argc) {
            config.library_uuid = argv[++i];
        } else if (arg == "--clang-format-path" && i + 1 < argc) {
            config.clang_format_path = argv[++i];
        } else if (arg == "--no-clang-format") {
            config.no_clang_format = true;
        } else if (arg == "--midl") {
            config.run_midl = true;
        } else if (arg == "--midl-path" && i + 1 < argc) {
            config.midl_path = argv[++i];
        } else if (arg == "--verbose") {
            config.verbose = true;
        } else if (arg.starts_with("-")) {
            return std::unexpected(std::format("Unknown option: {}", arg));
        } else {
            config.input_files.emplace_back(arg);
        }
    }

    if (config.input_files.empty()) {
        return std::unexpected(std::string{"No input files specified"});
    }

    return config;
}

int run_midl(const fs::path &idl_file, const fs::path &midl_path, bool verbose)
{
    std::string midl_exe = midl_path.empty() ? "midl.exe" : midl_path.string();
    std::string cmd = std::format("\"{}\" \"{}\"", midl_exe, idl_file.string());

    if (verbose) {
        std::println(stderr, "Running: {}", cmd);
    }

    return std::system(cmd.c_str());
}

int main(int argc, char *argv[])
{
    auto config_result = parse_args(argc, argv);
    if (!config_result) {
        std::println(stderr, "Error: {}", config_result.error());
        print_usage();
        return 1;
    }

    auto &config = *config_result;
    std::vector<idlgen::InterfaceDecl> all_interfaces;
    std::vector<idlgen::Warning> all_warnings;

    idlgen::PreprocessorConfig pp_config{
        .clang_format_path = config.clang_format_path,
        .no_clang_format = config.no_clang_format,
        .verbose = config.verbose,
    };

    // Process each input file
    for (auto &input_file : config.input_files) {
        if (!fs::exists(input_file)) {
            std::println(stderr, "Error: File not found: {}", input_file.string());
            return 1;
        }

        // Read file
        std::ifstream in{input_file};
        std::string source{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
        in.close();

        if (config.verbose) {
            std::println(stderr, "Processing: {}", input_file.string());
        }

        // Preprocess
        std::vector<std::string> pp_warnings;
        auto preprocessed = idlgen::preprocess(source, pp_config, pp_warnings);

        for (auto &w : pp_warnings) {
            if (config.verbose) {
                std::println(stderr, "  [preprocess] {}", w);
            }
        }

        // Parse
        auto parse_result = idlgen::parse_interfaces(preprocessed, input_file);
        if (!parse_result) {
            std::println(stderr, "Error parsing {}: {}", input_file.string(), parse_result.error().message);
            return 1;
        }

        // Type mapping
        for (auto &iface : *parse_result) {
            idlgen::map_interface_types(iface, all_warnings);
            all_interfaces.emplace_back(std::move(iface));
        }
    }

    // Print warnings
    for (auto &w : all_warnings) {
        std::println(stderr, "Warning [{}]: {}", w.source, w.message);
    }

    if (all_interfaces.empty()) {
        std::println(stderr, "No interfaces found in input files.");
        return 1;
    }

    if (config.verbose) {
        std::println(stderr, "Found {} interface(s)", all_interfaces.size());
    }

    // Determine output filename
    auto stem = config.input_files[0].stem().string();
    auto output_file = config.output_dir / (stem + ".idl");

    // Ensure output directory exists
    fs::create_directories(config.output_dir);

    // Generate IDL
    idlgen::GeneratorConfig gen_config{
        .library_name = config.library_name.empty() ? stem : config.library_name,
        .library_uuid = config.library_uuid,
        .generate_library_block = !config.library_uuid.empty(),
    };

    {
        std::ofstream out{output_file};
        if (!out) {
            std::println(stderr, "Error: Cannot open output file: {}", output_file.string());
            return 1;
        }
        idlgen::generate_idl(out, all_interfaces, gen_config);
    }

    std::println("Generated: {}", output_file.string());

    // Optionally run MIDL
    if (config.run_midl) {
        int ret = run_midl(output_file, config.midl_path, config.verbose);
        if (ret != 0) {
            std::println(stderr, "MIDL compilation failed with exit code {}", ret);
            return 1;
        }
        std::println("MIDL compilation successful");
    }

    return 0;
}
