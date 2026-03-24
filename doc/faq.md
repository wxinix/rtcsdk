# FAQ

## Why "Real Thin COM SDK"?

The name reflects what the library is — and what it isn't. ATL, the traditional way to write COM in C++, is a heavyweight framework: macros, COM maps, registration wizards, CComObject wrappers, IDL files, a runtime dependency on `atl*.dll`. It does a lot, but most of it you don't need.

rtcsdk strips COM support down to the essentials. The entire library is three header files with no dependencies beyond the C++ standard library and Windows SDK. There is no code generation, no registration infrastructure, no COM maps — just C++ templates that generate `QueryInterface`, `AddRef`, and `Release` at compile time. Your class inherits from `rtcsdk::object`, and that's it.

"Real thin" means the layer between your code and raw COM is as thin as it can be while still being useful: smart pointers, automatic `IUnknown` implementation, compile-time GUID resolution, and nothing else you didn't ask for.

## Is COM still relevant?

COM is far from gone — it remains the hidden infrastructure layer beneath much of the modern Microsoft stack. WinRT is built on top of COM. WinUI 3, the current native UI framework, uses COM under the hood. Office applications, DirectX, Media Foundation, Shell extensions, the Windows Runtime itself — all COM. Even newer technologies like WASDK (Windows App SDK) are fundamentally COM-based, just with friendlier projections on top.

.NET on Windows is also heavily built on COM. The CLR itself is a COM component, COM Interop is a first-class feature, and many framework APIs (WPF, WinForms, Office interop) are wrappers over COM interfaces. The cross-platform .NET runtime (.NET Core / .NET 5+) on Linux and macOS does not use COM — it was rewritten to eliminate platform-specific dependencies — but on Windows, COM remains deeply embedded in the .NET runtime and ecosystem.

If you're writing native Windows software that interoperates with the OS or other components at the binary level, COM is still the mechanism that makes it work.

## Why Windows and MSVC only?

COM is a Windows technology, but the underlying idea — binary interconnectivity between native components with a stable C++ ABI — is platform-agnostic. COM can also serve as a way to establish Inversion of Control principles in code.

There are a few Windows-specific bindings in the library that could be decoupled relatively easily, after which it could be used on other platforms.

## What served as inspiration for this library?

The library was initially created to modernize an old codebase that used ATL for COM support. It was inspired by early works by Kenny Kerr on his `moderncpp` project (which later became C++/WinRT). The library may share some common ideas (but not implementation) with C++/WinRT. It does not depend on WinRT and does not require Windows 10.

## How does this relate to Alexander Bessonov's moderncom?

rtcsdk is inspired by Alexander Bessonov's [`moderncom`](https://github.com/AlexBAV/moderncom) project. It shares the same design philosophy — pure-C++ COM without ATL — but **the implementation is entirely new**, targeting C++23 with concepts, `<stacktrace>`, and constexpr GUID parsing, and includes additional features like the IDL generator tool and connection points.

## Can I use macros or templates to declare interfaces?

Both. The `COM_INTERFACE` macro is the convenient path, but you can also declare interfaces directly using `rtcsdk::extends<>` templates. See [COM Interface Support](interfaces.md#declaring-interfaces) for both approaches with examples.

## Can I use this library to consume existing COM objects?

Yes. The `rtcsdk::com_ptr` smart pointer works with any COM interface — you don't need to declare or implement anything with rtcsdk to use it as a COM client.

**In-process COM** (DLLs loaded into your process):

```C++
#include <rtcsdk/rtcsdk.h>

rtcsdk::com_ptr<IStream> stream;
stream.CoCreateInstance(CLSID_MyStream);  // standard CoCreateInstance
stream->Write(data, size, nullptr);

// Or use the static factory that throws on failure:
auto stream = rtcsdk::com_ptr<IStream>::create(CLSID_MyStream);
```

**Out-of-process COM** (separate process, marshaled calls):

```C++
// Same API — just pass a different class context
rtcsdk::com_ptr<IMyService> service;
service.CoCreateInstance(CLSID_MyService, nullptr, CLSCTX_LOCAL_SERVER);
```

The library doesn't distinguish between in-process and out-of-process COM — that's handled by the COM runtime. `rtcsdk::com_ptr` is just a smart pointer around `IUnknown` and works with any COM object the system can activate, including DCOM remote objects (`CLSCTX_REMOTE_SERVER`).

For **implementing** COM servers (DLLs that expose objects to other processes), see [COM DLL Server](dll-server.md) and [Constructing Objects](constructing-objects.md).

## Does it handle COM callbacks, event sinks, and connection points?

**COM callbacks** — yes, fully supported. A callback is just a COM interface that the client implements and passes to the server. Use `rtcsdk::object` to implement the callback interface, create an instance, and hand it over:

```C++
// Define the callback interface and the server interface
COM_INTERFACE(IMyCallback, "{...}")
{
    virtual HRESULT on_data_received(int size) = 0;
};

COM_INTERFACE(IMyServer, "{...}")
{
    virtual HRESULT register_callback(IMyCallback *callback) = 0;
};

// Implement the callback
class MyCallback : public rtcsdk::object<MyCallback, IMyCallback>
{
public:
    HRESULT on_data_received(int size) override { /* handle it */ return S_OK; }
};

// Pass to the server
auto callback = MyCallback::create_instance().to_ptr();
server->register_callback(callback.get());
```

**Event sinks and connection points** — fully supported via the `connection_points<>` template mixin. Add it to your object's interface list and you get full `IConnectionPointContainer`/`IConnectionPoint` support — no macros, no maps:

```C++
class MyServer : public rtcsdk::object<MyServer, IMyServer,
    rtcsdk::connection_points<MyServer, IMyEvents>>
{
    void notify()
    {
        fire<IMyEvents>([](IMyEvents *sink) { sink->on_data(42); });
    }
};
```

Clients subscribe using the standard COM connection point protocol, which means .NET, VBScript, and PowerShell can also receive events. See [Connection Points](connection-points.md) for full documentation.

## Does it support COM apartment models?

The library itself is **apartment-agnostic** — it does not enforce or assume any particular threading model. Your objects can live in any apartment (STA, MTA, or NTA) depending on how you register them and how the hosting process calls `CoInitializeEx`.

What this means in practice:

- **STA (Single-Threaded Apartment)** — If your objects are registered with `ThreadingModel=Apartment` and created from an STA thread, COM guarantees all calls are serialized to that thread. Your objects don't need internal synchronization.

- **MTA (Multi-Threaded Apartment)** — If registered with `ThreadingModel=Free` or `Both`, your objects can receive concurrent calls from multiple threads. You are responsible for thread safety in your implementations. The library's ref counting uses `std::atomic` with correct memory ordering (`relaxed` for AddRef, `acq_rel` for Release) and is safe for concurrent use.

- **Free-threaded marshaler** — Not built in. If you need it, implement `IMarshal` via `aggregates<>` and delegate to `CoCreateFreeThreadedMarshaler`.

The library provides thread-safe primitives you can use in your implementations:
- `rtcsdk::srwlock` — slim reader-writer lock (wraps Windows SRWLOCK with SAL annotations)
- `connection_points<>` — internally thread-safe (all sink operations are lock-protected)

For DLL servers, you set the threading model in your `.rgs` registration script or registry entries — the library doesn't interfere with this. For in-process objects created directly via `create_instance()`, the apartment is whatever the calling thread has initialized.
