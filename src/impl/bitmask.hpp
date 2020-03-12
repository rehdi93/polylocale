#include <type_traits>

// BITMASK OPERATIONS
namespace bitmask
{

//template<class T>
//using is_bitmask = std::disjunction<std::is_integral<T>, std::is_enum<T>>;

template<typename T>
using identity = std::common_type<T>;
template<typename T>
using identity_t = typename identity<T>::type;


inline namespace ops
{

    template<typename Enum> [[nodiscard]]
    constexpr Enum operator & (Enum left, Enum right) noexcept {
        using IntType = std::underlying_type_t<Enum>;
        return static_cast<Enum>(static_cast<IntType>(left) & static_cast<IntType>(right));
    }
    template<typename Enum> [[nodiscard]]
    constexpr Enum operator | (Enum left, Enum right) noexcept {
        using IntType = std::underlying_type_t<Enum>;
        return static_cast<Enum>(static_cast<IntType>(left) | static_cast<IntType>(right));
    }
    template<typename Enum> [[nodiscard]]
    constexpr Enum operator ^ (Enum left, Enum right) noexcept {
        using IntType = std::underlying_type_t<Enum>;
        return static_cast<Enum>(static_cast<IntType>(left) ^ static_cast<IntType>(right));
    }
    template<typename Enum> [[nodiscard]]
    constexpr Enum operator ~ (Enum left) noexcept {
        using IntType = std::underlying_type_t<Enum>;
        return static_cast<Enum>(~static_cast<IntType>(left));
    }

    template<typename Enum>
    constexpr Enum operator &= (Enum& left, Enum right) noexcept {
        return left = left & right;
    }
    template<typename Enum>
    constexpr Enum operator |= (Enum& left, Enum right) noexcept {
        return left = left | right;
    }
    template<typename Enum>
    constexpr Enum operator ^= (Enum& left, Enum right) noexcept {
        return left = left ^ right;
    }

}


template<typename Enum> [[nodiscard]]
constexpr bool has(Enum left, identity_t<Enum> elements) noexcept {
    static_assert(std::is_integral<Enum>{} || std::is_enum<Enum>{}, "not a bitmask type");
    return (left & elements) != Enum{};
}

template<typename Enum> [[nodiscard]]
constexpr bool has_all(Enum left, identity_t<Enum> elements) noexcept {
    static_assert(std::is_integral<Enum>{} || std::is_enum<Enum>{}, "not a bitmask type");
    return (left & elements) == elements;
}

template<typename Enum>
constexpr Enum setm(Enum& left, identity_t<Enum> flags, identity_t<Enum> mask) noexcept {
    static_assert(std::is_integral<Enum>{} || std::is_enum<Enum>{}, "not a bitmask type");
    auto old = left;
    left = (left & ~mask) | (flags & mask);
    return old;
}

}

#ifdef BITMASK_MACROS

#define BITMASK_ENUM_T(name, utype, ...) namespace name { enum name ## Enum : utype { __VA_ARGS__ }; }
#define BITMASK_ENUM(name, ...) BITMASK_ENUM_T(name, int, __VA_ARGS__)

#endif // BITMASK_MACROS
