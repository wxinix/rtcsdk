# Automatic Leak Detection

The library provides built-in leak detection that shows not only *which* objects were leaked, but also the stack traces of the corresponding unmatched `AddRef` calls. Debug builds only.

## Compile-Time Configuration

Two macros control leak detection and debug ref checking. Define them **before** including any library header:

| Macro                          | Effect                                                                                                                                                                                                                                                |
|--------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `RTCSDK_COM_NO_LEAK_DETECTION` | Disables leak detection entirely. Removes the `<stacktrace>` dependency and all leak-tracking machinery. Define this if your compiler doesn't support C++23 `<stacktrace>`, or if you want faster debug builds.                                       |
| `RTCSDK_NO_CHECKED_REFS`       | Disables debug lifetime checks on `rtcsdk::ref` objects constructed from temporary `com_ptr` values. Without this, the library tracks ref-to-ptr relationships in debug builds and asserts if a `com_ptr` is destroyed while a `ref` still points to it. |

These macros set two internal flags:

| Internal flag               | Set when                                                        | Effect                                                                                                 |
|-----------------------------|-----------------------------------------------------------------|--------------------------------------------------------------------------------------------------------|
| `RTCSDK_HAS_LEAK_DETECTION` | `_DEBUG` defined AND `RTCSDK_COM_NO_LEAK_DETECTION` not defined | Enables `<stacktrace>`, `<atomic>`, `<mutex>` includes; activates cookie-based AddRef/Release tracking |
| `RTCSDK_HAS_CHECKED_REFS`   | `_DEBUG` defined AND `RTCSDK_NO_CHECKED_REFS` not defined       | Enables `<vector>` include; activates ref-to-ptr lifetime tracking                                     |

Example:

```C++
// Disable leak detection but keep checked refs
#define RTCSDK_COM_NO_LEAK_DETECTION
#include <rtcsdk/rtcsdk.h>
```

```C++
// Disable everything — fastest debug builds, no extra diagnostics
#define RTCSDK_COM_NO_LEAK_DETECTION
#define RTCSDK_NO_CHECKED_REFS
#include <rtcsdk/rtcsdk.h>
```

In release builds (`_DEBUG` not defined), both features are always disabled regardless of macro settings — zero overhead.

## Prerequisites

1. **C++23 `<stacktrace>`** — required when leak detection is enabled. If your compiler doesn't support it, define `RTCSDK_COM_NO_LEAK_DETECTION`.

2. **Call `rtcsdk::init_leak_detection()`** once at startup, before creating any `com_ptr` or objects. This call is empty in release builds and when leak detection is disabled.

3. **Opt in per class** with the [`enable_leak_detection`](traits.md#enable_leak_detection) trait. Only classes that opt in are tracked — this keeps overhead minimal:

   ```C++
   class SuspiciousObject
       : public rtcsdk::object<SuspiciousObject, IMyInterface>
       , public rtcsdk::enable_leak_detection
   { ... };
   ```

## How It Works

When leak detection is enabled:

1. Every `AddRef` (via `com_ptr`) captures a stack trace and assigns a unique cookie
2. Every matching `Release` removes that cookie and its stack trace
3. When you find a leaked object in the debugger, expand its `umb_usages` member — it contains the stack traces of all unmatched `AddRef` calls

The tracking uses a per-object map protected by `srwlock`, and cookies are passed through thread-local storage (TLS) between `com_ptr` and the tracked object.

## Usage

1. Run your program under the Visual Studio debugger
2. Use other facilities (VS built-in tools, tracing, stack objects with debug assertions) to identify leaked objects
3. Add the leaked object to the Watch window and expand until you find the `umb_usages` member
4. `umb_usages` contains stack traces of `AddRef` calls that were never matched by `Release`

## Checked Refs

When `RTCSDK_HAS_CHECKED_REFS` is active, `rtcsdk::ref` objects constructed from temporary `com_ptr` values register themselves with the parent `com_ptr`. If the `com_ptr` is destroyed while refs still exist, an assertion fires. This catches dangling-ref bugs early.

## Limitations

* Leak detection is built into `com_ptr` and `object` — it cannot track `AddRef`/`Release` calls made by external components (raw pointers, other smart pointer libraries)
* It does not automatically *detect* leaks; once a leaked object is found by other means, the library provides the diagnostic detail (stack traces of unmatched AddRef calls)
* Leak detection adds overhead: stack trace capture on every AddRef/Release, a per-object map, and a global TLS slot. Only enable it for classes you're actively investigating
