# Constructing Objects

## Simple Construction in the Heap

Call `Derived::create_instance(args...)` — arguments are forwarded to the constructor:

```C++
auto obj = MyObject::create_instance(42, "hello").to_ptr();
auto specific = MyObject::create_instance().to_ptr<ISecondInterface>();
```

Pass `rtcsdk::delayed` as the first argument to use the default constructor and forward remaining arguments to [`final_construct`](customization-points.md#final_construct):

```C++
auto obj = MyObject::create_instance(rtcsdk::delayed, arg1, arg2).to_ptr();
```

The returned [`object_holder`](interfaces.md#object_holder) supports additional initialization before converting to a `com_ptr`.

## Simple Construction on the Stack

For short-lived objects with a known scope:

```C++
void bar(rtcsdk::ref<IMyInterface> p) { /* ... */ }

void foo()
{
    rtcsdk::value_on_stack<MyClass> obj{/* constructor args */};
    bar(&obj);
}
```

`AddRef`/`Release` are no-ops for stack objects, but in debug builds the destructor asserts on mismatched call counts.

## Default Construction Mechanism

A generic mechanism for runtime object construction by CLSID, without knowing the concrete class.

### Registering a class

```C++
// Auto-detect CLSID from the class
OBJ_ENTRY_AUTO(MyClass)

// Or register with an explicit CLSID
OBJ_ENTRY_AUTO2("{FFDBB4B7-8ECB-42FE-BF68-163B1E0829A2}"_guid, MyClass)
```

To attach a CLSID to a class:

```C++
// Free-standing
DEFINE_CLASS(MyClass, "{FFDBB4B7-8ECB-42FE-BF68-163B1E0829A2}")

// Or inside the class
class MyClass : public rtcsdk::object<MyClass, IMyInterface>
{
public:
    CLASS_GUID("{FFDBB4B7-8ECB-42FE-BF68-163B1E0829A2}")
};
```

### Creating objects by CLSID

```C++
// Non-throwing — returns HRESULT
HRESULT hr = rtcsdk::create_object(clsid, iid, &ppv);

// Non-throwing — fills a com_ptr
rtcsdk::com_ptr<IMyInterface> obj;
HRESULT hr = rtcsdk::create_object(clsid, obj);

// Throwing — returns com_ptr or throws bad_hresult
auto obj = rtcsdk::create_object<IMyInterface>(clsid);
```

`create_object` respects the [singleton](traits.md#singleton_factory) and [single_cached_instance](traits.md#single_cached_instance) traits.
