// Copyright (c) 2022-2026 Wuping Xin. All rights reserved.
// MIT License. See LICENSE.md for details.

#pragma once

#if !defined(RTCSDK_COM_NO_LEAK_DETECTION) && defined(_DEBUG)
#define RTCSDK_HAS_LEAK_DETECTION 1
#else
#define RTCSDK_HAS_LEAK_DETECTION 0
#endif

#if !defined(RTCSDK_COM_NO_CHECKED_REFS) && defined(_DEBUG)
#define RTCSDK_HAS_CHECKED_REFS 1
#else
#define RTCSDK_HAS_CHECKED_REFS 0
#endif

#if defined(_DEBUG)
#include <vector>
#if RTCSDK_HAS_LEAK_DETECTION
#include <algorithm>
#include <stacktrace>
#endif
#endif

#include <atomic>
#include <cassert>
#include <cstdint>
#include <exception>
#include <mutex>
#include <type_traits>
#include <unknwn.h>// NOLINT
#include <utility>

namespace rtcsdk {

namespace details {

// ── GUID parsing ──────────────────────────────────────────────────────────

constexpr size_t normal_guid_size = 36;//  00000000-0000-0000-0000-000000000000
constexpr size_t braced_guid_size = 38;// {00000000-0000-0000-0000-000000000000}

/**
 Parse hexadecimal digit char to integer value.

 @exception std::domain_error   Raised when the input character is not a hex
                                digit char.

 @param     c   The hex char.

 @returns   Integer value of the hex char.
 */
constexpr uint32_t parse_hex_digit(const char c)// NOLINT
{
    // If the constexpr evaluation ends up with the throw-expression, the
    // program is ill-formed and won't compile.
    if ('0' <= c && c <= '9')
        return c - '0';
    if ('a' <= c && c <= 'f')
        return 10 + c - 'a';
    if ('A' <= c && c <= 'F')
        return 10 + c - 'A';
    throw std::domain_error{"Invalid character in GUID"};
}

template<typename T>
constexpr T parse_hex(const char *ptr)// NOLINT
{
    constexpr size_t digits = sizeof(T) * 2;
    T result{};

    for (size_t i = 0; i < digits; ++i) {
        result |= parse_hex_digit(ptr[i]) << (4 * (digits - i - 1));
    }

    return result;
}

constexpr GUID make_guid_helper(const char *begin)// NOLINT
{
    GUID result{};
    result.Data1 = parse_hex<uint32_t>(begin);
    begin += 8 + 1;
    result.Data2 = parse_hex<uint16_t>(begin);
    begin += 4 + 1;
    result.Data3 = parse_hex<uint16_t>(begin);
    begin += 4 + 1;
    result.Data4[0] = parse_hex<uint8_t>(begin);
    begin += 2;
    result.Data4[1] = parse_hex<uint8_t>(begin);
    begin += 2 + 1;

    for (size_t i = 0; i < 6; ++i) {
        result.Data4[i + 2] = parse_hex<uint8_t>(begin + i * 2);
    }

    return result;
}

template<typename T>
concept has_get_guid = requires { T::get_guid(); };

template<typename T>
concept has_free_get_guid = requires { get_guid(static_cast<T *>(nullptr)); };

template<typename T>
struct interface_wrapper {
    using type = T;
};

template<typename T>
constexpr auto msvc_get_guid_workaround = T::get_guid();

template<typename T>
constexpr GUID get_interface_guid_impl() noexcept
{
    if constexpr (has_get_guid<T>)
        return msvc_get_guid_workaround<T>;
    else if constexpr (has_free_get_guid<T>)
        return get_guid(static_cast<T *>(nullptr));
    else
        return __uuidof(T);
}

template<typename T>
constexpr GUID get_interface_guid(interface_wrapper<T>) noexcept
{
    return get_interface_guid_impl<T>();
}

// ── error handling ────────────────────────────────────────────────────────

/**
 bad_hresult is the type of the object thrown as an error reporting a bad
 HRESULT has occurred.
 */
class bad_hresult
{
public:
    constexpr bad_hresult() = default;

    explicit constexpr bad_hresult(const HRESULT hr) noexcept : hr_{hr}
    {
    }

    [[nodiscard]] constexpr HRESULT hr() const noexcept
    {
        return hr_;
    }

    [[nodiscard]] constexpr bool is_aborted() const noexcept
    {
        return hr_ == HRESULT_FROM_WIN32(ERROR_OPERATION_ABORTED);
    }

private:
    HRESULT hr_{E_FAIL};
};

[[noreturn]] inline void throw_bad_hresult(const HRESULT hr)
{
    throw bad_hresult{hr};// NOLINT(hicpp-exception-baseclass)
}

/**
 Encode window 32 error as HRESULT and throw bad_hresult.

 @param     err The win32 error.
 */
[[noreturn]] inline void throw_win32_error(DWORD err)
{
    throw_bad_hresult(HRESULT_FROM_WIN32(err));
}

/** Throw last win32 error as bad_hresult. */
[[noreturn]] inline void throw_last_error()
{
    throw_win32_error(GetLastError());
}

/**
 Throw bad_hresult on failed HRESULT.

 @param     hr  The HRESULT value.
 */
inline void throw_on_failed(const HRESULT hr)
{
    if (FAILED(hr)) {
        throw_bad_hresult(hr);
    }
}

// ── srwlock ───────────────────────────────────────────────────────────────

/**
 Windows slim reader-writer lock wrapper. Friendly to be used as shared_mutex.
 */
class srwlock
{
public:
    srwlock(const srwlock &) = delete;
    srwlock &operator=(const srwlock &) = delete;
    srwlock() noexcept = default;

    /*
     Annotated Locking Behavior using SAL. See the following doc for details
     https://learn.microsoft.com/en-us/cpp/code-quality/annotating-locking-behavior?view=msvc-170
    */

    _Acquires_exclusive_lock_(&lock_) void lock() noexcept
    {
        AcquireSRWLockExclusive(&lock_);
    }

    _Acquires_shared_lock_(&lock_) void lock_shared() noexcept
    {
        AcquireSRWLockShared(&lock_);
    }

    _When_(return, _Acquires_exclusive_lock_(&lock_)) bool try_lock() noexcept
    {
        return 0 != TryAcquireSRWLockExclusive(&lock_);
    }

    _When_(return, _Acquires_shared_lock_(&lock_)) bool try_lock_shared() noexcept
    {
        return 0 != TryAcquireSRWLockShared(&lock_);
    }

    _Releases_exclusive_lock_(&lock_) void unlock() noexcept
    {
        ReleaseSRWLockExclusive(&lock_);
    }

    _Releases_shared_lock_(&lock_) void unlock_shared() noexcept
    {
        ReleaseSRWLockShared(&lock_);
    }

private:
    SRWLOCK lock_{};
};

// ── com_ptr / ref ─────────────────────────────────────────────────────────

#if RTCSDK_HAS_LEAK_DETECTION

struct leak_detection {
    int ordinal;
    std::stacktrace stack;

    static int get_next() noexcept
    {
        static std::atomic<int> global_ordinal{};
        return ++global_ordinal;
    }

    leak_detection() noexcept : ordinal{get_next()}, stack{std::stacktrace::current()}
    {
    }
};

// Debug-only leak detection initialization. Intentional design trade-offs:
// - CreateFileMappingW/MapViewOfFile results are not null-checked; failure here is
//   fatal and will be caught immediately by the assert in get_slot().
// - The section handle and mapped view are intentionally leaked — they live for the
//   process lifetime and are cleaned up by the OS on exit.
inline void init_leak_detection() noexcept
{
    using namespace std::literals;
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr,
                                        PAGE_READWRITE,
                                        0,
                                        si.dwPageSize,
                                        (L"RTCSDK_LEAK_DETECTION_SECTION."s + std::to_wstring(GetCurrentProcessId())).c_str());

    *reinterpret_cast<DWORD *>(MapViewOfFile(section, FILE_MAP_WRITE, 0, 0, si.dwPageSize)) = TlsAlloc();
}

inline DWORD get_slot() noexcept
{
    using namespace std::literals;
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE,
                                        nullptr,
                                        PAGE_READWRITE,
                                        0,
                                        si.dwPageSize,
                                        (L"RTCSDK_LEAK_DETECTION_SECTION."s + std::to_wstring(GetCurrentProcessId())).c_str());

    assert(section && GetLastError() == ERROR_ALREADY_EXISTS && "init_leak_detection() must be called from EXE "
                                                                "module as early as possible! "
                                                                "It has not yet been called.");

    auto ptr = reinterpret_cast<DWORD *>(MapViewOfFile(section, FILE_MAP_WRITE, 0, 0, si.dwPageSize));

    assert(ptr);
    CloseHandle(section);
    auto ret = *ptr;
    UnmapViewOfFile(ptr);
    return ret;
}

inline void set_current_cookie(int cookie) noexcept
{
    static DWORD slot = get_slot();
    TlsSetValue(slot, reinterpret_cast<void *>(static_cast<uintptr_t>(cookie)));
}

inline int get_current_cookie() noexcept
{
    static DWORD slot = get_slot();
    auto retval = TlsGetValue(slot);
    TlsSetValue(slot, nullptr);
    return static_cast<int>(reinterpret_cast<uintptr_t>(retval));
}
#else
inline void init_leak_detection() noexcept
{
}
#endif

struct attach_t {
};

template<typename T>
class ref;

template<typename T>
class __declspec(empty_bases) com_ptr
{
    friend class ref<T>;

    friend T *&internal_get(com_ptr<T> &obj) noexcept
    {
        return obj.p_;
    }

private:
    T *p_{};

#if RTCSDK_HAS_CHECKED_REFS
    std::vector<ref<T> *> weaks_;
#endif

#if RTCSDK_HAS_LEAK_DETECTION
    int cookie_{};

    friend int &internal_get_cookie(com_ptr<T> &obj) noexcept
    {
        return obj.cookie_;
    }
#endif

#if RTCSDK_HAS_LEAK_DETECTION
    void store_cookie() noexcept
    {
        cookie_ = get_current_cookie();
    }
#else
    static void store_cookie() noexcept
    {
    }
#endif

    static void addref_pointer(IUnknown *pint) noexcept
    {
        if (pint) {
            pint->AddRef();
            store_cookie();
        }
    }

    void release_pointer(IUnknown *pint) noexcept
    {
        if (pint) {
#if RTCSDK_HAS_LEAK_DETECTION
            set_current_cookie(std::exchange(cookie_, 0));
#endif
            pint->Release();
        }
    }

public:
    explicit operator bool() const noexcept
    {
        return !!p_;
    }

    com_ptr() = default;
    explicit com_ptr(std::nullptr_t) noexcept
    {
    }

    com_ptr(T *p) noexcept : p_{p}// NOLINT(google-explicit-constructor)
    {
        addref_pointer(p);
    }

    // the following constructor does not call addref (attaching constructor)
    com_ptr(attach_t, T *p) noexcept : p_{p}
    {
        store_cookie();// might get incorrect cookie, check
    }

    template<typename OtherInterface>
    explicit com_ptr(OtherInterface *punk) noexcept
    {
        if constexpr (std::is_base_of_v<T, OtherInterface>) {
            p_ = static_cast<T *>(punk);
            addref_pointer(p_);
        } else {
            if (punk) {
                punk->QueryInterface(get_interface_guid(interface_wrapper<T>{}), reinterpret_cast<void **>(&p_));
                store_cookie();
            }
        }
    }

    template<typename OtherInterface>
    explicit com_ptr(const com_ptr<OtherInterface> &o) noexcept
    {
        if constexpr (std::is_base_of_v<T, OtherInterface>) {
            p_ = static_cast<T *>(o.get());
            addref_pointer(p_);
        } else {
            if (o) {
                o->QueryInterface(get_interface_guid(interface_wrapper<T>{}), reinterpret_cast<void **>(&p_));
                store_cookie();
            }
        }
    }

    template<typename OtherInterface>
    explicit com_ptr(com_ptr<OtherInterface> &&o) noexcept
    {
        if constexpr (std::is_base_of_v<T, OtherInterface>) {
            p_ = static_cast<T *>(o.get());
#if RTCSDK_HAS_LEAK_DETECTION
            cookie_ = internal_get_cookie(o);
            internal_get_cookie(o) = 0;
#endif
            internal_get(o) = nullptr;
        } else {
            if (o) {
                if (SUCCEEDED(o->QueryInterface(get_interface_guid(interface_wrapper<T>{}), reinterpret_cast<void **>(&p_)))) {
                    store_cookie();
                    o.release();
                }
            }
        }
    }

    void release() noexcept
    {
        if (p_) {
            release_pointer(p_);
            p_ = nullptr;
        }
    }

    void reset() noexcept
    {
        release();
    }

    ~com_ptr() noexcept
    {
        release_pointer(p_);
#if RTCSDK_HAS_CHECKED_REFS
        assert(weaks_.empty() && "There was ref<Interface> constructed from this com_ptr that outlived this object!");
#endif
    }

    com_ptr(const com_ptr &o) noexcept : p_{o.p_}
    {
        addref_pointer(p_);
    }

    template<typename OtherInterface>
    explicit com_ptr(ref<OtherInterface> o) noexcept;

    com_ptr &operator=(const com_ptr &o) noexcept
    {
        if (this == &o) {
            return *this;
        }

        release_pointer(p_);
        p_ = o.p_;
        addref_pointer(p_);
        return *this;
    }

    com_ptr(com_ptr &&o) noexcept : p_{o.p_}
#if RTCSDK_HAS_LEAK_DETECTION
                                    ,
                                    cookie_{o.cookie_}
#endif
    {
        o.p_ = nullptr;
#if RTCSDK_HAS_LEAK_DETECTION
        o.cookie_ = 0;
#endif
    }

    com_ptr &operator=(com_ptr &&o) noexcept
    {
#if RTCSDK_HAS_LEAK_DETECTION
        std::swap(cookie_, o.cookie_);
#endif
        std::swap(p_, o.p_);
        return *this;
    }

    T *operator->() const noexcept
    {
        return p_;
    }

    // Comparison operators
    bool operator==(const com_ptr &o) const noexcept
    {
        return p_ == o.p_;
    }

    bool operator==(T *pother) const noexcept
    {
        return p_ == pother;
    }

    friend bool operator==(T *p1, const com_ptr &p2) noexcept
    {
        return p1 == p2.p_;
    }

    // Ordering operator
    bool operator<(const com_ptr &o) const noexcept
    {
        return p_ < o.p_;
    }

    // Attach and detach operations
    void attach(T *p) noexcept
    {
        assert(!p_ && "Using attach on non-empty com_ptr is prohibited");
        p_ = p;
        store_cookie();// might get incorrect cookie
    }

    [[nodiscard]] T *detach() noexcept
    {
        T *cur{};
        std::swap(cur, p_);
#if RTCSDK_HAS_LEAK_DETECTION
        cookie_ = 0;
#endif

        return cur;
    }

    // Pointer accessors
    [[nodiscard]] T *get() const noexcept
    {
        return p_;
    }

#if RTCSDK_HAS_LEAK_DETECTION
    int get_cookie() const noexcept
    {
        return cookie_;
    }
#endif

    [[nodiscard]] T **put() noexcept
    {
        assert(!p_ && "Using put on non-empty object is prohibited");
        return &p_;
    }

    // Conversion operations
    template<typename Interface>
    auto as() const noexcept
    {
        return com_ptr<Interface>{*this};
    }

    template<typename Interface>
    HRESULT QueryInterface(Interface **ppresult) const
    {
        return p_->QueryInterface(get_interface_guid(interface_wrapper<Interface>{}), reinterpret_cast<void **>(ppresult));
    }

    HRESULT CoCreateInstance(const GUID &clsid, IUnknown *pUnkOuter = nullptr, DWORD dwClsContext = CLSCTX_ALL) noexcept
    {
        assert(!p_ && "Calling CoCreateInstance on initialized object is prohibited");
        const auto hr = ::CoCreateInstance(clsid,
                                           pUnkOuter,
                                           dwClsContext,
                                           get_interface_guid(interface_wrapper<T>{}),
                                           reinterpret_cast<void **>(&p_));
        store_cookie();
        return hr;
    }

    HRESULT create_instance(const GUID &clsid, IUnknown *pUnkOuter = nullptr, DWORD dwClsContext = CLSCTX_ALL) noexcept
    {
        return CoCreateInstance(clsid, pUnkOuter, dwClsContext);
    }

    static com_ptr create(const GUID &clsid, IUnknown *pUnkOuter = nullptr, DWORD dwClsContext = CLSCTX_ALL)
    {
        com_ptr result;
        auto hr = result.CoCreateInstance(clsid, pUnkOuter, dwClsContext);
        if (SUCCEEDED(hr))
            return result;
        throw_bad_hresult(hr);
    }

#if RTCSDK_HAS_CHECKED_REFS
    void add_weak(ref<T> *pweak) noexcept
    {
        try {
            weaks_.push_back(pweak);
        } catch (...) {}
    }

    void remove_weak(ref<T> *pweak) noexcept
    {
        std::erase(weaks_, pweak);
    }
#endif
};

constexpr attach_t attach = {};

template<typename T>
class __declspec(empty_bases) ref
{
    T *p_{};
#if RTCSDK_HAS_CHECKED_REFS
    com_ptr<T> *parent_{};
#endif

public:
    explicit operator bool() const noexcept
    {
        return !!p_;
    }

    ref() = default;

    explicit ref(std::nullptr_t) noexcept
    {
    }

    explicit ref(const com_ptr<T> &o) noexcept : p_{o.p_}
    {
    }

    explicit ref(com_ptr<T> &&o) noexcept : p_{o.p_}
    {
#if RTCSDK_HAS_CHECKED_REFS
        parent_ = &o;
        // We allow construction from temporary com_ptr, but in DEBUG build we make sure the com_ptr lives long enough
        o.add_weak(this);
#endif
    }

    // allow construction from derived interfaces
    template<typename OtherInterface>
    explicit ref(const com_ptr<OtherInterface> &o) noexcept
    {
        static_assert(std::is_base_of_v<T, OtherInterface>, "OtherInterface must derive from T");
        p_ = static_cast<T *>(o.get());
    }

    template<typename OtherInterface>
    explicit ref(com_ptr<OtherInterface> &&o) noexcept
    {
        static_assert(std::is_base_of_v<T, OtherInterface>, "OtherInterface must derive from T");
        p_ = static_cast<T *>(o.get());
    }

    template<typename OtherInterface>
    explicit ref(const ref<OtherInterface> &o) noexcept
    {
        static_assert(std::is_base_of_v<T, OtherInterface>, "OtherInterface must derive from T");
        p_ = static_cast<T *>(o.get());
    }

#if RTCSDK_HAS_CHECKED_REFS
    ~ref()
    {
        if (parent_) {
            parent_->remove_weak(this);
        }
    }

    ref(const ref &o) noexcept : p_{o.p_}, parent_{o.parent_}
    {
        if (parent_) {
            parent_->add_weak(this);
        }
    }

    ref(ref &&o) noexcept : p_{o.p_}, parent_{o.parent_}
    {
        if (parent_) {
            parent_->remove_weak(&o);
            parent_->add_weak(this);
        }
    }
#endif

    explicit ref(T *p) noexcept : p_{p}
    {
    }

    template<typename Interface>
    ref &operator=(const Interface &) = delete;

    T *operator->() const noexcept
    {
        return p_;
    }

    // Comparison operators
    bool operator==(const ref &o) const noexcept
    {
        return p_ == o.p_;
    }

    bool operator==(T *pother) const noexcept
    {
        return p_ == pother;
    }

    friend bool operator==(T *p1, const ref &p2) noexcept
    {
        return p1 == p2.p_;
    }

    bool operator==(const com_ptr<T> &o) const noexcept
    {
        return p_ == o.get();
    }

    // Ordering operator
    bool operator<(const ref &o) const noexcept
    {
        return p_ < o.p_;
    }

    // Pointer accessors
    T *get() const noexcept
    {
        return p_;
    }

    // Conversion operations
    template<typename Interface>
    auto as() const noexcept
    {
        return com_ptr<Interface>{p_};
    }
};

template<typename Interface>
template<typename OtherInterface>
inline com_ptr<Interface>::com_ptr(ref<OtherInterface> o) noexcept : com_ptr{o.get()}
{
}

// ── type list ─────────────────────────────────────────────────────────────

template<typename... Ts>
struct vector {
    using type = vector;
};

struct ModuleCount {
    static std::atomic<int> lock_count;
};

inline __declspec(selectany) std::atomic<int> ModuleCount::lock_count{};

#pragma region object

struct supports_aggregation_t {
};

struct singleton_factory_t {
};
struct smart_singleton_factory_t {
};

struct increments_module_count_t {
};

struct enable_leak_detection_t {
};

struct delayed_t {
};

constexpr delayed_t delayed = {};

template<typename Interface>
struct __declspec(empty_bases) embeds_interface_id {
};

template<typename T>
concept has_get_first = requires {
    T::get_first();
};

template<typename T>
concept has_implements = requires {
    typename T::can_query;
};

template<typename T>
concept has_increments_module_count = requires {
    typename T::increments_module_count_t;
    requires std::same_as<typename T::increments_module_count_t, increments_module_count_t>;
};

template<typename T>
concept has_supports_aggregation = requires {
    typename T::supports_aggregation_t;
    requires std::same_as<typename T::supports_aggregation_t, supports_aggregation_t>;
};

template<typename T>
concept has_enable_leak_detector = requires {
    typename T::enable_leak_detection_t;
    requires std::same_as<typename T::enable_leak_detection_t, enable_leak_detection_t>;
};

template<typename T>
concept has_singleton_factory = requires {
    typename T::singleton_factory_t;
    requires std::same_as<typename T::singleton_factory_t, singleton_factory_t>;
};

template<typename T>
concept has_smart_singleton_factory = requires {
    typename T::smart_singleton_factory_t;
    requires std::same_as<typename T::smart_singleton_factory_t, smart_singleton_factory_t>;
};

template<typename Interface, typename T>
inline void *query_single([[maybe_unused]] T *pobj, [[maybe_unused]] const GUID &iid) noexcept
{
    if constexpr (has_implements<Interface>) {
        return Interface::query_self(pobj, iid);
    } else if constexpr (std::is_same_v<Interface, IUnknown>) {// NOLINT(bugprone-branch-clone)
        return nullptr;
    } else if (iid == get_interface_guid(interface_wrapper<Interface>{})) {
        auto *ret = static_cast<Interface *>(pobj);
        ret->AddRef();
        return ret;
    }
    return nullptr;
}

template<typename T, typename... Interfaces>
inline void *query(T *pobj, const GUID &iid, details::vector<Interfaces...>) noexcept
{
    void *result{nullptr};
    (... || (nullptr != (result = query_single<Interfaces>(pobj, iid))));
    return result;
}

template<typename... Interfaces>
struct __declspec(empty_bases) extends_base : public Interfaces... {
    template<typename Derived>
    static void *query_children(Derived *pobject, const GUID &riid) noexcept
    {
        return query(pobject, riid, details::vector<Interfaces...>{});
    }
};

// extends marks derived as pure interface
template<typename ThisInterface, typename... Interfaces>
struct __declspec(empty_bases) extends : public extends_base<Interfaces...> {
    struct can_query {
        using type = details::vector<Interfaces...>;
    };

    template<typename Derived>
    static void *query_self(Derived *pobject, const GUID &iid) noexcept
    {
        if (get_interface_guid(interface_wrapper<ThisInterface>{}) == iid) {
            auto *ret = static_cast<ThisInterface *>(pobject);
            ret->AddRef();
            return ret;
        }
        return extends_base<Interfaces...>::query_children(pobject, iid);
    }
};

template<typename T>
inline constexpr auto get_first([[maybe_unused]] interface_wrapper<T> v) noexcept
{
    if constexpr (has_get_first<T>)
        return T::get_first();
    else
        return v;
}

template<typename ThisClass, typename FirstInterface, typename... RestInterfaces>
struct __declspec(empty_bases) intermediate : public extends_base<FirstInterface, RestInterfaces...> {
    struct can_query {
        using type = details::vector<FirstInterface, RestInterfaces...>;
    };

    template<typename Derived>
    static void *query_self(Derived *pobject, const GUID &iid) noexcept
    {
        return extends_base<FirstInterface, RestInterfaces...>::query_children(pobject, iid);
    }

    static constexpr auto get_first() noexcept
    {
        return details::get_first(interface_wrapper<FirstInterface>{});
    }
};

template<typename ThisClass>
struct __declspec(empty_bases) eats_all : public extends_base<> {
    struct can_query {
        using type = details::vector<>;
    };

    template<typename Derived>
    static void *query_self(Derived *pobject, const GUID &iid) noexcept
    {
        return static_cast<ThisClass *>(pobject)->on_eat_all(iid);
    }
};

template<typename ThisClass, typename... Interfaces>
struct __declspec(empty_bases) aggregates {
    struct can_query {
        using type = details::vector<Interfaces...>;
    };

    template<typename Derived>
    static void *query_self(Derived *pobject, const GUID &iid) noexcept
    {
        void *result{nullptr};
        (... || (iid == get_interface_guid(interface_wrapper<Interfaces>{}) ? (result = static_cast<ThisClass *>(pobject)->on_query(interface_wrapper<Interfaces>{})), true : false));
        return result;
    }
};

template<typename... Interfaces>
struct __declspec(empty_bases) also// no inheriting from interfaces!
{
    struct can_query {
        using type = details::vector<Interfaces...>;
    };

    template<typename Derived>
    static void *query_self(Derived *pobject, const GUID &iid) noexcept
    {
        return query(pobject, iid, details::vector<Interfaces...>{});
    }
};

template<typename T, typename... Args>
concept has_legacy_final_construct = requires(T obj, Args &&...args) {
    {
        obj.FinalConstruct(std::forward<Args>(args)...)
    } -> std::same_as<HRESULT>;
};

template<typename T, typename... Args>
concept has_final_construct = requires(T obj, Args &&...args) {
    {
        obj.final_construct(std::forward<Args>(args)...)
    } -> std::same_as<HRESULT>;
};

template<typename T>
concept has_legacy_final_release = requires(T val) {
    val.FinalRelease();
};

template<typename T, typename Holder = T>
concept has_final_release = requires(std::unique_ptr<Holder> instance) {
    T::final_release(std::move(instance));
};

template<typename T>
concept has_on_release = requires(const T &obj, int val) { obj.on_release(val); };

template<typename T>
concept has_on_add_ref = requires(const T &obj, int val) { obj.on_add_ref(val); };

template<typename Derived>
class __declspec(empty_bases) contained_value final : public Derived
{
public:
    template<typename... Args>
    contained_value(IUnknown *pOuterUnk, Args &&...args) : Derived{std::forward<Args>(args)...}, pOuterUnk_{pOuterUnk}
    {
    }

    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
        return pOuterUnk_->AddRef();
    }

    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        return pOuterUnk_->Release();
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) noexcept override
    {
        return pOuterUnk_->QueryInterface(riid, ppvObject);
    }

    HRESULT RealQueryInterface(REFIID riid, void **ppvObject) noexcept
    {
        return Derived::QueryInterface(riid, ppvObject);
    }

private:
    IUnknown *const pOuterUnk_;
};

struct ref_count_base {
    std::atomic<int> rc_refcount_{};

    void safe_increment() noexcept
    {
        rc_refcount_.fetch_add(10, std::memory_order_relaxed);
    }

    void safe_decrement() noexcept
    {
        rc_refcount_.fetch_sub(10, std::memory_order_relaxed);
    }
};

#if defined(_DEBUG)
using no_count_base = ref_count_base;
#else
struct no_count_base {
    static void safe_increment() noexcept
    {
    }

    static void safe_decrement() noexcept
    {
    }
};
#endif

template<bool Enabled>
struct usage_map_base {
    static void add_cookie() noexcept
    {
    }

    static void remove_cookie() noexcept
    {
    }
};

#if RTCSDK_HAS_LEAK_DETECTION
template<>
struct usage_map_base<true> {
    std::vector<leak_detection *> umb_usages;
    srwlock umb_lock;

    void add_cookie() noexcept
    {
        try {
            auto cookie = new leak_detection;

            {
                std::scoped_lock l{umb_lock};
                umb_usages.emplace_back(cookie);
            }

            set_current_cookie(cookie->ordinal);
        } catch (...) {
        }
    }

    void remove_cookie() noexcept
    {
        try {
            auto cookie = get_current_cookie();
            if (cookie) {
                std::scoped_lock l{umb_lock};
                auto it = std::ranges::find(umb_usages, cookie, [](const auto *p) { return p->ordinal; });

                if (it != umb_usages.end()) {
                    delete *it;
                    umb_usages.erase(it);
                } else {
                    assert(false && "Cookie is not found in a map");
                }
            }
        } catch (...) {
        }
    }
};
#endif

template<typename Derived, class Base>
struct __declspec(empty_bases) final_construct_support : Base, usage_map_base<has_enable_leak_detector<Derived>> {
    template<typename... Args>
    void do_final_construct([[maybe_unused]] Derived &obj, [[maybe_unused]] Args &&...args)
    {
        static_assert(!has_legacy_final_construct<Derived, Args...>,
                      "Legacy FinalConstruct not supported. Replace with new final_construct (syntax does not change).");

        if constexpr (has_final_construct<Derived, Args...>) {
            Base::safe_increment();
            auto hr = obj.final_construct(std::forward<Args>(args)...);
            if (FAILED(hr)) {
                throw_bad_hresult(hr);
            }
            Base::safe_decrement();
        }

        if constexpr (has_increments_module_count<Derived>) {
            ModuleCount::lock_count.fetch_add(1, std::memory_order_relaxed);
        }
    }

    template<typename Holder>
    static void do_final_release(std::unique_ptr<Holder> obj, [[maybe_unused]] std::atomic<int> &refcount) noexcept
    {
        static_assert(!has_legacy_final_release<Derived>,
                      "Legacy FinalRelease not supported. Use new style final_release instead");

        if constexpr (has_final_release<Derived, Holder>) {
            // allow for safe QueryInterface for an overloaded final_release function
            refcount.store(1, std::memory_order_relaxed);
            Derived::final_release(std::move(obj));
        } else {
            obj.reset();
        }

        if constexpr (has_increments_module_count<Derived>) {
            ModuleCount::lock_count.fetch_sub(1, std::memory_order_relaxed);
        }
    }

    void debug_on_add_ref([[maybe_unused]] const Derived &obj, [[maybe_unused]] int value) noexcept
    {
        usage_map_base<has_enable_leak_detector<Derived>>::add_cookie();
        if constexpr (has_on_add_ref<Derived>) {
            obj.on_add_ref(value);
        }
    }

    void debug_on_release([[maybe_unused]] const Derived &obj, [[maybe_unused]] int value) noexcept
    {
        usage_map_base<has_enable_leak_detector<Derived>>::remove_cookie();
        if constexpr (has_on_release<Derived>) {
            obj.on_release(value);
        }
    }
};

template<typename Derived>
class __declspec(empty_bases) aggvalue final : public final_construct_support<Derived, ref_count_base>, public IUnknown
{
    using final_construct_support<Derived, ref_count_base>::rc_refcount_;

public:
    template<typename... Args>
    aggvalue(IUnknown *pOuterUnk, Args &&...args) : object_{pOuterUnk, std::forward<Args>(args)...}// NOLINT(google-explicit-constructor)
    {
        static_assert(!has_final_release<Derived> || has_final_release<Derived, aggvalue<Derived>>,
                      "Class overrides final_release, but does not work with aggregate values."
                      "Consider taking templated holder if your object can be aggregated.");

        this->do_final_construct(object_);
    }

    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
        return rc_refcount_.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        auto prev = rc_refcount_.fetch_sub(1, std::memory_order_acq_rel);
        if (prev == 1) {
            this->do_final_release(std::unique_ptr<aggvalue>{this}, rc_refcount_);
        }
        return prev - 1;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) noexcept override
    {
        if (riid == get_interface_guid(interface_wrapper<IUnknown>{})) {
            *ppvObject = static_cast<IUnknown *>(this);
            AddRef();
            return S_OK;
        }
        // "real" query interface
        return object_.RealQueryInterface(riid, ppvObject);
    }

    Derived *get() noexcept
    {
        return &object_;
    }

private:
    contained_value<Derived> object_;
};

template<typename DerivedNonMatchingName>
class __declspec(empty_bases) value
    : public DerivedNonMatchingName,
      public final_construct_support<DerivedNonMatchingName, ref_count_base>
{
public:
    virtual ~value() = default;
    value(const value &o) : DerivedNonMatchingName{static_cast<const DerivedNonMatchingName &>(o)}
    {
        this->do_final_construct(*this);
    }

    template<typename... Args>
    value(Args &&...args) : DerivedNonMatchingName{std::forward<Args>(args)...}// NOLINT(google-explicit-constructor)
    {
        this->do_final_construct(*this);
    }

    template<typename... Args>
    value(delayed_t, Args &&...args)// NOLINT(google-explicit-constructor)
    {
        this->do_final_construct(*this, std::forward<Args>(args)...);
    }

    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
        auto ret = this->rc_refcount_.fetch_add(1, std::memory_order_relaxed) + 1;
        this->debug_on_add_ref(*static_cast<const DerivedNonMatchingName *>(this), ret);
        return ret;
    }

    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        auto prev = this->rc_refcount_.fetch_sub(1, std::memory_order_acq_rel);
        this->debug_on_release(*static_cast<const DerivedNonMatchingName *>(this), prev);

        if (prev == 1) {
            this->do_final_release(std::unique_ptr<DerivedNonMatchingName>{this}, this->rc_refcount_);
        }

        return prev - 1;
    }
};

template<typename DerivedNonMatchingName>
class __declspec(empty_bases) smart_singleton_value final : public value<DerivedNonMatchingName>
{
public:
    std::weak_ptr<DerivedNonMatchingName> get_weak() const noexcept
    {
        return self_;
    }

private:
    std::shared_ptr<DerivedNonMatchingName> self_{static_cast<DerivedNonMatchingName *>(this), [](auto *) {}};
};

template<typename Derived>
class __declspec(empty_bases) value_on_stack : public Derived, public final_construct_support<Derived, no_count_base>
{
public:
    value_on_stack(const value_on_stack &) = delete;
    value_on_stack &operator=(const value_on_stack &) = delete;

    template<typename... Args>
    value_on_stack(Args &&...args) : Derived{std::forward<Args>(args)...}// NOLINT(google-explicit-constructor)
    {
        static_assert(!has_final_release<Derived>, "Classes with final_release cannot be used on stack");
        this->do_final_construct(*this);
    }

    template<typename... Args>
    value_on_stack(delayed_t, Args &&...args)// NOLINT(google-explicit-constructor)
    {
        static_assert(!has_final_release<Derived>, "Classes with final_release cannot be used on stack");
        this->do_final_construct(*this, std::forward<Args>(args)...);
    }

#if defined(_DEBUG)
    ~value_on_stack()
    {
        assert(0 == this->rc_refcount_.load(std::memory_order_relaxed)
               && "value_on_stack is destroyed with active reference!");
    }
#endif

    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
#if defined(_DEBUG)
        this->rc_refcount_.fetch_add(1, std::memory_order_relaxed);
#endif
        return 2;
    }

    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
#if defined(_DEBUG)
        this->rc_refcount_.fetch_sub(1, std::memory_order_relaxed);
#endif
        return 2;
    }
};

// Trait wrappers for check_trait machinery (concepts can't be template template params)
template<typename T>
struct trait_supports_aggregation : std::bool_constant<has_supports_aggregation<T>> {
};

template<typename T>
struct trait_enable_leak_detector : std::bool_constant<has_enable_leak_detector<T>> {
};

template<typename T>
struct trait_singleton_factory : std::bool_constant<has_singleton_factory<T>> {
};

template<typename T>
struct trait_smart_singleton_factory : std::bool_constant<has_smart_singleton_factory<T>> {
};

template<template<typename> typename Trait, typename First, typename... Rest>
constexpr bool check_trait_deep() noexcept;

template<template<typename> typename Trait, typename... Interfaces>
constexpr bool check_trait_vector(details::vector<Interfaces...>) noexcept
{
    return check_trait_deep<Trait, Interfaces...>();
}

template<template<typename> typename Trait, typename Class>
constexpr bool check_trait_single() noexcept
{
    if constexpr (Trait<Class>::value) {
        return true;
    } else if constexpr (has_implements<Class>) {
        return check_trait_vector<Trait>(typename Class::can_query::type{});
    } else {
        return false;
    }
}

template<template<typename> typename Trait, typename First, typename... Rest>
constexpr bool check_trait_deep() noexcept
{
    if constexpr (check_trait_single<Trait, First>())
        return true;
    else if constexpr (sizeof...(Rest) != 0)
        return check_trait_deep<Trait, Rest...>();
    else
        return false;
}

template<typename T>
class object_holder
{
private:
    std::unique_ptr<T> value_;

public:
    object_holder(std::unique_ptr<T> &&value) noexcept : value_{std::move(value)}// NOLINT(google-explicit-constructor)
    {
    }

    template<typename Interface>
        requires(std::is_convertible_v<T *, Interface *> || std::is_same_v<IUnknown, Interface>)
    com_ptr<Interface> to_ptr() && noexcept
    {
        if constexpr (std::is_same_v<IUnknown, Interface>) {
            return com_ptr<IUnknown>(value_.release()->GetUnknown());
        } else {
            return com_ptr<Interface>(static_cast<Interface *>(value_.release()));
        }
    }

    auto to_ptr() && noexcept
    {
        return std::move(*this).template to_ptr<typename T::DefaultInterface>();
    }

    T *obj() const noexcept
    {
        return value_.get();
    }

    T *release() noexcept
    {
        return value_.release();
    }
};

template<typename Derived, typename FirstInterface, typename... OtherInterfaces>
class __declspec(empty_bases) object : public extends_base<FirstInterface, OtherInterfaces...>
{
private:
    using fint_t = decltype(details::get_first(interface_wrapper<FirstInterface>{}));
    using FirstRealInterface = fint_t::type;
    static_assert(!std::is_same<FirstRealInterface, IUnknown>{}, "Do not directly derive from IUnknown");

    template<template<typename> class Trait>
    static constexpr bool check_trait() noexcept
    {
        return Trait<Derived>::value || check_trait_deep<Trait, FirstInterface, OtherInterfaces...>();
    }

    // Derived may override the following functions
    static HRESULT pre_query_interface(REFIID, void **) noexcept
    {
        return E_NOINTERFACE;
    }

    static HRESULT post_query_interface(REFIID, void **ppres) noexcept
    {
        *ppres = nullptr;
        return E_NOINTERFACE;
    }

public:
    virtual ~object() = default;
    using DefaultInterface = FirstRealInterface;

    // Not const — CRTP requires static_cast<Derived*>(this) which needs non-const this.
    IUnknown *GetUnknown() noexcept
    {
        return static_cast<FirstRealInterface *>(static_cast<Derived *>(this));
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) noexcept override
    {
        auto *pobject = static_cast<Derived *>(this);
        auto hr = pobject->pre_query_interface(riid, ppvObject);
        if (SUCCEEDED(hr) || hr != E_NOINTERFACE) {
            return hr;
        }

        if (riid == get_interface_guid(interface_wrapper<IUnknown>{})) {
            auto *pUnk = pobject->GetUnknown();
            *ppvObject = pUnk;
            pUnk->AddRef();
            return S_OK;
        }

        auto result = this->query_children(pobject, riid);
        if (result) {
            // AddRef has already been called
            *ppvObject = result;
            return S_OK;
        }
        return pobject->post_query_interface(riid, ppvObject);
    }

    // Instance creation
    template<typename... Args>
    static object_holder<value<Derived>> create_instance(Args &&...args)
    {
        static_assert(!check_trait<trait_smart_singleton_factory>(),
                      "Objects marked as single_cached_instance (AKA smart_singleton_factory)"
                      "cannot be currently created using create_instance method");
        return {std::make_unique<value<Derived>>(std::forward<Args>(args)...)};
    }

    template<typename... Args>
    static com_ptr<IUnknown> create_aggregate(IUnknown *pOuterUnknown, Args &&...args)
    {
        static_assert(check_trait<trait_supports_aggregation>(),
                      "Class is missing supports_aggregation type trait to support aggregation");
        auto pobject = std::make_unique<aggvalue<Derived>>(pOuterUnknown, std::forward<Args>(args)...);
        return com_ptr<IUnknown>(static_cast<IUnknown *>(pobject.release()));
    }

    template<typename Interface = FirstInterface>
        requires(std::is_convertible_v<Derived *, Interface *> || std::is_same_v<IUnknown, Interface>)
    com_ptr<Interface> create_copy() const
    {
        auto pobject = std::make_unique<value<Derived>>(*static_cast<const value<Derived> *>(this));
        if constexpr (std::is_same_v<IUnknown, Interface>) {
            return com_ptr<IUnknown>(pobject.release()->GetUnknown());
        } else {
            return com_ptr<Interface>(static_cast<Interface *>(pobject.release()));
        }
    }

    static HRESULT factory_create_object(const GUID &iid, void **ppv, IUnknown *pOuterUnknown = nullptr) noexcept;

protected:
    // for derived class to call
    auto addref() noexcept
    {
        return static_cast<value<Derived> *>(this)->AddRef();
    }

    auto release() noexcept
    {
        return static_cast<value<Derived> *>(this)->Release();
    }
};

template<typename Derived, typename FirstInterface, typename... OtherInterfaces>
HRESULT object<Derived, FirstInterface, OtherInterfaces...>::factory_create_object(
    const GUID &iid, void **ppv, IUnknown *pOuterUnk) noexcept
{
    // QI a unique_ptr-held object; on success, transfer ownership to caller via AddRef interface.
    constexpr auto qi_and_release = [](auto instance, const GUID &iid, void **ppv) noexcept {
        const auto hr = instance->QueryInterface(iid, ppv);
        if (SUCCEEDED(hr)) instance.release();
        return hr;
    };

    try {
        if (pOuterUnk) {
            static_assert(check_trait<trait_supports_aggregation>(), "Missing supports_aggregation trait");
            if constexpr (check_trait<trait_supports_aggregation>()) {
                assert(iid == get_interface_guid(interface_wrapper<IUnknown>{}) && "Aggregated QI must be for IUnknown");
                return qi_and_release(std::make_unique<aggvalue<Derived>>(pOuterUnk), iid, ppv);
            } else {
                std::unreachable();
            }
        }
        if constexpr (check_trait<trait_singleton_factory>()) {
            static_assert(!has_final_release<Derived>, "Singleton incompatible with final_release");
            static value_on_stack<Derived> singleton;
            return singleton.QueryInterface(iid, ppv);
        } else if constexpr (check_trait<trait_smart_singleton_factory>()) {
            static srwlock lock;
            static std::weak_ptr<Derived> cached;
            std::scoped_lock l{lock};
            if (auto alive = cached.lock()) return alive->QueryInterface(iid, ppv);
            auto inst = std::make_unique<smart_singleton_value<Derived>>();
            const auto hr = inst->QueryInterface(iid, ppv);
            if (SUCCEEDED(hr)) cached = inst.release()->get_weak();
            return hr;
        } else {
            return qi_and_release(std::make_unique<value<Derived>>(), iid, ppv);
        }
    } catch (const std::bad_alloc &) {
        return E_OUTOFMEMORY;
    } catch (const bad_hresult &e) {
        return e.hr();
    } catch (...) {
        return E_FAIL;
    }
}

#pragma endregion

#pragma region Auto factory support
using create_function_t = HRESULT (*)(const GUID &iid, void **ppv, IUnknown *) noexcept;
struct ObjMapEntry {
    GUID clsid;
    create_function_t create;
};

#pragma section("BIS$__a", read)
#pragma section("BIS$__z", read)
#pragma section("BIS$__b", read)
extern "C" {
__declspec(selectany) __declspec(allocate("BIS$__a")) ObjMapEntry *pobjObjEntryFirst = nullptr;// NOLINT (cppcoreguidelines-avoid-non-const-global-variables)
__declspec(selectany) __declspec(allocate("BIS$__z")) ObjMapEntry *pobjObjEntryLast = nullptr; // NOLINT (cppcoreguidelines-avoid-non-const-global-variables)
}

inline HRESULT create_object(const GUID &clsid, const GUID &iid, void **ppv, IUnknown *pOuterUnk = nullptr) noexcept
{
    for (auto p = &pobjObjEntryFirst + 1; p < &pobjObjEntryLast; ++p) {
        if (*p && (*p)->clsid == clsid) {
            return (*p)->create(iid, ppv, pOuterUnk);
        }
    }
    return REGDB_E_CLASSNOTREG;
}

template<typename Interface>
HRESULT create_object(const GUID &clsid, com_ptr<Interface> &result, IUnknown *pOuterUnk = nullptr) noexcept
{
    return create_object(clsid,
                         get_interface_guid(interface_wrapper<Interface>{}),
                         reinterpret_cast<void **>(result.put()),
                         pOuterUnk);
}

template<typename Interface>
com_ptr<Interface> create_object(const GUID &clsid, IUnknown *pOuterUnk = nullptr)
{
    com_ptr<Interface> result;
    throw_on_failed(create_object(clsid,
                                  get_interface_guid(details::interface_wrapper<Interface>{}),
                                  reinterpret_cast<void **>(result.put()), pOuterUnk));
    return result;
}
#pragma endregion

}// namespace details

// ── public API ────────────────────────────────────────────────────────────

template<size_t N>
constexpr GUID make_guid(const char (&str)[N])
{
    using namespace details;

    static_assert(N == (braced_guid_size + 1) || N == (normal_guid_size + 1),
                  "String GUID of form {00000000-0000-0000-0000-000000000000} "
                  "or 00000000-0000-0000-0000-000000000000 expected");

    if constexpr (N == (braced_guid_size + 1)) {
        if (str[0] != '{' || str[braced_guid_size - 1] != '}') {
            throw std::domain_error{"Missing opening or closing brace"};
        }
    }
    // Offset str by 1 to skip the brace.
    return make_guid_helper(str + (N == (braced_guid_size + 1) ? 1 : 0));
}

constexpr GUID operator""_guid(const char *str, const size_t N)
{
    using namespace details;

    if (!(N == normal_guid_size || N == braced_guid_size)) {
        throw std::domain_error{"String GUID of form {00000000-0000-0000-0000-000000000000} "
                                "or 00000000-0000-0000-0000-000000000000 expected"};
    }

    if (N == braced_guid_size && (str[0] != '{' || str[braced_guid_size - 1] != '}')) {
        throw std::domain_error{"Missing opening or closing brace"};
    }
    return make_guid_helper(str + (N == braced_guid_size ? 1 : 0));
}

template<typename Interface>
constexpr auto get_interface_guid() noexcept
{
    return details::get_interface_guid(details::interface_wrapper<Interface>{});
}

// ── trait structs (public, reference details:: tag types) ───────────────────

struct __declspec(empty_bases) singleton_factory {
    using singleton_factory_t = details::singleton_factory_t;
};

struct __declspec(empty_bases) single_cached_instance {
    using smart_singleton_factory_t = details::smart_singleton_factory_t;
};

struct __declspec(empty_bases) supports_aggregation {
    using supports_aggregation_t = details::supports_aggregation_t;
};

struct __declspec(empty_bases) increments_module_count {
    using increments_module_count_t = details::increments_module_count_t;
};

struct __declspec(empty_bases) enable_leak_detection {
    using enable_leak_detection_t = details::enable_leak_detection_t;
};

// ── public API exported from details ────────────────────────────────────────

using details::aggregates;
using details::also;
using details::attach;
using details::bad_hresult;
using details::com_ptr;
using details::create_object;
using details::delayed;
using details::eats_all;
using details::extends;
using details::init_leak_detection;
using details::interface_wrapper;
using details::intermediate;
using details::object;
using details::ref;
using details::srwlock;
using details::throw_bad_hresult;
using details::throw_last_error;
using details::throw_on_failed;
using details::throw_win32_error;
using details::value_on_stack;

}// namespace rtcsdk

#define RTCSDK_CLASS_GUID(id)                        \
    static constexpr auto get_guid() noexcept        \
    {                                                \
        constexpr auto guid = rtcsdk::make_guid(id); \
        return guid;                                 \
    }
#define RTCSDK_CLASS_GUID_EXISTING(id)        \
    static constexpr auto get_guid() noexcept \
    {                                         \
        return id;                            \
    }

#define RTCSDK_DEFINE_CLASS(name, id) constexpr auto name = rtcsdk::make_guid(id)

#define RTCSDK_GUID_HELPER_(name, id)                                       \
    struct name;                                                            \
    constexpr auto msvc_get_guid_workaround_##name = rtcsdk::make_guid(id); \
    inline constexpr auto get_guid(name *) noexcept                         \
    {                                                                       \
        return msvc_get_guid_workaround_##name;                             \
    }                                                                       \
// end of macro

// The following macro keeps __declspec(uuid()) for backward compatibility
#define COM_INTERFACE(name, id)   \
    RTCSDK_GUID_HELPER_(name, id) \
    struct __declspec(novtable) __declspec(empty_bases) __declspec(uuid(id)) name : rtcsdk::extends<name, IUnknown>// end of macro

#define COM_INTERFACE_BASE(name, base, id) \
    RTCSDK_GUID_HELPER_(name, id)          \
    struct __declspec(novtable) __declspec(empty_bases) __declspec(uuid(id)) name : rtcsdk::extends<name, base>// end of macro

// Short aliases (preferred)
#define DEFINE_CLASS(name, id) RTCSDK_DEFINE_CLASS(name, id)
#define CLASS_GUID(id) RTCSDK_CLASS_GUID(id)
#define CLASS_GUID_EXISTING(id) RTCSDK_CLASS_GUID_EXISTING(id)
#define OBJ_ENTRY_AUTO(class) RTCSDK_OBJ_ENTRY_AUTO(class)
#define OBJ_ENTRY_AUTO2(clsid, class) RTCSDK_OBJ_ENTRY_AUTO2(clsid, class)
#define OBJ_ENTRY_AUTO2_NAMED(clsid, class, name) RTCSDK_OBJ_ENTRY_AUTO2_NAMED(clsid, class, name)

// Legacy aliases
#define RTCSDK_DEFINE_INTERFACE(name, id) COM_INTERFACE(name, id)
#define RTCSDK_DEFINE_INTERFACE_BASE(name, base, id) COM_INTERFACE_BASE(name, base, id)

#if !defined(_M_IA64)
#pragma comment(linker, "/merge:BIS=.rdata")
#endif

#ifndef RTCSDK_OBJ_ENTRY_PRAGMA

#if defined(_M_IX86)
#define RTCSDK_OBJ_ENTRY_PRAGMA(class) __pragma(comment(linker, "/include:___p2objMap_" #class));
#elif defined(_M_IA64) || defined(_M_AMD64) || defined(_M_ARM)
#define RTCSDK_OBJ_ENTRY_PRAGMA(class) __pragma(comment(linker, "/include:__p2objMap_" #class));
#else
#error Unknown Platform. define RTCSDK_OBJ_ENTRY_PRAGMA
#endif

#endif

#define RTCSDK_OBJ_ENTRY_AUTO(class)                                                                                                                    \
    const rtcsdk::details::ObjMapEntry __objxMap_##class = {rtcsdk::get_interface_guid<class>(), &class ::factory_create_object};                       \
    extern "C" __declspec(allocate("BIS$__b")) __declspec(selectany) const rtcsdk::details::ObjMapEntry *const __p2objMap_##class = &__objxMap_##class; \
    RTCSDK_OBJ_ENTRY_PRAGMA(class)                                                                                                                      \
    // end of macro

#define RTCSDK_OBJ_ENTRY_AUTO2(clsid, class)                                                                                                            \
    const rtcsdk::details::ObjMapEntry __objxMap_##class = {clsid, &class ::factory_create_object};                                                     \
    extern "C" __declspec(allocate("BIS$__b")) __declspec(selectany) const rtcsdk::details::ObjMapEntry *const __p2objMap_##class = &__objxMap_##class; \
    RTCSDK_OBJ_ENTRY_PRAGMA(class)                                                                                                                      \
    // end of macro

#define RTCSDK_OBJ_ENTRY_AUTO2_NAMED(clsid, class, name)                                                                                                            \
    const rtcsdk::details::ObjMapEntry __objxMap_##class##name = {clsid, &class ::factory_create_object};                                                           \
    extern "C" __declspec(allocate("BIS$__b")) __declspec(selectany) const rtcsdk::details::ObjMapEntry *const __p2objMap_##class##name = &__objxMap_##class##name; \
    RTCSDK_OBJ_ENTRY_PRAGMA(class##name)                                                                                                                            \
    // end of macro

// ATL interoperability to support RegisterServer/UnregisterServer

#if defined(__ATLBASE_H__)
inline void WINAPI StubObjectMain(bool)
{
}

inline const struct ATL::_ATL_CATMAP_ENTRY *StubGetCats()
{
    return nullptr;
}

inline __declspec(selectany) ATL::_ATL_OBJMAP_CACHE stub_cache{};

#define RTCSDK_OBJ_ENTRY_AUTO_ATL_COMPAT(clsid, class)                                                                                                        \
    __declspec(selectany) ATL::_ATL_OBJMAP_CACHE __objCache__##class = {nullptr, 0};                                                                          \
    const ATL::_ATLObjMapEntry_EX __objMap_##class = {&clsid, class ::UpdateRegistry, nullptr, nullptr, &stub_cache, nullptr, &StubGetCats, &StubObjectMain}; \
    extern "C" __declspec(allocate("ATL$__m")) __declspec(selectany) const ATL::_ATLObjMapEntry_EX *const __pobjMap_##class = &__objMap_##class;              \
    OBJECT_ENTRY_PRAGMA(class)

#define OBJ_ENTRY_AUTO_ATL_COMPAT(clsid, class) RTCSDK_OBJ_ENTRY_AUTO_ATL_COMPAT(clsid, class)

#endif
