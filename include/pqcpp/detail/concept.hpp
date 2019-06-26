#pragma once

#include <type_traits>
#include <boost/asio.hpp>

namespace pqcpp {
namespace concept {

    template <typename, typename = std::void_t<>>
    struct is_connection: std::false_type {};
    template <typename T>
    struct is_connection<T, std::void_t<
        decltype((void)std::declval<T>().get_strand()),
        decltype((void)std::declval<T>().get_option())
    >> : std::true_type {};

	template <typename>
	struct awaitable_value;
	template <typename T>
	struct awaitable_value<boost::asio::awaitable<T>> {
		using type = T;
	};
	template <typename T>
	using awaitable_value_t = typename awaitable_value<T>::type;

	template <typename, typename = std::void_t<>>
	struct is_asio_coroutine_function : std::false_type {};
	template <typename F>
	struct is_asio_coroutine_function<F, std::void_t<
		awaitable_value_t<std::invoke_result_t<F>>
	>> : std::true_type {};
	template <typename F>
	inline constexpr auto is_asio_coroutine_function_v = is_asio_coroutine_function<F>::value;

	template <typename F>
	struct coroutine_function_result {
		using type = typename awaitable_value<std::invoke_result_t<F>>::type;
	};
	template <typename F>
	using coroutine_function_result_t = typename coroutine_function_result<F>::type;

	template <typename F>
	inline constexpr bool is_non_void_coroutine_function_v =
		is_asio_coroutine_function_v<F>
		&& !std::is_same_v<coroutine_function_result_t<F>, void>;

	template <typename F>
	inline constexpr bool is_void_coroutine_function_v =
		is_asio_coroutine_function_v<F>
		&& std::is_same_v<coroutine_function_result_t<F>, void>;
}
}