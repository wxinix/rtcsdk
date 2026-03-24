# Traits

A trait is a special class that your `Derived` class directly inherits from to change defaults. Some traits automatically propagate down the inheritance chain — if an implementation proxy or interface class specifies a trait, all derived final classes inherit it.

## `singleton_factory`

When constructed via the [default construction mechanism](constructing-objects.md#default-construction-mechanism), use a single global instance. The object is created on first request and lives until program exit. Thread-safe.

```C++
class MyService
    : public rtcsdk::object<MyService, IMyService>
    , public rtcsdk::singleton_factory
{
    // ...
};
```

## `single_cached_instance`

A weak singleton: created on first request, cached for subsequent requests, destroyed when all references are released. Thread-safe.

```C++
class MyCache
    : public rtcsdk::object<MyCache, IMyCache>
    , public rtcsdk::single_cached_instance
{
    // ...
};
```

## `supports_aggregation`

Marks the class as supporting COM aggregation (construction via `create_aggregate` or default construction with non-null `pOuterUnknown`). Omit this trait for a more efficient implementation when aggregation is not needed.

```C++
class MyObject
    : public rtcsdk::object<MyObject, IMyInterface>
    , public rtcsdk::supports_aggregation
{
    // ...
};
```

## `implements_module_count`

Increments the global module reference count while instances exist. Used with `DllCanUnloadNow`.

```C++
class MyObject
    : public rtcsdk::object<MyObject, IMyInterface>
    , public rtcsdk::implements_module_count
{
    // ...
};
```

## `enable_leak_detection`

Turns on [automatic leak detection](leak-detection.md) for this class (debug builds only).

```C++
class SuspiciousObject
    : public rtcsdk::object<SuspiciousObject, IMyInterface>
    , public rtcsdk::enable_leak_detection
{
    // ...
};
```
