# rtcsdk - Real Thin COM SDK

A header-only, lightweight C++23 library for declaring and implementing Windows COM interfaces in pure C++.

**Why "Real Thin"?** ATL is a heavyweight framework: macros, COM maps, registration wizards, IDL files, and a runtime dependency on `atl*.dll`. rtcsdk strips COM support to the essentials — three headers, no dependencies beyond the C++ standard library and Windows SDK, pure compile-time codegen. The layer between your code and raw COM is as thin as it can be while still being useful.

rtcsdk is inspired by Alexander Bessonov's [`moderncom`](https://github.com/AlexBAV/moderncom) project and Kenny Kerr's `moderncpp` work. It shares the same design philosophy — replacing ATL with modern C++ — but **the implementation is entirely new**, targeting C++23 with concepts, `<stacktrace>`, and constexpr GUID parsing throughout.

## Quick Start

Define a COM interface:

```C++
#include <rtcsdk/rtcsdk.h>

COM_INTERFACE(ICalculator, "{AB9A7AF1-6792-4D0A-83BE-8252A8432B45}")
{
    virtual int sum(int a, int b) const noexcept = 0;
    virtual int get_answer() const noexcept = 0;
};
```

Or, if you prefer templates to macros:

```C++
struct ICalculator;
inline constexpr auto get_guid(ICalculator *) noexcept
{
    return "{AB9A7AF1-6792-4D0A-83BE-8252A8432B45}"_guid;
}

struct __declspec(novtable) __declspec(empty_bases)
    ICalculator : rtcsdk::extends<ICalculator, IUnknown>
{
    virtual int sum(int a, int b) const noexcept = 0;
    virtual int get_answer() const noexcept = 0;
};
```

Implement it:

```C++
class Calculator : public rtcsdk::object<Calculator, ICalculator>
{
public:
    int sum(int a, int b) const noexcept override { return a + b; }
    int get_answer() const noexcept override { return 42; }
};
```

Create and use:

```C++
// On the heap
rtcsdk::com_ptr<ICalculator> calc = Calculator::create_instance().to_ptr();
int result = calc->sum(3, 4);  // 7

// With constructor arguments
class CalculatorWithState : public rtcsdk::object<CalculatorWithState, ICalculator>
{
    int answer_;
public:
    CalculatorWithState(int answer) : answer_(answer) {}
    int sum(int a, int b) const noexcept override { return a + b; }
    int get_answer() const noexcept override { return answer_; }
};

auto calc2 = CalculatorWithState::create_instance(100).to_ptr();
calc2->get_answer();  // 100

// Multiple interfaces
COM_INTERFACE_BASE(IScientificCalc, ICalculator,
    "{C3E1F2A4-7890-4B5C-A1D2-E3F4567890AB}")
{
    virtual double sqrt(double x) const noexcept = 0;
};

class SmartCalc : public rtcsdk::object<SmartCalc, ICalculator, IScientificCalc>
{
public:
    int sum(int a, int b) const noexcept override { return a + b; }
    int get_answer() const noexcept override { return 42; }
    double sqrt(double x) const noexcept override { return std::sqrt(x); }
};

auto sci = SmartCalc::create_instance().to_ptr<IScientificCalc>();
```

That's all! `AddRef`, `Release`, and `QueryInterface` are automatically generated.

## Connection Points and Callbacks

Declare an event source by adding `connection_points<>` to the interface list:

```C++
#include <rtcsdk/connection_point.h>

COM_INTERFACE(IMyEvents, "{F1A2B3C4-D5E6-4789-A012-B34567890ABC}")
{
    virtual HRESULT on_data(int value) = 0;
    virtual HRESULT on_complete() = 0;
};

COM_INTERFACE(IMyServer, "{12345678-ABCD-4EF0-1234-567890ABCDEF}")
{
    virtual HRESULT do_work() = 0;
};

class MyServer : public rtcsdk::object<MyServer, IMyServer,
    rtcsdk::connection_points<MyServer, IMyEvents>>
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
};
```

Clients subscribe through standard COM connection points:

```C++
// Implement the sink
class MyEventSink : public rtcsdk::object<MyEventSink, IMyEvents>
{
public:
    HRESULT on_data(int value) override { /* handle */ return S_OK; }
    HRESULT on_complete() override { /* handle */ return S_OK; }
};

// Subscribe
rtcsdk::com_ptr<IConnectionPointContainer> cpc{server};
rtcsdk::com_ptr<IConnectionPoint> cp;
cpc->FindConnectionPoint(rtcsdk::get_interface_guid<IMyEvents>(), cp.put());

DWORD cookie{};
cp->Advise(sink.get(), &cookie);

// Later: cp->Unadvise(cookie);
```

Full standard `IConnectionPointContainer`/`IConnectionPoint` — works with .NET, VBScript, and any COM-aware client.

## Features

* Declare and implement COM interfaces in pure C++ — using macros or templates
* Automatic generation of `QueryInterface`, `AddRef`, and `Release`
* Support for any number of COM interfaces, directly or through implementation proxies
* Aggregation, stack-based objects, and singletons
* Non-default constructors (unlike ATL)
* ATL interoperability for gradual migration of legacy projects
* `com_ptr` smart pointer (usable independently)
* Compile-time GUID parsing (usable independently)
* Customization points for object lifetime events
* Connection points (`IConnectionPointContainer`/`IConnectionPoint`) via a single template mixin
* Built-in leak detection with stack traces (debug builds, C++23 `<stacktrace>`)
* IDL generator tool (`idlgen`) for type library generation from C++ headers

## Requirements

C++23. Tested on Microsoft Visual C++ (Visual Studio 2022). Supports 32-bit (x86) and 64-bit (x64/ARM) targets, Windows 7 through Windows 11.

## Installation

Header-only — no build step. Add the `rtcsdk` folder to your include path. The core is a single header under 2,000 lines. Two opt-in headers add DLL server and connection point support — include them only if you need them.

## Documentation

### Core — Real Thin COM SDK (`rtcsdk/rtcsdk.h`)

- [GUID Helpers](doc/guid-helpers.md) — compile-time GUID parsing, identifier resolution, concepts
- [COM Smart Pointer](doc/com-ptr.md) — `rtcsdk::com_ptr` and `rtcsdk::ref`
- [COM Interface Support](doc/interfaces.md) — declaring and implementing interfaces, `object`, `also`, `aggregates`, `intermediate`
- [Traits](doc/traits.md) — `singleton_factory`, `single_cached_instance`, `supports_aggregation`, `implements_module_count`, `enable_leak_detection`
- [Customization Points](doc/customization-points.md) — `final_construct`, `final_release`, `on_add_ref`, `on_release`, `pre/post_query_interface`
- [Constructing Objects](doc/constructing-objects.md) — heap, stack, and default (CLSID-based) construction
- [Leak Detection](doc/leak-detection.md) — built-in leak detection with `<stacktrace>`

### DLL Server — opt-in (`rtcsdk/factory.h`)

- [COM DLL Server](doc/dll-server.md) — implementing `DllGetClassObject` and `DllCanUnloadNow`

### Connection Points — opt-in (`rtcsdk/connection_point.h`)

- [Connection Points](doc/connection-points.md) — `IConnectionPointContainer`/`IConnectionPoint` via `connection_points<>` mixin

### Reference

- [Macro Reference](doc/macros.md) — all public macros with preferred short names and legacy aliases
- [IDL Generator](doc/idlgen.md) — `idlgen` tool for generating IDL and type libraries from C++ headers
- [FAQ](doc/faq.md) — platform support, apartment models, callbacks, inspiration
