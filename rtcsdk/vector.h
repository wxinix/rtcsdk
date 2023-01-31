#pragma warning( disable : 4068 )
#pragma once

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedTemplateParameterInspection"
#pragma ide diagnostic ignored "OCUnusedTypeAliasInspection"
#pragma ide diagnostic ignored "OCUnusedStructInspection"

#include <tuple>

namespace rtcsdk::details {

template<typename... Ts>
struct vector
{
  using type = vector;
};

template<typename T>
struct size;

template<typename... Ts>
struct size<vector<Ts...>>
{
  static const size_t value = sizeof...(Ts);
};

template<typename Dest, typename T>
struct push_back;

template<typename... Ts, typename T>
struct push_back<vector<Ts...>, T>
{
  using type = vector<Ts..., T>;
};

template<typename Dest, typename T>
using push_back_t = typename push_back<Dest, T>::type;

template<typename Dest, typename T>
struct push_front;

template<typename... Ts, typename T>
struct push_front<vector<Ts...>, T>
{
  using type = vector<T, Ts...>;
};

template<typename Dest, typename T>
using push_front_t = typename push_front<Dest, T>::type;

template<typename A, typename B>
struct append;

template<typename... Ts, typename T>
struct append<vector<Ts...>, T>
{
  using type = vector<Ts..., T>;
};

template<typename T, typename... Ts>
struct append<T, vector<Ts...>>
{
  using type = vector<T, Ts...>;
};

template<typename... Ls, typename... Rs>
struct append<vector<Ls...>, vector<Rs...>>
{
  using type = vector<Ls..., Rs...>;
};

template<typename A, typename B>
using append_t = typename append<A, B>::type;

template<typename T>
struct front;

template<typename First, typename... Rest>
struct front<vector<First, Rest...>>
{
  using type = First;
};

template<typename T>
using front_t = typename front<T>::type;

template<typename T>
struct back;

template<typename... Rest, typename Last>
struct back<vector<Rest..., Last>>
{
  using type = Last;
};

template<typename T>
using back_t = typename back<T>::type;

template<typename T>
struct remove_front;

template<typename First, typename... Rest>
struct remove_front<vector<First, Rest...>>
{
  using type = vector<Rest...>;
};

template<typename T>
using remove_front_t = typename remove_front<T>::type;

template<typename T>
struct remove_back;

template<typename... Rest, typename Last>
struct remove_back<vector<Rest..., Last>>
{
  using type = vector<Rest...>;
};

template<typename T>
using remove_back_t = typename remove_back<T>::type;

template<typename T>
struct as_tuple;

template<typename... Ts>
struct as_tuple<vector<Ts...>>
{
  using type = std::tuple<Ts...>;
};

template<typename T>
using as_tuple_t = typename as_tuple<T>::type;

}// end of namespace rtcsdk::details

#pragma clang diagnostic pop
#pragma warning( default : 4068 )