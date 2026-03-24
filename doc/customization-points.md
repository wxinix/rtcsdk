# Object Customization Points

Customization points are optional public methods on your `Derived` class that execute at specific object lifetime events.

## `final_construct`

Called after reference-counting is fully initialized — use this when you need the object to be a valid COM object during initialization (e.g., passing `this` to external APIs).

```C++
class MyObject : public rtcsdk::object<MyObject, IMyInterface>
{
public:
    template<typename... Args>
    HRESULT final_construct(Args &&...args)
    {
        // Reference counting is active here — safe to make COM calls
        return S_OK;
    }
};

// Pass rtcsdk::delayed as first arg to route args to final_construct instead of ctor:
auto obj = MyObject::create_instance(rtcsdk::delayed, arg1, arg2).to_ptr();
```

`final_construct` may throw exceptions or return non-zero `HRESULT`. A non-zero return throws `bad_hresult`.

## `final_release`

Static method called when reference count reaches zero. Takes ownership of the object via `unique_ptr`:

```C++
class MyObject : public rtcsdk::object<MyObject, IMyInterface>
{
public:
    static void final_release(std::unique_ptr<MyObject> ptr) noexcept
    {
        // Custom cleanup before destruction
        ptr->cleanup();
        // Object destroyed at end of scope
    }
};
```

For aggregated objects, use a template variant:

```C++
template<typename D>
static void final_release(std::unique_ptr<D> ptr) noexcept
{
    if constexpr (std::is_same_v<D, MyObject>) {
        ptr->do_cleanup();
    } else {
        ptr->get()->do_cleanup();  // Access the actual object from aggregate
    }
}
```

## `on_add_ref` / `on_release`

Called on every reference count change:

```C++
void on_add_ref(int new_counter_value);
void on_release(int new_counter_value);
```

## `pre_query_interface`

Called **before** standard `QueryInterface` processing:

```C++
HRESULT pre_query_interface(REFIID iid, void **ppresult) noexcept;
```

* Return `S_OK` with a valid pointer (after calling `AddRef`) to short-circuit
* Return `E_NOINTERFACE` to fall through to standard processing
* Return any other error to abort (must store `nullptr` at `*ppresult`)

## `post_query_interface`

Called **after** standard processing fails to find the interface:

```C++
HRESULT post_query_interface(REFIID iid, void **ppresult) noexcept;
```

* Return `S_OK` with a valid pointer (after calling `AddRef`) to provide the interface
* Return `E_NOINTERFACE` with `nullptr` at `*ppresult` otherwise
