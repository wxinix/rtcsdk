#pragma once

#include <cstdint>
#include <stdexcept>

namespace rtcsdk {

namespace details {

constexpr size_t normal_guid_size = 36;//  00000000-0000-0000-0000-000000000000
constexpr size_t braced_guid_size = 38;// {00000000-0000-0000-0000-000000000000}

/**
 Parse hexadecimal digit char to integer value.

 @exception std::domain_error   Raised when the input character is not a hex
                                digit char.

 @param     c   The hex char.

 @returns   Integer value of the hex char.
 */
constexpr uint32_t parse_hex_digit(const char c) // NOLINT
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
constexpr T parse_hex(const char *ptr) // NOLINT
{
    constexpr size_t digits = sizeof(T) * 2;
    T result{};

    for (size_t i = 0; i < digits; ++i) {
        result |= parse_hex_digit(ptr[i]) << (4 * (digits - i - 1));
    }

    return result;
}

constexpr GUID make_guid_helper(const char *begin) // NOLINT
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

}// namespace details

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

namespace literals {

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
    // Offset str by 1 to skip the brace.
    return make_guid_helper(str + (N == braced_guid_size ? 1 : 0));
}

}// namespace literals

}// end of namespace rtcsdk
