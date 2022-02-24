#pragma once

#include <algorithm>
#include <type_traits>
#include <tuple>

#undef min

#include <boost/preprocessor/repetition/repeat.hpp>


#define VARIADIC_FRIENDS(z, N, Ts) friend std::tuple_element_t<std::min((std::size_t)N+1, sizeof...(Ts)), std::tuple<void, Ts...>>;

template <typename... T>
class badge
{
	BOOST_PP_REPEAT(128, VARIADIC_FRIENDS, T)

private:
	constexpr badge() noexcept = default;
};
