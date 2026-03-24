# COM Interface Smart Pointer

```C++
#include <rtcsdk/rtcsdk.h>
```

This header provides two template classes: `rtcsdk::com_ptr` (owning smart pointer) and `rtcsdk::ref` (non-owning reference).

## `rtcsdk::com_ptr`

```C++
template<typename Interface>
class com_ptr;
```

A reference-counting smart pointer for COM interfaces. The library must be able to [fetch an IID](guid-helpers.md) from the `Interface` type.

### Constructors

| # | Signature | Description |
|---|-----------|-------------|
| 1 | `com_ptr()` / `com_ptr(std::nullptr_t)` | Default/null — creates an empty pointer |
| 2 | `com_ptr(Interface *)` | From raw pointer — calls `AddRef` |
| 3 | `com_ptr(rtcsdk::attach_t, Interface *)` | Attaching — does **not** call `AddRef`. Use `rtcsdk::attach` as the first argument |
| 4 | `com_ptr(OtherInterface *)` | From other raw pointer — `static_cast` + `AddRef` if related, otherwise `QueryInterface` |
| 5 | `com_ptr(ref<Interface> p)` | From `rtcsdk::ref` |
| 6 | `com_ptr(const com_ptr &)` | Copy |
| 7 | `com_ptr(com_ptr &&)` | Move |
| 8 | `com_ptr(const com_ptr<OtherInterface> &)` | Cross-type copy — `static_cast` or `QueryInterface` |
| 9 | `com_ptr(com_ptr<OtherInterface> &&)` | Cross-type move — `static_cast` or `QueryInterface` |

A symmetric set of assignment operators is also provided.

### Methods

| Method | Description |
|--------|-------------|
| `explicit operator bool() const noexcept` | Checks if not empty |
| `release()` / `reset()` | Releases the interface and empties the pointer |
| `Interface *operator ->() const noexcept` | Dereferences |
| `bool operator ==(const com_ptr &) const noexcept` | Equality (`!=` is synthesized) |
| `bool operator <(const com_ptr &) const noexcept` | Ordering |
| `void attach(Interface *p) noexcept` | Attaches raw pointer (asserts if not empty) |
| `[[nodiscard]] Interface *detach() noexcept` | Detaches and returns the raw pointer |
| `Interface *get() const noexcept` | Returns the raw pointer |
| `Interface **put() noexcept` | Write access to raw pointer (asserts if not empty) |
| `auto as<OtherInterface>() const noexcept` | Constructs another smart pointer via `QueryInterface` |
| `HRESULT QueryInterface(OtherInterface **) const` | Raw `QueryInterface` |
| `HRESULT CoCreateInstance(clsid, pOuter, ctx)` | Calls `::CoCreateInstance` and stores result |
| `HRESULT create_instance(clsid, pOuter, ctx)` | Same as `CoCreateInstance` |
| `static com_ptr create(clsid, pOuter, ctx)` | Static — creates or throws `bad_hresult` |

### Example

```C++
#include <rtcsdk/rtcsdk.h>

// Create via CoCreateInstance
rtcsdk::com_ptr<IStream> stream;
stream.CoCreateInstance(CLSID_MyStream);

// Query for another interface
auto unknown = stream.as<IUnknown>();

// Attach without AddRef (e.g., from a function that returns a raw pointer)
IStream *raw = get_stream_no_addref();
rtcsdk::com_ptr<IStream> owned{rtcsdk::attach, raw};
```

## `rtcsdk::ref`

```C++
template<typename Interface>
class ref;
```

A non-owning reference to a COM interface — use it in place of raw `Interface *` when you don't need to call `AddRef`/`Release`. In debug builds, lifetime checks catch dangling references with zero overhead in release builds.

### Constructors

| # | Signature | Notes |
|---|-----------|-------|
| 1 | `ref()` / `ref(std::nullptr_t)` | Empty |
| 2 | `ref(Interface *p)` | From raw pointer |
| 3 | `ref(const com_ptr<Interface> &)` | From owning pointer |
| 4 | `ref(com_ptr<Interface> &&)` | From temporary — debug lifetime checks unless `RTCSDK_NO_CHECKED_REFS` is defined |
| 5 | `ref(const com_ptr<OtherInterface> &)` | Cross-type — `OtherInterface` must derive from `Interface` |
| 6 | `ref(com_ptr<OtherInterface> &&)` | Cross-type from temporary |
| 7 | `ref(const ref<OtherInterface> &)` | Copy from related ref |

Assignment operators are **prohibited** for `ref` objects.

### Methods

| Method | Description |
|--------|-------------|
| `Interface *operator ->() const noexcept` | Dereferences |
| `Interface *get() const noexcept` | Returns raw pointer |
| `auto as<OtherInterface>() const noexcept` | Constructs a `com_ptr` via `QueryInterface` |

### Example

```C++
// Use rtcsdk::ref as a function parameter when you don't store the pointer
void serialize(rtcsdk::ref<IStream> stream)
{
    // Use stream-> as normal; no AddRef/Release overhead
    stream->Write(data, size, nullptr);
}

void foo()
{
    rtcsdk::com_ptr<IStream> stream = create_stream();
    serialize(stream);  // implicit conversion to rtcsdk::ref
}
```
