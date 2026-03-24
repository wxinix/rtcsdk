// Copyright (c) 2022-2026 Wuping Xin. All rights reserved.
// MIT License. See LICENSE.md for details.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#define RTCSDK_COM_NO_LEAK_DETECTION

#include <rtcsdk/connection_point.h>
#include <rtcsdk/factory.h>

COM_INTERFACE(ISampleInterface, "{AB9A7AF1-6792-4D0A-83BE-8252A8432B45}")
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
    rtcsdk::com_ptr<ISampleInterface> copy(obj);
    CHECK(obj == copy);
    CHECK_FALSE(obj != copy);
}

TEST_CASE("com_ptr inequality with nullptr")
{
    auto obj = sample_object::create_instance(42).to_ptr();
    rtcsdk::com_ptr<ISampleInterface> empty;
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
        rtcsdk::com_ptr<IUnknown> unk{obj};
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

TEST_SUITE_BEGIN("com_ptr Extended Test Suite");

TEST_CASE("com_ptr boolean conversion")
{
    rtcsdk::com_ptr<ISampleInterface> empty;
    CHECK_FALSE(static_cast<bool>(empty));

    auto obj = sample_object::create_instance(1).to_ptr();
    CHECK(static_cast<bool>(obj));
}

TEST_CASE("com_ptr attach and detach")
{
    auto obj = sample_object::create_instance(42).to_ptr();
    auto *raw = obj.detach();
    CHECK_FALSE(obj);
    CHECK(raw != nullptr);

    rtcsdk::com_ptr<ISampleInterface> reattached;
    reattached.attach(raw);
    CHECK(reattached);
    CHECK_EQ(reattached->get_answer(), 42);
}

TEST_CASE("com_ptr move semantics")
{
    auto obj = sample_object::create_instance(7).to_ptr();
    auto *raw = obj.get();

    // Move construct
    rtcsdk::com_ptr<ISampleInterface> moved{std::move(obj)};
    CHECK_FALSE(obj);  // NOLINT - testing post-move state
    CHECK(moved);
    CHECK_EQ(moved.get(), raw);

    // Move assign
    rtcsdk::com_ptr<ISampleInterface> target;
    target = std::move(moved);
    CHECK_FALSE(moved);  // NOLINT
    CHECK(target);
    CHECK_EQ(target->get_answer(), 7);
}

TEST_CASE("com_ptr reset releases the pointer")
{
    auto obj = sample_object::create_instance(1).to_ptr();
    CHECK(obj);
    obj.reset();
    CHECK_FALSE(obj);
}

TEST_CASE("com_ptr as() queries for another interface")
{
    auto obj = sample_object::create_instance(42).to_ptr();
    auto unk = obj.as<IUnknown>();
    CHECK(unk);
}

TEST_CASE("com_ptr put() returns write address")
{
    rtcsdk::com_ptr<ISampleInterface> p;
    auto **pp = p.put();
    CHECK(pp != nullptr);
    CHECK(*pp == nullptr);
}

TEST_SUITE_END;

TEST_CASE("com_ptr copy semantics")
{
    auto obj = sample_object::create_instance(42).to_ptr();
    rtcsdk::com_ptr<ISampleInterface> copy = obj;
    CHECK(copy);
    CHECK(obj);
    CHECK_EQ(copy.get(), obj.get());
    CHECK_EQ(copy->get_answer(), 42);
}

TEST_CASE("com_ptr copy assignment")
{
    auto obj1 = sample_object::create_instance(1).to_ptr();
    auto obj2 = sample_object::create_instance(2).to_ptr();
    CHECK_EQ(obj1->get_answer(), 1);
    obj1 = obj2;
    CHECK_EQ(obj1->get_answer(), 2);
    CHECK_EQ(obj1.get(), obj2.get());
}

TEST_CASE("com_ptr self-assignment is safe")
{
    auto obj = sample_object::create_instance(42).to_ptr();
    auto *raw = obj.get();
    obj = obj;  // NOLINT - intentional self-assignment test
    CHECK_EQ(obj.get(), raw);
    CHECK_EQ(obj->get_answer(), 42);
}

TEST_CASE("com_ptr ordering")
{
    auto a = sample_object::create_instance(1).to_ptr();
    auto b = sample_object::create_instance(2).to_ptr();
    // One must be less than the other (pointer comparison)
    CHECK((a < b || b < a));
    CHECK_FALSE(a < a);
}

TEST_CASE("com_ptr default is null")
{
    rtcsdk::com_ptr<ISampleInterface> p;
    CHECK_FALSE(p);
    CHECK_EQ(p.get(), nullptr);

    rtcsdk::com_ptr<ISampleInterface> p2{nullptr};
    CHECK_FALSE(p2);
}

TEST_SUITE_END;

TEST_SUITE_BEGIN("com::ref Test Suite");

TEST_CASE("com::ref from com_ptr")
{
    auto obj = sample_object::create_instance(42).to_ptr();
    rtcsdk::ref<ISampleInterface> r{obj};
    CHECK(r);
    CHECK_EQ(r->get_answer(), 42);
    CHECK_EQ(r.get(), obj.get());
}

TEST_CASE("com::ref as() creates a com_ptr")
{
    auto obj = sample_object::create_instance(1).to_ptr();
    rtcsdk::ref<ISampleInterface> r{obj};
    auto unk = r.as<IUnknown>();
    CHECK(unk);
}

TEST_CASE("com::ref default is null")
{
    rtcsdk::ref<ISampleInterface> r;
    CHECK_FALSE(r);
    CHECK_EQ(r.get(), nullptr);
}

TEST_CASE("com::ref from raw pointer")
{
    auto obj = sample_object::create_instance(42).to_ptr();
    rtcsdk::ref<ISampleInterface> r{obj.get()};
    CHECK(r);
    CHECK_EQ(r->get_answer(), 42);
}

TEST_CASE("com::ref comparison operators")
{
    auto obj = sample_object::create_instance(1).to_ptr();
    rtcsdk::ref<ISampleInterface> r1{obj};
    rtcsdk::ref<ISampleInterface> r2{obj};
    CHECK(r1 == r2);
    CHECK(r1 == obj);
    CHECK(r1 == obj.get());
}

TEST_SUITE_END;

TEST_SUITE_BEGIN("GUID Test Suite");

TEST_CASE("make_guid parses braced GUID")
{
    constexpr auto guid = rtcsdk::make_guid("{AB9A7AF1-6792-4D0A-83BE-8252A8432B45}");
    CHECK_EQ(guid.Data1, 0xAB9A7AF1);
    CHECK_EQ(guid.Data2, 0x6792);
    CHECK_EQ(guid.Data3, 0x4D0A);
}

TEST_CASE("make_guid parses unbraced GUID")
{
    constexpr auto guid = rtcsdk::make_guid("AB9A7AF1-6792-4D0A-83BE-8252A8432B45");
    CHECK_EQ(guid.Data1, 0xAB9A7AF1);
}

TEST_CASE("_guid UDL works")
{
    using namespace rtcsdk;
    constexpr auto guid = "{AB9A7AF1-6792-4D0A-83BE-8252A8432B45}"_guid;
    constexpr auto expected = rtcsdk::make_guid("{AB9A7AF1-6792-4D0A-83BE-8252A8432B45}");
    CHECK(IsEqualGUID(guid, expected));
}

TEST_SUITE_END;

TEST_SUITE_BEGIN("Multiple Interface Test Suite");

COM_INTERFACE(ISecondInterface, "{DEADBEEF-1234-5678-9ABC-DEF012345678}")
{
    virtual int multiply(int a, int b) const noexcept = 0;
};

class multi_object : public rtcsdk::object<multi_object, ISampleInterface, ISecondInterface>
{
    int sum(int a, int b) const noexcept override { return a + b; }
    int get_answer() const noexcept override { return 99; }
    int multiply(int a, int b) const noexcept override { return a * b; }
};

TEST_CASE("QueryInterface between multiple interfaces")
{
    auto obj = multi_object::create_instance().to_ptr();
    CHECK_EQ(obj->sum(3, 4), 7);

    auto second = obj.as<ISecondInterface>();
    CHECK(second);
    CHECK_EQ(second->multiply(3, 4), 12);

    // QI back to first interface
    auto first = second.as<ISampleInterface>();
    CHECK(first);
    CHECK_EQ(first->get_answer(), 99);

    // COM identity: QI for IUnknown from any interface must return the same pointer
    rtcsdk::com_ptr<IUnknown> unk1;
    obj->QueryInterface(__uuidof(IUnknown), reinterpret_cast<void **>(unk1.put()));
    rtcsdk::com_ptr<IUnknown> unk2;
    second->QueryInterface(__uuidof(IUnknown), reinterpret_cast<void **>(unk2.put()));
    CHECK_EQ(unk1.get(), unk2.get());
}

TEST_SUITE_END;

TEST_SUITE_BEGIN("value_on_stack Test Suite");

TEST_CASE("value_on_stack basic usage")
{
    rtcsdk::value_on_stack<sample_object> obj{42};
    auto *iface = static_cast<ISampleInterface *>(&obj);
    CHECK_EQ(iface->get_answer(), 42);
    CHECK_EQ(iface->sum(1, 2), 3);
}

TEST_SUITE_END;

TEST_SUITE_BEGIN("Error Handling Extended");

TEST_CASE("bad_hresult default has E_FAIL")
{
    rtcsdk::bad_hresult err;
    CHECK_EQ(err.hr(), E_FAIL);
}

TEST_CASE("bad_hresult is_aborted")
{
    rtcsdk::bad_hresult aborted{HRESULT_FROM_WIN32(ERROR_OPERATION_ABORTED)};
    CHECK(aborted.is_aborted());

    rtcsdk::bad_hresult not_aborted{E_FAIL};
    CHECK_FALSE(not_aborted.is_aborted());
}

TEST_CASE("throw_win32_error throws correct HRESULT")
{
    try {
        rtcsdk::throw_win32_error(ERROR_FILE_NOT_FOUND);
    } catch (const rtcsdk::bad_hresult &e) {
        CHECK_EQ(e.hr(), HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
    }
}

TEST_SUITE_END;

TEST_SUITE_BEGIN("GUID Extended");

TEST_CASE("make_guid parses all Data4 bytes")
{
    constexpr auto guid = rtcsdk::make_guid("{AB9A7AF1-6792-4D0A-83BE-8252A8432B45}");
    CHECK_EQ(guid.Data4[0], 0x83);
    CHECK_EQ(guid.Data4[1], 0xBE);
    CHECK_EQ(guid.Data4[2], 0x82);
    CHECK_EQ(guid.Data4[3], 0x52);
    CHECK_EQ(guid.Data4[4], 0xA8);
    CHECK_EQ(guid.Data4[5], 0x43);
    CHECK_EQ(guid.Data4[6], 0x2B);
    CHECK_EQ(guid.Data4[7], 0x45);
}

TEST_CASE("two different GUIDs are not equal")
{
    constexpr auto a = rtcsdk::make_guid("{AB9A7AF1-6792-4D0A-83BE-8252A8432B45}");
    constexpr auto b = rtcsdk::make_guid("{12345678-ABCD-4EF0-1234-567890ABCDEF}");
    CHECK_FALSE(IsEqualGUID(a, b));
}

TEST_SUITE_END;

TEST_SUITE_BEGIN("Object Holder");

TEST_CASE("object_holder obj() allows pre-conversion access")
{
    auto holder = sample_object::create_instance(10);
    // obj() returns Derived* — access through interface pointer
    auto *iface = static_cast<ISampleInterface *>(holder.obj());
    CHECK_EQ(iface->get_answer(), 10);
    auto ptr = std::move(holder).to_ptr();
    CHECK_EQ(ptr->get_answer(), 10);
}

TEST_SUITE_END;

TEST_SUITE_BEGIN("QueryInterface Edge Cases");

TEST_CASE("QI for unsupported interface returns E_NOINTERFACE")
{
    auto obj = sample_object::create_instance(1).to_ptr();
    constexpr auto fake_iid = rtcsdk::make_guid("{00000000-0000-0000-0000-FFFFFFFFFFFF}");
    void *result = nullptr;
    CHECK_EQ(obj->QueryInterface(fake_iid, &result), E_NOINTERFACE);
    CHECK_EQ(result, nullptr);
}

TEST_CASE("QI for IUnknown always succeeds")
{
    auto obj = sample_object::create_instance(1).to_ptr();
    rtcsdk::com_ptr<IUnknown> unk;
    CHECK_EQ(obj->QueryInterface(__uuidof(IUnknown), reinterpret_cast<void **>(unk.put())), S_OK);
    CHECK(unk);
}

TEST_SUITE_END;

TEST_SUITE_BEGIN("Intermediate Test Suite");

COM_INTERFACE(IPartialInterface, "{11111111-2222-3333-4444-555566667777}")
{
    virtual int method_a() const noexcept = 0;
    virtual int method_b() const noexcept = 0;
};

// Partial implementation via intermediate
class partial_impl : public rtcsdk::intermediate<partial_impl, IPartialInterface>
{
public:
    int method_a() const noexcept override { return 100; }
};

// Final class completes the implementation
class final_object : public rtcsdk::object<final_object, partial_impl>
{
public:
    int method_b() const noexcept override { return 200; }
};

TEST_CASE("intermediate provides partial implementation")
{
    auto obj = final_object::create_instance().to_ptr<IPartialInterface>();
    CHECK_EQ(obj->method_a(), 100);
    CHECK_EQ(obj->method_b(), 200);
}

TEST_CASE("intermediate object supports QI for its interface")
{
    auto obj = final_object::create_instance().to_ptr<IPartialInterface>();
    rtcsdk::com_ptr<IUnknown> unk;
    CHECK_EQ(obj->QueryInterface(__uuidof(IUnknown), reinterpret_cast<void **>(unk.put())), S_OK);
    CHECK(unk);
}

TEST_SUITE_END;

// ── Connection Point Tests ──────────────────────────────────────────────────

COM_INTERFACE(IMyEvents, "{F1A2B3C4-D5E6-4789-A012-B34567890ABC}")
{
    virtual HRESULT on_data(int value) = 0;
    virtual HRESULT on_complete() = 0;
};

COM_INTERFACE(IOtherEvents, "{A1B2C3D4-E5F6-4789-0123-456789ABCDEF}")
{
    virtual HRESULT on_status(int code) = 0;
};

// Event source — implements IMyServer and fires IMyEvents + IOtherEvents
COM_INTERFACE(IMyServer, "{12345678-ABCD-4EF0-1234-567890ABCDEF}")
{
    virtual HRESULT do_work() = 0;
    virtual HRESULT do_status(int code) = 0;
};

class __declspec(novtable) my_server
    : public rtcsdk::object<my_server, IMyServer,
          rtcsdk::connection_points<my_server, IMyEvents, IOtherEvents>>
{
public:
    HRESULT do_work() override
    {
        fire<IMyEvents>([](IMyEvents *sink) {
            sink->on_data(42);
            sink->on_complete();
        });
        return S_OK;
    }

    HRESULT do_status(int code) override
    {
        fire<IOtherEvents>([code](IOtherEvents *sink) { sink->on_status(code); });
        return S_OK;
    }
};

// Event sink implementations
class my_event_sink : public rtcsdk::object<my_event_sink, IMyEvents>
{
public:
    int last_data{};
    int complete_count{};

    HRESULT on_data(int value) override
    {
        last_data = value;
        return S_OK;
    }

    HRESULT on_complete() override
    {
        ++complete_count;
        return S_OK;
    }
};

class other_event_sink : public rtcsdk::object<other_event_sink, IOtherEvents>
{
public:
    int last_status{};

    HRESULT on_status(int code) override
    {
        last_status = code;
        return S_OK;
    }
};

TEST_SUITE_BEGIN("Connection Point Test Suite");

TEST_CASE("FindConnectionPoint succeeds for registered event interfaces")
{
    auto server = my_server::create_instance().to_ptr<IMyServer>();
    rtcsdk::com_ptr<IConnectionPointContainer> cpc{server};
    REQUIRE(cpc);

    rtcsdk::com_ptr<IConnectionPoint> cp;
    CHECK_EQ(cpc->FindConnectionPoint(rtcsdk::get_interface_guid<IMyEvents>(), cp.put()), S_OK);
    CHECK(cp);
}

TEST_CASE("FindConnectionPoint fails for unregistered interfaces")
{
    auto server = my_server::create_instance().to_ptr<IMyServer>();
    rtcsdk::com_ptr<IConnectionPointContainer> cpc{server};

    rtcsdk::com_ptr<IConnectionPoint> cp;
    CHECK_EQ(cpc->FindConnectionPoint(rtcsdk::get_interface_guid<ISampleInterface>(), cp.put()),
             CONNECT_E_NOCONNECTION);
}

TEST_CASE("Advise and fire events")
{
    auto server_holder = my_server::create_instance();
    auto *server_obj = server_holder.obj();
    auto server = std::move(server_holder).to_ptr<IMyServer>();

    // Create sink
    auto sink_holder = my_event_sink::create_instance();
    auto *sink_obj = sink_holder.obj();
    auto sink = std::move(sink_holder).to_ptr<IMyEvents>();

    // Subscribe
    rtcsdk::com_ptr<IConnectionPointContainer> cpc{server};
    rtcsdk::com_ptr<IConnectionPoint> cp;
    cpc->FindConnectionPoint(rtcsdk::get_interface_guid<IMyEvents>(), cp.put());

    DWORD cookie{};
    CHECK_EQ(cp->Advise(sink.get(), &cookie), S_OK);
    CHECK_NE(cookie, 0);

    // Fire
    server->do_work();
    CHECK_EQ(sink_obj->last_data, 42);
    CHECK_EQ(sink_obj->complete_count, 1);

    // Fire again
    server->do_work();
    CHECK_EQ(sink_obj->complete_count, 2);

    // Unadvise
    CHECK_EQ(cp->Unadvise(cookie), S_OK);

    // Fire after unadvise — sink should not be called
    server->do_work();
    CHECK_EQ(sink_obj->complete_count, 2);
}

TEST_CASE("Multiple event interfaces work independently")
{
    auto server_holder = my_server::create_instance();
    auto *server_obj = server_holder.obj();
    auto server = std::move(server_holder).to_ptr<IMyServer>();

    auto sink1_holder = my_event_sink::create_instance();
    auto *sink1_obj = sink1_holder.obj();
    auto sink1 = std::move(sink1_holder).to_ptr<IMyEvents>();

    auto sink2_holder = other_event_sink::create_instance();
    auto *sink2_obj = sink2_holder.obj();
    auto sink2 = std::move(sink2_holder).to_ptr<IOtherEvents>();

    rtcsdk::com_ptr<IConnectionPointContainer> cpc{server};

    rtcsdk::com_ptr<IConnectionPoint> cp1;
    cpc->FindConnectionPoint(rtcsdk::get_interface_guid<IMyEvents>(), cp1.put());
    DWORD cookie1{};
    cp1->Advise(sink1.get(), &cookie1);

    rtcsdk::com_ptr<IConnectionPoint> cp2;
    cpc->FindConnectionPoint(rtcsdk::get_interface_guid<IOtherEvents>(), cp2.put());
    DWORD cookie2{};
    cp2->Advise(sink2.get(), &cookie2);

    server->do_work();
    CHECK_EQ(sink1_obj->last_data, 42);
    CHECK_EQ(sink2_obj->last_status, 0); // IOtherEvents not fired by do_work

    server->do_status(99);
    CHECK_EQ(sink2_obj->last_status, 99);
    CHECK_EQ(sink1_obj->last_data, 42); // IMyEvents not fired by do_status

    cp1->Unadvise(cookie1);
    cp2->Unadvise(cookie2);
}

TEST_CASE("Unadvise with invalid cookie returns error")
{
    auto server = my_server::create_instance().to_ptr<IMyServer>();
    rtcsdk::com_ptr<IConnectionPointContainer> cpc{server};
    rtcsdk::com_ptr<IConnectionPoint> cp;
    cpc->FindConnectionPoint(rtcsdk::get_interface_guid<IMyEvents>(), cp.put());

    CHECK_EQ(cp->Unadvise(99999), CONNECT_E_NOCONNECTION);
}

TEST_CASE("GetConnectionInterface returns correct IID")
{
    auto server = my_server::create_instance().to_ptr<IMyServer>();
    rtcsdk::com_ptr<IConnectionPointContainer> cpc{server};
    rtcsdk::com_ptr<IConnectionPoint> cp;
    cpc->FindConnectionPoint(rtcsdk::get_interface_guid<IMyEvents>(), cp.put());

    IID iid{};
    CHECK_EQ(cp->GetConnectionInterface(&iid), S_OK);
    CHECK(IsEqualGUID(iid, rtcsdk::get_interface_guid<IMyEvents>()));
}

TEST_CASE("EnumConnectionPoints enumerates all event interfaces")
{
    auto server = my_server::create_instance().to_ptr<IMyServer>();
    rtcsdk::com_ptr<IConnectionPointContainer> cpc{server};

    rtcsdk::com_ptr<IEnumConnectionPoints> enumerator;
    CHECK_EQ(cpc->EnumConnectionPoints(enumerator.put()), S_OK);

    IConnectionPoint *pts[2]{};
    ULONG fetched{};
    CHECK_EQ(enumerator->Next(2, pts, &fetched), S_OK);
    CHECK_EQ(fetched, 2);

    // Verify both have valid IIDs
    IID iid1{}, iid2{};
    pts[0]->GetConnectionInterface(&iid1);
    pts[1]->GetConnectionInterface(&iid2);
    pts[0]->Release();
    pts[1]->Release();

    CHECK((IsEqualGUID(iid1, rtcsdk::get_interface_guid<IMyEvents>())
           || IsEqualGUID(iid1, rtcsdk::get_interface_guid<IOtherEvents>())));
    CHECK((IsEqualGUID(iid2, rtcsdk::get_interface_guid<IMyEvents>())
           || IsEqualGUID(iid2, rtcsdk::get_interface_guid<IOtherEvents>())));
    CHECK_FALSE(IsEqualGUID(iid1, iid2));
}

TEST_CASE("Advise with wrong interface type returns CONNECT_E_CANNOTCONNECT")
{
    auto server = my_server::create_instance().to_ptr<IMyServer>();
    rtcsdk::com_ptr<IConnectionPointContainer> cpc{server};
    rtcsdk::com_ptr<IConnectionPoint> cp;
    cpc->FindConnectionPoint(rtcsdk::get_interface_guid<IMyEvents>(), cp.put());

    // Try to advise with an IOtherEvents sink on an IMyEvents connection point
    auto wrong_sink = other_event_sink::create_instance().to_ptr<IOtherEvents>();
    DWORD cookie{};
    CHECK_EQ(cp->Advise(wrong_sink.get(), &cookie), CONNECT_E_CANNOTCONNECT);
}

TEST_CASE("Multiple sinks on the same connection point")
{
    auto server_holder = my_server::create_instance();
    auto server = std::move(server_holder).to_ptr<IMyServer>();

    auto sink1_holder = my_event_sink::create_instance();
    auto *sink1_obj = sink1_holder.obj();
    auto sink1 = std::move(sink1_holder).to_ptr<IMyEvents>();

    auto sink2_holder = my_event_sink::create_instance();
    auto *sink2_obj = sink2_holder.obj();
    auto sink2 = std::move(sink2_holder).to_ptr<IMyEvents>();

    rtcsdk::com_ptr<IConnectionPointContainer> cpc{server};
    rtcsdk::com_ptr<IConnectionPoint> cp;
    cpc->FindConnectionPoint(rtcsdk::get_interface_guid<IMyEvents>(), cp.put());

    DWORD cookie1{}, cookie2{};
    cp->Advise(sink1.get(), &cookie1);
    cp->Advise(sink2.get(), &cookie2);
    CHECK_NE(cookie1, cookie2);

    server->do_work();
    CHECK_EQ(sink1_obj->last_data, 42);
    CHECK_EQ(sink2_obj->last_data, 42);
    CHECK_EQ(sink1_obj->complete_count, 1);
    CHECK_EQ(sink2_obj->complete_count, 1);

    // Unadvise first sink, fire again
    cp->Unadvise(cookie1);
    server->do_work();
    CHECK_EQ(sink1_obj->complete_count, 1);  // not called
    CHECK_EQ(sink2_obj->complete_count, 2);  // still connected

    cp->Unadvise(cookie2);
}

TEST_CASE("GetConnectionPointContainer returns the container")
{
    auto server = my_server::create_instance().to_ptr<IMyServer>();
    rtcsdk::com_ptr<IConnectionPointContainer> cpc{server};
    rtcsdk::com_ptr<IConnectionPoint> cp;
    cpc->FindConnectionPoint(rtcsdk::get_interface_guid<IMyEvents>(), cp.put());

    rtcsdk::com_ptr<IConnectionPointContainer> container;
    CHECK_EQ(cp->GetConnectionPointContainer(container.put()), S_OK);
    CHECK(container);
    CHECK_EQ(container.get(), cpc.get());
}

TEST_CASE("EnumConnections enumerates active sinks")
{
    auto server = my_server::create_instance().to_ptr<IMyServer>();
    auto sink = my_event_sink::create_instance().to_ptr<IMyEvents>();

    rtcsdk::com_ptr<IConnectionPointContainer> cpc{server};
    rtcsdk::com_ptr<IConnectionPoint> cp;
    cpc->FindConnectionPoint(rtcsdk::get_interface_guid<IMyEvents>(), cp.put());

    DWORD cookie{};
    cp->Advise(sink.get(), &cookie);

    rtcsdk::com_ptr<IEnumConnections> enumerator;
    CHECK_EQ(cp->EnumConnections(enumerator.put()), S_OK);

    CONNECTDATA cd{};
    ULONG fetched{};
    CHECK_EQ(enumerator->Next(1, &cd, &fetched), S_OK);
    CHECK_EQ(fetched, 1);
    CHECK(cd.pUnk != nullptr);
    CHECK_EQ(cd.dwCookie, cookie);
    cd.pUnk->Release();

    // No more items
    CHECK_EQ(enumerator->Next(1, &cd, &fetched), S_FALSE);
    CHECK_EQ(fetched, 0);

    cp->Unadvise(cookie);
}

TEST_CASE("Enumerator Reset and Skip work")
{
    auto server = my_server::create_instance().to_ptr<IMyServer>();
    rtcsdk::com_ptr<IConnectionPointContainer> cpc{server};

    rtcsdk::com_ptr<IEnumConnectionPoints> enumerator;
    cpc->EnumConnectionPoints(enumerator.put());

    // Skip one
    CHECK_EQ(enumerator->Skip(1), S_OK);

    // Get the remaining one
    IConnectionPoint *cp{};
    ULONG fetched{};
    CHECK_EQ(enumerator->Next(1, &cp, &fetched), S_OK);
    CHECK_EQ(fetched, 1);
    cp->Release();

    // No more
    CHECK_EQ(enumerator->Next(1, &cp, &fetched), S_FALSE);

    // Reset and get both
    CHECK_EQ(enumerator->Reset(), S_OK);
    IConnectionPoint *both[2]{};
    CHECK_EQ(enumerator->Next(2, both, &fetched), S_OK);
    CHECK_EQ(fetched, 2);
    both[0]->Release();
    both[1]->Release();
}

TEST_CASE("Fire with no sinks does not crash")
{
    auto server = my_server::create_instance().to_ptr<IMyServer>();
    // No sinks connected — fire should be a no-op
    CHECK_EQ(server->do_work(), S_OK);
    CHECK_EQ(server->do_status(1), S_OK);
}

TEST_CASE("Enumerator Clone preserves position")
{
    auto server = my_server::create_instance().to_ptr<IMyServer>();
    rtcsdk::com_ptr<IConnectionPointContainer> cpc{server};

    rtcsdk::com_ptr<IEnumConnectionPoints> enumerator;
    cpc->EnumConnectionPoints(enumerator.put());

    // Advance past first item
    enumerator->Skip(1);

    // Clone
    rtcsdk::com_ptr<IEnumConnectionPoints> clone;
    CHECK_EQ(enumerator->Clone(clone.put()), S_OK);

    // Clone should be at the same position — one item left
    IConnectionPoint *cp{};
    ULONG fetched{};
    CHECK_EQ(clone->Next(1, &cp, &fetched), S_OK);
    CHECK_EQ(fetched, 1);
    cp->Release();

    // Clone should now be at the end
    CHECK_EQ(clone->Next(1, &cp, &fetched), S_FALSE);
    CHECK_EQ(fetched, 0);
}

TEST_CASE("Double Unadvise returns error on second call")
{
    auto server = my_server::create_instance().to_ptr<IMyServer>();
    auto sink = my_event_sink::create_instance().to_ptr<IMyEvents>();

    rtcsdk::com_ptr<IConnectionPointContainer> cpc{server};
    rtcsdk::com_ptr<IConnectionPoint> cp;
    cpc->FindConnectionPoint(rtcsdk::get_interface_guid<IMyEvents>(), cp.put());

    DWORD cookie{};
    cp->Advise(sink.get(), &cookie);
    CHECK_EQ(cp->Unadvise(cookie), S_OK);
    CHECK_EQ(cp->Unadvise(cookie), CONNECT_E_NOCONNECTION);
}

TEST_SUITE_END;