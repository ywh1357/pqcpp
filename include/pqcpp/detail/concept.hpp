#pragma once

#include <type_traits>

namespace pqcpp {
namespace concept {

    template <typename, typename = std::void_t<>>
    struct is_connection: std::false_type {};
    template <typename T>
    struct is_connection<T, std::void_t<
        decltype((void)std::declval<T>().get_strand()),
        decltype((void)std::declval<T>().get_option())
    >> : std::true_type {};

}
}