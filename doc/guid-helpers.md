# GUID Helpers

```C++
#include <rtcsdk/rtcsdk.h>
```

> This header is automatically included by all other library headers.

## Compile-Time GUID Parsing

The library provides functions that parse string GUIDs into `GUID` structures at compile time:

```C++
template<size_t N>
constexpr GUID rtcsdk::make_guid(const char(&str)[N]);
```

Supports the formats `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}` and `XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX`.

A user-defined literal (UDL) `_guid` is also provided:

```C++
constexpr GUID id = "{AB9A7AF1-6792-4D0A-83BE-8252A8432B45}"_guid;
```

## Fetching Identifiers

The library resolves interface and class GUIDs at compile time using C++23 concepts. It checks three sources in order:

1. **Static member function** — `T::get_guid()` (a `constexpr` static member returning `GUID`)
2. **Free function via ADL** — `get_guid(T*)` found by argument-dependent lookup
3. **MSVC extension** — `__uuidof(T)` as a fallback

```C++
// Concept-based detection (from guid.h):
template<typename T>
concept has_get_guid = requires { T::get_guid(); };

template<typename T>
concept has_free_get_guid = requires { get_guid(static_cast<T *>(nullptr)); };
```

You can retrieve a GUID from any type the library can resolve:

```C++
template<typename Interface>
constexpr GUID get_interface_guid() noexcept;
```

### Providing a GUID for your type

**Option 1: Static member** — define `get_guid()` directly on the class:

```C++
class MyClass {
public:
    static constexpr GUID get_guid()
    {
        return "{AB9A7AF1-6792-4D0A-83BE-8252A8432B45}"_guid;
    }
};
```

**Option 2: Free function** — specialize `get_guid` via ADL:

```C++
struct MyInterface;
inline constexpr auto get_guid(MyInterface *) noexcept
{
    return "{AB9A7AF1-6792-4D0A-83BE-8252A8432B45}"_guid;
}
```

**Option 3: MSVC `__declspec(uuid(...))`** — attach a GUID with the compiler extension:

```C++
struct __declspec(uuid("AB9A7AF1-6792-4D0A-83BE-8252A8432B45")) MyInterface;
```

**Option 4: Use a macro** — `COM_INTERFACE` and `DEFINE_CLASS` macros set up the free function automatically. See [COM Interface Support](interfaces.md).
