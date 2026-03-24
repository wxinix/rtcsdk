# COM Interface Support

```C++
#include <rtcsdk/rtcsdk.h>
```

## Declaring Interfaces

rtcsdk offers two ways to declare COM interfaces: convenience macros (the common path) and direct template usage (for advanced scenarios).

### Using Macros (Recommended)

**`COM_INTERFACE`** — declares an interface derived from `IUnknown`:

```C++
COM_INTERFACE(ICalculator, "{AB9A7AF1-6792-4D0A-83BE-8252A8432B45}")
{
    virtual int sum(int a, int b) const noexcept = 0;
    virtual int get_answer() const noexcept = 0;
};
```

**`COM_INTERFACE_BASE`** — declares an interface derived from another interface:

```C++
COM_INTERFACE_BASE(IScientificCalculator, ICalculator,
    "{C3E1F2A4-7890-4B5C-A1D2-E3F4567890AB}")
{
    virtual double sqrt(double x) const noexcept = 0;
    virtual double power(double base, double exp) const noexcept = 0;
};
```

#### What the macros expand to

The macros set up three things automatically:

1. A forward declaration of the struct
2. A `constexpr` free function `get_guid(name*)` so the library can resolve the IID at compile time
3. A struct definition that inherits from `rtcsdk::extends<name, base>`, which wires up `QueryInterface` traversal

Expanded form of `COM_INTERFACE(ICalculator, "{...}")`:

```C++
struct ICalculator;
inline constexpr auto get_guid(ICalculator *) noexcept
{
    return rtcsdk::make_guid("{AB9A7AF1-6792-4D0A-83BE-8252A8432B45}");
}
struct __declspec(novtable) __declspec(empty_bases) __declspec(uuid("..."))
    ICalculator : rtcsdk::extends<ICalculator, IUnknown>
{
    virtual int sum(int a, int b) const noexcept = 0;
    virtual int get_answer() const noexcept = 0;
};
```

### Using Templates Directly (No Macros)

If you prefer not to use macros, you can declare interfaces with plain C++ templates. You need to:

1. Define the struct inheriting from `rtcsdk::extends<YourInterface, BaseInterface>`
2. Provide a GUID via any of the [GUID resolution mechanisms](guid-helpers.md)

```C++
// Step 1: Forward declare and provide a GUID
struct ILogger;
inline constexpr auto get_guid(ILogger *) noexcept
{
    return "{D4E5F6A7-8901-4B2C-C3D4-E5F678901234}"_guid;
}

// Step 2: Define the interface using rtcsdk::extends
struct __declspec(novtable) __declspec(empty_bases)
    ILogger : rtcsdk::extends<ILogger, IUnknown>
{
    virtual void log(const wchar_t *message) noexcept = 0;
    virtual int get_log_count() const noexcept = 0;
};
```

Or, more compactly, using a static member for the GUID:

```C++
struct __declspec(novtable) __declspec(empty_bases)
    ILogger : rtcsdk::extends<ILogger, IUnknown>
{
    static constexpr GUID get_guid()
    {
        return "{D4E5F6A7-8901-4B2C-C3D4-E5F678901234}"_guid;
    }

    virtual void log(const wchar_t *message) noexcept = 0;
    virtual int get_log_count() const noexcept = 0;
};
```

Both approaches are equivalent — the macros simply automate the boilerplate.

### Working with Legacy Interfaces

The library also works with "legacy" interfaces — any abstract class derived from `IUnknown` for which the library can [fetch an IID](guid-helpers.md). This includes all Platform SDK interfaces like `IStream`, `IDispatch`, etc.

```C++
// Legacy interfaces work out of the box — __uuidof resolves their GUIDs
rtcsdk::com_ptr<IStream> stream;
stream.CoCreateInstance(CLSID_MyStream);
```

## Implementing Interfaces

### Basic Implementation

Derive your class from `rtcsdk::object<Derived, Interfaces...>`. The library automatically generates `QueryInterface`, `AddRef`, and `Release`:

```C++
class __declspec(novtable) Calculator
    : public rtcsdk::object<Calculator, ICalculator>
{
    int answer_ = 42;

public:
    int sum(int a, int b) const noexcept override { return a + b; }
    int get_answer() const noexcept override { return answer_; }
};
```

### Multiple Interfaces

List all interfaces in the template argument list:

```C++
class __declspec(novtable) SmartCalculator
    : public rtcsdk::object<SmartCalculator, ICalculator, IScientificCalculator>
{
public:
    // ICalculator
    int sum(int a, int b) const noexcept override { return a + b; }
    int get_answer() const noexcept override { return 42; }

    // IScientificCalculator
    double sqrt(double x) const noexcept override { return std::sqrt(x); }
    double power(double base, double exp) const noexcept override { return std::pow(base, exp); }
};
```

### Non-Default Constructors

Unlike ATL, your class can have constructors that take arguments and throw:

```C++
class Calculator : public rtcsdk::object<Calculator, ICalculator>
{
    int initial_answer_;

public:
    Calculator(int initial_answer) : initial_answer_(initial_answer) {}

    int sum(int a, int b) const noexcept override { return a + b; }
    int get_answer() const noexcept override { return initial_answer_; }
};

// Construct with arguments:
auto calc = Calculator::create_instance(42).to_ptr();
```

### Creating Instances

```C++
// On the heap — returns com_ptr to the default (first) interface
rtcsdk::com_ptr<ICalculator> calc = Calculator::create_instance().to_ptr();

// Query a different interface at creation time
rtcsdk::com_ptr<IScientificCalculator> sci = SmartCalculator::create_instance()
    .to_ptr<IScientificCalculator>();

// With constructor arguments
auto calc = Calculator::create_instance(42).to_ptr();
```

## `object`

```C++
template<typename Derived, typename... Interfaces>
class object;
```

`Derived` is the class name (CRTP). `Interfaces` is a list of:

* COM interface classes (for which the library can fetch IIDs)
* [`also<SomeInterface>`](#also) — expose legacy base interfaces
* [`eats_all<Derived>`](#eats_all) — custom `QueryInterface` handler
* [`aggregates<Derived, Interfaces...>`](#aggregates) — delegated interfaces
* Classes derived from [`intermediate`](#intermediate) — implementation proxies

### Members

| Member | Description |
|--------|-------------|
| `using DefaultInterface = ...` | The first (default) interface. Never `IUnknown`. |
| `IUnknown *GetUnknown() noexcept` | Direct `IUnknown` pointer without `AddRef` |
| `QueryInterface(riid, ppvObject)` | Standard `IUnknown::QueryInterface` implementation |
| `static create_instance(args...)` | Heap-construct, returns [`object_holder`](#object_holder) |
| `static create_aggregate(pOuter, args...)` | Create aggregated instance (requires [`supports_aggregation`](traits.md#supports_aggregation)) |
| `create_copy<Interface>() const` | Copy-construct and query interface |

Protected helpers: `addref()`, `release()`.

## `also`

Use `also` when your class implements a legacy interface that derives from another legacy interface, and you want `QueryInterface` to support the base:

```C++
// Without also: QI for IDispatch fails!
class MyClass : public rtcsdk::object<MyClass, IDispatchEx> { ... };

// With also: QI for both IDispatchEx and IDispatch works
class MyClass : public rtcsdk::object<MyClass, IDispatchEx, rtcsdk::also<IDispatch>> { ... };
```

Not needed for interfaces declared with `COM_INTERFACE` — those are automatically wired.

## `eats_all`

A `QueryInterface` escape hatch. Add `eats_all<Derived>` to the interface list and implement:

```C++
class MyClass : public rtcsdk::object<MyClass, IMyInterface, rtcsdk::eats_all<MyClass>>
{
public:
    void *on_eat_all(const IID &id) noexcept
    {
        // Return pointer + AddRef for supported interfaces, or nullptr
        return nullptr;
    }
};
```

## `aggregates`

Delegate interface queries to another object:

```C++
class MyClass : public rtcsdk::object<MyClass,
    IMyInterface,
    rtcsdk::aggregates<MyClass, IAggregatedInterface>>
{
    rtcsdk::com_ptr<ISomeOtherInterface> member_ = initialize_member();

public:
    void *on_query(rtcsdk::interface_wrapper<IAggregatedInterface>) noexcept
    {
        IAggregatedInterface *result{};
        member_->QueryInterface(&result);
        return result;  // Already AddRef'd by QueryInterface
    }
};
```

## `intermediate`

Implementation proxies — partial interface implementations reusable across classes:

```C++
COM_INTERFACE(IMyInterface, "{AB9A7AF1-6792-4D0A-83BE-8252A8432B45}")
{
    virtual void Method1() = 0;
    virtual void Method2() = 0;
};

// Partial implementation
class MyInterfaceImpl : public rtcsdk::intermediate<MyInterfaceImpl, IMyInterface>
{
    void Method1() override { /* default implementation */ }
};

// Final class — only needs to implement Method2
class MyClass : public rtcsdk::object<MyClass, MyInterfaceImpl>
{
    void Method2() override { /* ... */ }
};
```

## `object_holder`

Returned by `create_instance()`. Provides:

| Method | Description |
|--------|-------------|
| `to_ptr() &&` | Convert to `com_ptr<DefaultInterface>` |
| `to_ptr<Interface>() &&` | Convert to `com_ptr<Interface>` |
| `obj() const` | Access the raw `Derived*` for additional initialization |

```C++
// Additional initialization before handing out the interface pointer
auto holder = MyObject::create_instance();
holder.obj()->additional_init();
auto ptr = std::move(holder).to_ptr();
```
