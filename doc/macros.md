# Macro Reference

## Summary

| Preferred                                            | Legacy                              | Purpose                                                                 |
|------------------------------------------------------|-------------------------------------|-------------------------------------------------------------------------|
| `COM_INTERFACE(name, id)`                            | `RTCSDK_DEFINE_INTERFACE`           | Declare a COM interface derived from `IUnknown`                         |
| `COM_INTERFACE_BASE(name, base, id)`                 | `RTCSDK_DEFINE_INTERFACE_BASE`      | Declare a COM interface derived from another interface                   |
| `DEFINE_CLASS(name, id)`                             | `RTCSDK_DEFINE_CLASS`               | Define a named `constexpr GUID` constant                                |
| `CLASS_GUID(id)`                                     | `RTCSDK_CLASS_GUID`                 | Attach a CLSID inside a class body (via `static constexpr get_guid()`)  |
| `CLASS_GUID_EXISTING(id)`                            | `RTCSDK_CLASS_GUID_EXISTING`        | Attach an existing GUID variable as CLSID                               |
| `OBJ_ENTRY_AUTO(class)`                              | `RTCSDK_OBJ_ENTRY_AUTO`            | Register class for CLSID-based construction (auto CLSID)                |
| `OBJ_ENTRY_AUTO2(clsid, class)`                      | `RTCSDK_OBJ_ENTRY_AUTO2`           | Register class with explicit CLSID                                      |
| `OBJ_ENTRY_AUTO2_NAMED(clsid, class, name)`          | `RTCSDK_OBJ_ENTRY_AUTO2_NAMED`     | Register with explicit CLSID and linker symbol name                     |
| `OBJ_ENTRY_AUTO_ATL_COMPAT(clsid, class)`            | `RTCSDK_OBJ_ENTRY_AUTO_ATL_COMPAT` | Register ATL-compatible class (only when `<atlbase.h>` is included)     |

**Internal macros** (not intended for direct use):

| Macro                                                | Purpose                                                                                                        |
|------------------------------------------------------|----------------------------------------------------------------------------------------------------------------|
| `RTCSDK_GUID_HELPER_(name, id)`                      | Forward-declares struct and defines `get_guid` free function. Used by `COM_INTERFACE` / `COM_INTERFACE_BASE`.  |
| `RTCSDK_OBJ_ENTRY_PRAGMA(class)`                     | Platform-specific linker pragma for the object map. Used by `OBJ_ENTRY_AUTO*`.                                 |
| `RTCSDK_HAS_LEAK_DETECTION`                          | Set to `0` or `1` based on `RTCSDK_COM_NO_LEAK_DETECTION`.                                                    |
| `RTCSDK_HAS_CHECKED_REFS`                            | Set to `0` or `1` based on `RTCSDK_NO_CHECKED_REFS`.                                                          |

**User-defined opt-out macros** (define before including headers):

| Macro                                                | Effect                                                               |
|------------------------------------------------------|----------------------------------------------------------------------|
| `RTCSDK_COM_NO_LEAK_DETECTION`                       | Disable leak detection entirely (removes `<stacktrace>` dependency)  |
| `RTCSDK_NO_CHECKED_REFS`                             | Disable debug lifetime checks on `rtcsdk::ref` from temporaries         |

## Quick Examples

```C++
// Declare an interface
COM_INTERFACE(IMyInterface, "{AB9A7AF1-6792-4D0A-83BE-8252A8432B45}")
{
    virtual int do_something() const noexcept = 0;
};

// Declare a derived interface
COM_INTERFACE_BASE(IMyExtended, IMyInterface, "{C3E1F2A4-7890-4B5C-A1D2-E3F4567890AB}")
{
    virtual void do_more() = 0;
};

// Implement and register for CLSID-based construction
class MyObject : public rtcsdk::object<MyObject, IMyInterface>
{
public:
    CLASS_GUID("{FFDBB4B7-8ECB-42FE-BF68-163B1E0829A2}")
    int do_something() const noexcept override { return 42; }
};

OBJ_ENTRY_AUTO(MyObject)

// Now creatable by CLSID:
auto obj = rtcsdk::create_object<IMyInterface>(
    "{FFDBB4B7-8ECB-42FE-BF68-163B1E0829A2}"_guid);
```
