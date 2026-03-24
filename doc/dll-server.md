# Implementing a COM DLL Server

```C++
#include <rtcsdk/factory.h>
```

`create_object` (see [Constructing Objects](constructing-objects.md)) serves as the foundation for `DllGetClassObject`. Add the following to one of your `.cpp` files:

```C++
HRESULT_export CALLBACK DllCanUnloadNow()
{
    return rtcsdk::DllCanUnloadNow();
}

HRESULT_export CALLBACK DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppvObj)
{
    return rtcsdk::DllGetClassObject(rclsid, riid, ppvObj);
}
```

Make sure your classes are registered with [`OBJ_ENTRY_AUTO`](constructing-objects.md#registering-a-class) and use the [`implements_module_count`](traits.md#implements_module_count) trait where appropriate.
