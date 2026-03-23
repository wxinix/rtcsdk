#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#define RTCSDK_COM_NO_LEAK_DETECTION

#include <rtcsdk/factory.h>
#include <rtcsdk/interfaces.h>
#include <rtcsdk/vector.h>

RTCSDK_DEFINE_INTERFACE(ISampleInterface, "{AB9A7AF1-6792-4D0A-83BE-8252A8432B45}")
{
    [[nodiscard]] virtual int sum(int a, int b) const noexcept = 0;
    [[nodiscard]] virtual int get_answer() const noexcept = 0;
};

// Define implementation
class __declspec(novtable) sample_object : public rtcsdk::object<sample_object, ISampleInterface>
{
public:
    explicit sample_object(int default_answer) noexcept : default_answer_{default_answer}
    {
    }

private:
    // ISampleInterface implementation
    [[nodiscard]] int sum(int a, int b) const noexcept override
    {
        return a + b;
    }

    [[nodiscard]] int get_answer() const noexcept override
    {
        return default_answer_;
    }

    int default_answer_;
};

TEST_SUITE_BEGIN("Interface Test Suite");

TEST_CASE("sample interface works as expected.")
{
    auto obj = sample_object::create_instance(42).to_ptr();
    CHECK_EQ(obj->sum(obj->get_answer(), 5), 47);
}

TEST_SUITE_END;

TEST_SUITE_BEGIN("Error Handling Test Suite");

TEST_CASE("throw_on_failed does not throw on success")
{
    CHECK_NOTHROW(rtcsdk::throw_on_failed(S_OK));
    CHECK_NOTHROW(rtcsdk::throw_on_failed(S_FALSE));
}

TEST_CASE("throw_on_failed throws on failure")
{
    CHECK_THROWS_AS(rtcsdk::throw_on_failed(E_FAIL), rtcsdk::bad_hresult);
    CHECK_THROWS_AS(rtcsdk::throw_on_failed(E_OUTOFMEMORY), rtcsdk::bad_hresult);
}

TEST_CASE("bad_hresult stores correct HRESULT")
{
    try {
        rtcsdk::throw_bad_hresult(E_INVALIDARG);
    } catch (const rtcsdk::bad_hresult &e) {
        CHECK_EQ(e.hr(), E_INVALIDARG);
    }
}

TEST_SUITE_END;

TEST_SUITE_BEGIN("com_ptr Test Suite");

TEST_CASE("com_ptr equality")
{
    auto obj = sample_object::create_instance(42).to_ptr();
    com::ptr<ISampleInterface> copy(obj);
    CHECK(obj == copy);
    CHECK_FALSE(obj != copy);
}

TEST_CASE("com_ptr inequality with nullptr")
{
    auto obj = sample_object::create_instance(42).to_ptr();
    com::ptr<ISampleInterface> empty;
    CHECK(obj != empty);
    CHECK_FALSE(obj == empty);
}

TEST_CASE("com_ptr comparison with raw pointer")
{
    auto obj = sample_object::create_instance(42).to_ptr();
    CHECK(obj == obj.get());
    CHECK(obj.get() == obj);
}

TEST_CASE("com_ptr to_ptr with IUnknown")
{
    auto unk = sample_object::create_instance(10).to_ptr<IUnknown>();
    CHECK(unk);
}

TEST_CASE("com_ptr QueryInterface cross-interface construction")
{
    auto obj = sample_object::create_instance(42).to_ptr();

    SUBCASE("construct com_ptr<IUnknown> from com_ptr<ISampleInterface>")
    {
        com::ptr<IUnknown> unk{obj};
        CHECK(unk);
    }
}

TEST_CASE("get_interface_guid resolves correctly")
{
    auto guid = rtcsdk::get_interface_guid<ISampleInterface>();
    constexpr auto expected = rtcsdk::make_guid("{AB9A7AF1-6792-4D0A-83BE-8252A8432B45}");
    CHECK(IsEqualGUID(guid, expected));
}

TEST_SUITE_END;

TEST_SUITE_BEGIN("Utils Test Suite");

TEST_CASE("vector type traits")
{
    using namespace rtcsdk::details;
    SUBCASE("remove_front_t works as expected")
    {
        CHECK(std::is_same_v<remove_front_t<vector<int, float, double>>, vector<float, double>>);
    }
}

TEST_SUITE_END;