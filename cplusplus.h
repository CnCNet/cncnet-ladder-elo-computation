 
#pragma once

#include <type_traits>

namespace dts
{

template <typename E>
constexpr auto to_underlying(E e) noexcept
{
    return static_cast<std::underlying_type_t<E>>(e);
}

template <typename... Args>
inline void unused(Args&&...)
{
}

}
