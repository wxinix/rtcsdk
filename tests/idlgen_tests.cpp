// Copyright (c) 2022-2026 Wuping Xin. All rights reserved.
// MIT License. See LICENSE.md for details.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "../tools/idlgen/idl_generator.h"
#include "../tools/idlgen/parser.h"
#include "../tools/idlgen/preprocessor.h"
#include "../tools/idlgen/type_mapper.h"

#include <sstream>

using namespace idlgen;

TEST_SUITE_BEGIN("Preprocessor");

TEST_CASE("strip comments preserves direction annotations")
{
    std::string source = R"(
    virtual HRESULT GetData(/*[out,retval]*/ BSTR* result) = 0;
    // This is a line comment
    virtual void SetData(/* regular comment */ int x) = 0;
  )";

    PreprocessorConfig config{.no_clang_format = true};
    std::vector<std::string> warnings;
    auto result = preprocess(source, config, warnings);

    CHECK(result.contains("/*[out,retval]*/"));
    CHECK_FALSE(result.contains("line comment"));
    CHECK_FALSE(result.contains("regular comment"));
}

TEST_CASE("strip attributes removes [[nodiscard]]")
{
    std::string source = R"(
    [[nodiscard]] virtual int get_answer() const noexcept = 0;
  )";

    PreprocessorConfig config{.no_clang_format = true};
    std::vector<std::string> warnings;
    auto result = preprocess(source, config, warnings);

    CHECK_FALSE(result.contains("[[nodiscard]]"));
    CHECK(result.contains("virtual int get_answer"));
}

TEST_SUITE_END;

TEST_SUITE_BEGIN("Parser");

TEST_CASE("parse simple method")
{
    auto result = parse_method("virtual int sum(int a, int b) const noexcept = 0;");
    REQUIRE(result.has_value());

    auto &m = *result;
    CHECK_EQ(m.name, "sum");
    CHECK_EQ(m.cpp_return_type, "int");
    CHECK(m.is_const);
    CHECK(m.is_noexcept);
    CHECK(m.needs_transform);// non-HRESULT return
    CHECK_EQ(m.params.size(), 2);
    CHECK_EQ(m.params[0].name, "a");
    CHECK_EQ(m.params[1].name, "b");
}

TEST_CASE("parse HRESULT method with annotations")
{
    auto result = parse_method("virtual HRESULT GetData(/*[in]*/ int id, /*[out,retval]*/ BSTR* pResult) = 0;");
    REQUIRE(result.has_value());

    auto &m = *result;
    CHECK_EQ(m.name, "GetData");
    CHECK_EQ(m.cpp_return_type, "HRESULT");
    CHECK_FALSE(m.needs_transform);
    CHECK_EQ(m.params.size(), 2);
    CHECK_EQ(m.params[0].direction, ParamDirection::In);
    CHECK_EQ(m.params[1].direction, ParamDirection::OutRetval);
}

TEST_CASE("parse COM_INTERFACE")
{
    std::string source = R"(
COM_INTERFACE(ISampleInterface, "{AB9A7AF1-6792-4D0A-83BE-8252A8432B45}")
{
  virtual int sum(int a, int b) const noexcept = 0;
  virtual int get_answer() const noexcept = 0;
};
)";

    auto result = parse_interfaces(source, "test.h");
    REQUIRE(result.has_value());
    REQUIRE_EQ(result->size(), 1);

    auto &iface = (*result)[0];
    CHECK_EQ(iface.name, "ISampleInterface");
    CHECK_EQ(iface.guid, "AB9A7AF1-6792-4D0A-83BE-8252A8432B45");
    CHECK_EQ(iface.base_interface, "IUnknown");
    CHECK_EQ(iface.methods.size(), 2);
    CHECK_EQ(iface.methods[0].name, "sum");
    CHECK_EQ(iface.methods[1].name, "get_answer");
}

TEST_CASE("direction heuristics")
{
    SUBCASE("const pointer is [in]")
    {
        auto result = parse_method("virtual HRESULT Foo(const int* pVal) = 0;");
        REQUIRE(result.has_value());
        CHECK_EQ(result->params[0].direction, ParamDirection::In);
    }

    SUBCASE("double pointer is [out]")
    {
        auto result = parse_method("virtual HRESULT Foo(IUnknown** ppUnk) = 0;");
        REQUIRE(result.has_value());
        CHECK_EQ(result->params[0].direction, ParamDirection::Out);
    }

    SUBCASE("last non-const pointer with HRESULT return is [out,retval]")
    {
        auto result = parse_method("virtual HRESULT Foo(int id, BSTR* pResult) = 0;");
        REQUIRE(result.has_value());
        CHECK_EQ(result->params[0].direction, ParamDirection::In);
        CHECK_EQ(result->params[1].direction, ParamDirection::OutRetval);
    }

    SUBCASE("value params are [in]")
    {
        auto result = parse_method("virtual HRESULT Foo(int a, float b) = 0;");
        REQUIRE(result.has_value());
        CHECK_EQ(result->params[0].direction, ParamDirection::In);
        CHECK_EQ(result->params[1].direction, ParamDirection::In);
    }
}

TEST_SUITE_END;

TEST_SUITE_BEGIN("Type Mapper");

TEST_CASE("basic type mapping")
{
    std::vector<Warning> warnings;

    CHECK_EQ(map_type("int", warnings), "long");
    CHECK_EQ(map_type("DWORD", warnings), "unsigned long");
    CHECK_EQ(map_type("BSTR", warnings), "BSTR");
    CHECK_EQ(map_type("HRESULT", warnings), "HRESULT");
    CHECK_EQ(map_type("float", warnings), "float");
    CHECK_EQ(map_type("void", warnings), "void");
    CHECK(warnings.empty());
}

TEST_CASE("pointer type mapping")
{
    std::vector<Warning> warnings;

    CHECK_EQ(map_type("int*", warnings), "long*");
    CHECK_EQ(map_type("BSTR*", warnings), "BSTR*");
    CHECK_EQ(map_type("IUnknown**", warnings), "IUnknown**");
    CHECK(warnings.empty());
}

TEST_CASE("unmappable type generates warning")
{
    std::vector<Warning> warnings;

    auto result = map_type("std::wstring", warnings);
    CHECK_FALSE(warnings.empty());
    CHECK(warnings[0].message.contains("std::wstring"));
}

TEST_SUITE_END;

TEST_SUITE_BEGIN("IDL Generator");

TEST_CASE("generate IDL for ISampleInterface")
{
    InterfaceDecl iface;
    iface.name = "ISampleInterface";
    iface.guid = "AB9A7AF1-6792-4D0A-83BE-8252A8432B45";
    iface.base_interface = "IUnknown";

    MethodDecl sum;
    sum.name = "sum";
    sum.cpp_return_type = "int";
    sum.idl_return_type = "long";
    sum.needs_transform = true;
    sum.params = {
        {.cpp_type = "int", .idl_type = "long", .name = "a", .direction = ParamDirection::In},
        {.cpp_type = "int", .idl_type = "long", .name = "b", .direction = ParamDirection::In},
    };
    iface.methods.push_back(sum);

    MethodDecl get_answer;
    get_answer.name = "get_answer";
    get_answer.cpp_return_type = "int";
    get_answer.idl_return_type = "long";
    get_answer.needs_transform = true;
    iface.methods.push_back(get_answer);

    std::ostringstream out;
    generate_idl(out, {iface}, {});
    auto idl = out.str();

    CHECK(idl.contains("import \"oaidl.idl\""));
    CHECK(idl.contains("uuid(AB9A7AF1-6792-4D0A-83BE-8252A8432B45)"));
    CHECK(idl.contains("interface ISampleInterface : IUnknown"));
    CHECK(idl.contains("HRESULT sum([in] long a, [in] long b, [out, retval] long* pResult)"));
    CHECK(idl.contains("HRESULT get_answer([out, retval] long* pResult)"));
}

TEST_SUITE_END;
