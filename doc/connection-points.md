# Connection Points

```C++
#include <rtcsdk/connection_point.h>
```

Connection points are the standard COM mechanism for event notifications. A source object exposes `IConnectionPointContainer`, clients subscribe through `IConnectionPoint::Advise`, and the source fires events to all connected sinks.

rtcsdk provides this via a single template mixin — no macros, no maps, no ATL.

## Declaring an Event Source

Add `rtcsdk::connection_points<Derived, EventInterfaces...>` to your object's interface list:

```C++
// Define event interfaces
COM_INTERFACE(IMyEvents, "{...}")
{
    virtual HRESULT on_data(int value) = 0;
    virtual HRESULT on_complete() = 0;
};

COM_INTERFACE(IOtherEvents, "{...}")
{
    virtual HRESULT on_status(int code) = 0;
};

// Define the server interface
COM_INTERFACE(IMyServer, "{...}")
{
    virtual HRESULT do_work() = 0;
};

// Implement the server with connection points
class MyServer : public rtcsdk::object<MyServer, IMyServer,
    rtcsdk::connection_points<MyServer, IMyEvents, IOtherEvents>>
{
public:
    HRESULT do_work() override
    {
        // Fire to all connected IMyEvents sinks
        fire<IMyEvents>([](IMyEvents *sink) {
            sink->on_data(42);
            sink->on_complete();
        });
        return S_OK;
    }
};
```

The mixin automatically:
- Adds `IConnectionPointContainer` to `QueryInterface`
- Creates one `IConnectionPoint` per event interface
- Implements `FindConnectionPoint`, `EnumConnectionPoints`, `Advise`, `Unadvise`, `EnumConnections`

## Subscribing to Events (Client Side)

```C++
// Implement the event sink
class MyEventSink : public rtcsdk::object<MyEventSink, IMyEvents>
{
public:
    HRESULT on_data(int value) override { /* handle */ return S_OK; }
    HRESULT on_complete() override { /* handle */ return S_OK; }
};

// Subscribe
auto server = MyServer::create_instance().to_ptr<IMyServer>();
auto sink = MyEventSink::create_instance().to_ptr<IMyEvents>();

rtcsdk::com_ptr<IConnectionPointContainer> cpc{server};
rtcsdk::com_ptr<IConnectionPoint> cp;
cpc->FindConnectionPoint(rtcsdk::get_interface_guid<IMyEvents>(), cp.put());

DWORD cookie{};
cp->Advise(sink.get(), &cookie);

// ... use server, receive events ...

// Unsubscribe
cp->Unadvise(cookie);
```

## Firing Events

The `fire<Event>(callable)` method is available as a protected member of your class. It:

1. Takes a snapshot of connected sinks under a lock
2. Releases the lock
3. Calls your callable with each sink pointer

This snapshot pattern prevents deadlocks when sinks call back into the source during event handling.

```C++
// Fire with a lambda
fire<IMyEvents>([](IMyEvents *sink) {
    sink->on_data(42);
});

// Check if anyone is listening before expensive work
if (has_sinks<IMyEvents>()) {
    auto data = compute_expensive_data();
    fire<IMyEvents>([&](IMyEvents *sink) {
        sink->on_data(data);
    });
}
```

## Thread Safety

All connection point operations (`Advise`, `Unadvise`, `fire`) are thread-safe. The implementation uses `srwlock` for synchronization. The fire pattern (snapshot + release lock + call) prevents deadlocks when sinks re-enter the source.

## Multiple Event Interfaces

List all event interfaces in the template:

```C++
class MyServer : public rtcsdk::object<MyServer, IMyServer,
    rtcsdk::connection_points<MyServer, IMyEvents, IOtherEvents, IMoreEvents>>
{
    // fire<IMyEvents>(...), fire<IOtherEvents>(...), fire<IMoreEvents>(...) all available
};
```

Each event interface gets its own independent `IConnectionPoint` with its own sink list and cookies.

## Standard COM Compliance

The implementation is fully compliant with the standard COM connection point protocol:

- `IConnectionPointContainer::FindConnectionPoint` — finds by IID
- `IConnectionPointContainer::EnumConnectionPoints` — enumerates all points
- `IConnectionPoint::Advise` — QIs the sink for the event interface, returns a cookie
- `IConnectionPoint::Unadvise` — removes by cookie
- `IConnectionPoint::GetConnectionInterface` — returns the event IID
- `IConnectionPoint::GetConnectionPointContainer` — returns the container
- `IConnectionPoint::EnumConnections` — enumerates active connections

This means non-C++ clients (.NET via COM Interop, VBScript, PowerShell) can subscribe to events using their standard connection point mechanisms.
