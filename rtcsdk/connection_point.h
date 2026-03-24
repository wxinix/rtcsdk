// Copyright (c) 2022-2026 Wuping Xin. All rights reserved.
// MIT License. See LICENSE.md for details.

#pragma once

#include <map>
#include <ocidl.h>
#include <olectl.h>

#include <rtcsdk/rtcsdk.h>

namespace rtcsdk {

namespace details {

// ── Enumerator helpers ──────────────────────────────────────────────────────
// Lightweight ref-counted COM enumerator using CRTP. Manual IUnknown avoids
// routing COM SDK interfaces through rtcsdk::object's type trait machinery.

template<typename Derived, typename EnumInterface, typename ItemType>
class com_enumerator : public EnumInterface
{
    std::atomic<ULONG> ref_{1};

public:
    virtual ~com_enumerator() = default;

protected:
    std::vector<ItemType> items_;
    size_t pos_{};

    // Items that need AddRef on enumeration (IUnknown-derived pointer members)
    static void addref_item(ItemType &item) noexcept
    {
        if constexpr (std::is_pointer_v<ItemType>) {
            item->AddRef();
        } else if constexpr (requires { item.pUnk; }) {
            item.pUnk->AddRef();
        }
    }

public:
    explicit com_enumerator(std::vector<ItemType> items) noexcept
        : items_(std::move(items)) {}

    // IUnknown
    ULONG STDMETHODCALLTYPE AddRef() noexcept override { return ++ref_; }
    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        const auto r = --ref_;
        if (r == 0) delete static_cast<Derived *>(this);
        return r;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) noexcept override
    {
        if (!ppv) return E_POINTER;
        if (riid == __uuidof(EnumInterface) || riid == __uuidof(IUnknown)) {
            *ppv = static_cast<EnumInterface *>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    // IEnum*::Next
    HRESULT STDMETHODCALLTYPE Next(const ULONG celt, ItemType *rgelt, ULONG *pcFetched) noexcept override
    {
        if (!rgelt) return E_POINTER;
        ULONG fetched = 0;
        while (fetched < celt && pos_ < items_.size()) {
            rgelt[fetched] = items_[pos_];
            addref_item(rgelt[fetched]);
            ++fetched;
            ++pos_;
        }
        if (pcFetched) *pcFetched = fetched;
        return fetched == celt ? S_OK : S_FALSE;
    }

    HRESULT STDMETHODCALLTYPE Skip(const ULONG celt) noexcept override
    {
        pos_ = (std::min)(pos_ + static_cast<size_t>(celt), items_.size());
        return pos_ < items_.size() ? S_OK : S_FALSE;
    }

    HRESULT STDMETHODCALLTYPE Reset() noexcept override
    {
        pos_ = 0;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Clone(EnumInterface **ppEnum) noexcept override
    {
        if (!ppEnum) return E_POINTER;
        try {
            auto *clone = new Derived(items_);
            clone->pos_ = pos_;
            *ppEnum = clone;
            return S_OK;
        } catch (const std::bad_alloc &) {
            return E_OUTOFMEMORY;
        }
    }
};

class enum_connections_impl final
    : public com_enumerator<enum_connections_impl, IEnumConnections, CONNECTDATA>
{
    using com_enumerator::com_enumerator;
};

class enum_connection_points_impl final
    : public com_enumerator<enum_connection_points_impl, IEnumConnectionPoints, IConnectionPoint *>
{
    using com_enumerator::com_enumerator;
};

// ── Single connection point ─────────────────────────────────────────────────

template<typename EventInterface>
class connection_point final : public IConnectionPoint
{
public:
    connection_point() = default;
    virtual ~connection_point() = default;

    void init(IUnknown *owner) noexcept
    {
        owner_ = owner;
    }

    // IUnknown — delegate to owner
    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
        return owner_->AddRef();
    }

    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        return owner_->Release();
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) noexcept override
    {
        if (!ppv)
            return E_POINTER;
        if (riid == __uuidof(IConnectionPoint) || riid == __uuidof(IUnknown)) {
            *ppv = static_cast<IConnectionPoint *>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    // IConnectionPoint
    HRESULT STDMETHODCALLTYPE GetConnectionInterface(IID *pIID) noexcept override
    {
        if (!pIID)
            return E_POINTER;
        *pIID = get_interface_guid(interface_wrapper<EventInterface>{});
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetConnectionPointContainer(IConnectionPointContainer **ppCPC) noexcept override
    {
        if (!ppCPC)
            return E_POINTER;
        return owner_->QueryInterface(__uuidof(IConnectionPointContainer),
                                      reinterpret_cast<void **>(ppCPC));
    }

    HRESULT STDMETHODCALLTYPE Advise(IUnknown *pUnkSink, DWORD *pdwCookie) noexcept override
    {
        if (!pUnkSink || !pdwCookie)
            return E_POINTER;
        try {
            com_ptr<EventInterface> sink;
            const auto hr = pUnkSink->QueryInterface(
                get_interface_guid(interface_wrapper<EventInterface>{}),
                reinterpret_cast<void **>(sink.put()));
            if (FAILED(hr))
                return CONNECT_E_CANNOTCONNECT;

            std::scoped_lock l{lock_};
            const auto cookie = next_cookie_++;
            sinks_.emplace(cookie, std::move(sink));
            sink_count_.fetch_add(1, std::memory_order_relaxed);
            *pdwCookie = cookie;
            return S_OK;
        } catch (const std::bad_alloc &) {
            return E_OUTOFMEMORY;
        }
    }

    HRESULT STDMETHODCALLTYPE Unadvise(DWORD dwCookie) noexcept override
    {
        std::scoped_lock l{lock_};
        if (sinks_.erase(dwCookie) == 0)
            return CONNECT_E_NOCONNECTION;
        sink_count_.fetch_sub(1, std::memory_order_relaxed);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE EnumConnections(IEnumConnections **ppEnum) noexcept override
    {
        if (!ppEnum)
            return E_POINTER;

        try {
            std::vector<CONNECTDATA> data;
            {
                std::scoped_lock l{lock_};
                data.reserve(sinks_.size());
                for (const auto &[cookie, sink] : sinks_) {
                    data.push_back({.pUnk = sink.get(), .dwCookie = cookie});
                }
            }
            *ppEnum = new enum_connections_impl(std::move(data));
            return S_OK;
        } catch (const std::bad_alloc &) {
            return E_OUTOFMEMORY;
        }
    }

    // Fire helper — takes a snapshot under lock, fires outside lock to prevent deadlocks
    template<typename F>
    void fire_event(F &&f)
    {
        std::vector<com_ptr<EventInterface>> snapshot;
        {
            std::scoped_lock l{lock_};
            snapshot.reserve(sinks_.size());
            for (const auto &[cookie, sink] : sinks_) {
                snapshot.push_back(sink);
            }
        }
        for (const auto &sink : snapshot) {
            f(sink.get());
        }
    }

    // Relaxed atomic read is well-defined — may return a slightly stale value,
    // which is acceptable for an approximate "should I bother firing?" check.
    [[nodiscard]] bool has_sinks() const noexcept
    {
        return sink_count_.load(std::memory_order_relaxed) > 0;
    }

private:
    // Initialized to nullptr; guaranteed non-null before any public method via
    // connection_points::ensure_init() which uses std::call_once.
    IUnknown *owner_{};
    std::map<DWORD, com_ptr<EventInterface>> sinks_;
    mutable srwlock lock_;
    // Cookie overflow at 2^32 Advise calls is practically unreachable.
    DWORD next_cookie_{1};
    std::atomic<size_t> sink_count_{};
};

// ── Connection point container mixin ────────────────────────────────────────

// Note: clangd may report a false positive on IConnectionPointContainer here
// ("constexpr variable '_Is_pointer_address_convertible' must be initialized
// by a constant expression"). This is a clang limitation with MSVC COM
// interfaces that use __declspec(uuid). Compiles correctly on MSVC.
template<typename Derived, typename... Events>
class connection_points
    : public intermediate<connection_points<Derived, Events...>, IConnectionPointContainer>
{
    std::tuple<connection_point<Events>...> points_;
    std::once_flag init_flag_;

    void ensure_init() noexcept
    {
        std::call_once(init_flag_, [this] {
            auto *owner = static_cast<Derived *>(this)->GetUnknown();
            std::apply([owner](auto &...pts) { (pts.init(owner), ...); }, points_);
        });
    }

    // Not const — calls ensure_init() which mutates init_flag_ and connection point owner pointers.
    IConnectionPoint *find_point(REFIID riid) noexcept
    {
        ensure_init();
        IConnectionPoint *result = nullptr;
        std::apply(
            [&](auto &...pts) {
                (... || (get_interface_guid(interface_wrapper<Events>{}) == riid ? (result = static_cast<IConnectionPoint *>(&pts), true) : false));
            },
            points_);
        return result;
    }

public:
    // IConnectionPointContainer
    HRESULT STDMETHODCALLTYPE FindConnectionPoint(REFIID riid, IConnectionPoint **ppCP) noexcept override
    {
        if (!ppCP)
            return E_POINTER;

        auto *cp = find_point(riid);
        if (!cp) {
            *ppCP = nullptr;
            return CONNECT_E_NOCONNECTION;
        }
        cp->AddRef();
        *ppCP = cp;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE EnumConnectionPoints(IEnumConnectionPoints **ppEnum) noexcept override
    {
        if (!ppEnum)
            return E_POINTER;

        try {
            ensure_init();
            std::vector<IConnectionPoint *> pts;
            pts.reserve(sizeof...(Events));
            std::apply(
                [&](auto &...cp) {
                    (pts.push_back(static_cast<IConnectionPoint *>(&cp)), ...);
                },
                points_);

            *ppEnum = new enum_connection_points_impl(std::move(pts));
            return S_OK;
        } catch (const std::bad_alloc &) {
            return E_OUTOFMEMORY;
        }
    }

protected:
    /// Fire an event to all connected sinks of the given interface.
    /// Sinks are snapshotted under lock; the callable runs outside the lock.
    template<typename Event, typename F>
    void fire(F &&f)
    {
        std::get<connection_point<Event>>(points_).fire_event(std::forward<F>(f));
    }

    /// Check if any sinks are connected for the given event interface.
    template<typename Event>
    [[nodiscard]] bool has_sinks() const noexcept
    {
        return std::get<connection_point<Event>>(points_).has_sinks();
    }
};

}// namespace details

using details::connection_points;

}// namespace rtcsdk
