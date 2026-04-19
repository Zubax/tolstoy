// Copyright (c)  Zubax Robotics  <zubax.com>
// Author: Pavel Kirienko <pavel.kirienko@zubax.com>

#pragma once

#include <tuple>
#include <cmath>
#include <array>
#include <ranges>
#include <chrono>
#include <limits>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <variant>
#include <optional>
#include <string_view>

namespace tolstoy {
/// Convert an integer (signed or unsigned) to string of the specified base and store it into an internal buffer.
/// Lowercase letters are used for bases above 10 and below 36.
/// For bases above 36, uppercase letters are also used, which makes the representation case-sensitive.
/// Bases above 62 are not supported; an attempt to use them will result in a compile-time error.
/// The interface of the resulting object is string-like. The instance is movable and copyable.
template <std::integral T, std::uint8_t radix = 10>
class IntAsString
{
    static_assert(radix >= 2);
    // Plus 1 to round up, see the standard for details.
    static constexpr std::uint8_t N =
      ((radix >= 10) ? std::numeric_limits<T>::digits10 : std::numeric_limits<T>::digits) + 1 +
      (std::is_signed_v<T> ? 1 : 0);

    static constexpr std::array<char, 62> alphabet{ {
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', //
      'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
      'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', //
      'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
      'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    } };

  public:
    explicit constexpr IntAsString(const T value) noexcept
    {
        static_assert(radix <= alphabet.size());
        if constexpr ((2 == std::numeric_limits<T>::radix) && (1 == std::numeric_limits<T>::digits)) {
            --offset_;
            storage_.at(offset_) = value ? '1' : '0';
        } else {
            T tmp = value;
            do {
                --offset_;
                storage_.at(offset_) = alphabet.at(
                  static_cast<std::uint8_t>(std::abs(static_cast<std::int8_t>(tmp % static_cast<T>(radix)))));
                tmp = static_cast<T>(tmp / static_cast<T>(radix));
            } while (tmp != 0);
        }
        if constexpr (std::is_signed_v<T>) {
            if (value < 0) {
                --offset_;
                storage_.at(offset_) = '-';
            }
        }
    }

    [[nodiscard]] auto c_str() const noexcept -> const char* { return &storage_.at(offset_); }
    [[nodiscard]] auto data() noexcept -> char* { return &storage_.at(offset_); }
    [[nodiscard]] auto length() const noexcept -> std::size_t { return N - offset_; }
    [[nodiscard]] auto size() const noexcept -> std::size_t { return length(); }

    [[nodiscard]] constexpr auto capacity() const noexcept -> std::size_t { return N; }
    [[nodiscard]] constexpr auto max_size() const noexcept -> std::size_t { return N; }

    // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
    [[nodiscard]] operator std::string_view() const noexcept // NOSONAR implicit by design
    {
        return { c_str(), size() };
    }

  private:
    std::uint8_t            offset_ = N;
    std::array<char, N + 1> storage_{}; // Plus 1 because of zero termination.
};

/// This is like IntAsString<> but for floating-point types.
/// The default format is fixed-length like: "+0.00000e+00"; see options for other options.
/// Non-finite values are represented to maximize JSON[5] compatibility: "NaN", "Infinity", "-Infinity".
template <std::floating_point T>
class FloatAsString
{
  public:
    struct Options final
    {
        /// If set, the sign will be always present, even if the value is positive.
        bool explicit_sign = true;
    };

    FloatAsString(const T value, const Options& opt) noexcept
    {
        if (std::isfinite(value)) {
            do_finite(value, opt.explicit_sign);
        } else {
            do_nonfinite(value, opt.explicit_sign);
        }
    }
    explicit FloatAsString(const T value) noexcept
      : FloatAsString(value, Options{})
    {
    }

    [[nodiscard]] auto c_str() const noexcept -> const char* { return &buf_.at(off_); }
    [[nodiscard]] auto data() noexcept -> char* { return &buf_.at(off_); }
    [[nodiscard]] auto length() const noexcept -> std::size_t { return N - off_; }
    [[nodiscard]] auto size() const noexcept -> std::size_t { return length(); }

    [[nodiscard]] constexpr auto capacity() const noexcept -> std::size_t { return N; }
    [[nodiscard]] constexpr auto max_size() const noexcept -> std::size_t { return N; }

    // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
    [[nodiscard]] operator std::string_view() const noexcept // NOSONAR implicit by design
    {
        return { c_str(), size() };
    }

  private:
    void do_finite(const T value, const bool explicit_sign) noexcept
    {
        auto [ii, ff, ee]  = split_int_frac_exp(value);
        const char ee_sign = (ee < 0) ? '-' : '+';
        for (auto k = 0U; k < exp_digits; k++) {
            ee = next(ee);
        }
        assert(ee == 0);
        if (explicit_sign || (ee_sign != '+')) {
            [[likely]] push(ee_sign);
        }
        push('e');
        for (auto k = 0U; k < frac_digits; k++) {
            ff = next(ff);
        }
        assert(ff == 0);
        push('.');
        do {
            ii = next(ii);
        } while (ii != 0);
        if (explicit_sign || (value < 0)) {
            [[likely]] push((value < 0) ? '-' : '+');
        }
    }

    void do_nonfinite(const T value, const bool explicit_sign) noexcept
    {
        if ((value > 0) || (value < 0)) {
            push('y'); // JSON5 and Python JSON accept "[+-]Infinity". jq accepts that and other forms as well.
            push('t');
            push('i');
            push('n');
            push('i');
            push('f');
            push('n');
            push('I');
            if (explicit_sign || (value < 0)) {
                [[likely]] push((value < 0) ? '-' : '+');
            }
        } else {
            push('N'); // JSON5, Python JSON, jq accept "NaN" but not "nan". Canonical YAML accepts neither.
            push('a');
            push('N');
        }
    }

    template <typename N>
    [[nodiscard]] auto next(const N value) noexcept -> N
    {
        push(static_cast<char>('0' + std::abs(static_cast<std::int8_t>(value % 10)))); // NOSONAR char arithmetics
        return static_cast<N>(value / 10);
    }

    [[nodiscard]] static auto split_int_frac_exp(const T& value) noexcept
      -> std::tuple<std::uint8_t, std::uint64_t, std::int16_t>
    {
        static_assert((log10_ceil(std::numeric_limits<std::uint64_t>::max()) - 1) > std::numeric_limits<T>::digits10,
                      "This floating point type may cause integer overflow during string conversion");
        constexpr auto mul = pow10(std::numeric_limits<T>::digits10 - 1);
        constexpr auto lim = pow10(std::numeric_limits<T>::digits10);
        const T        a   = std::abs(value);
        std::int16_t   e   = (a > std::numeric_limits<T>::denorm_min()) //
                               ? static_cast<std::int16_t>(std::floor(std::log10(a)))
                               : 0;
        auto           x   = static_cast<std::uint64_t>(
          std::llround((a / std::pow(static_cast<T>(10), static_cast<T>(e))) * static_cast<T>(mul)));
        if (x >= lim) // Check if rounding caused an overflow.
        {
            e += 1;
            x /= 10U;
        }
        return { static_cast<std::uint8_t>(x / mul), x % mul, e };
    }

    void push(const char sym) noexcept
    {
        --off_;
        buf_.at(off_) = sym;
    }

    [[nodiscard]] static constexpr auto pow10(const std::uint8_t p) noexcept -> std::uint64_t
    {
        std::uint64_t pw = 1U;
        for (std::uint8_t k = 0U; k < p; k++) {
            pw *= 10UL;
        }
        return pw;
    }

    [[nodiscard]] static constexpr auto log10_ceil(const std::uint64_t x) noexcept -> std::uint8_t
    {
        std::uint8_t out = 0;
        auto         t   = x;
        while (t > 0) {
            out++;
            t /= 10U;
        }
        return out;
    }

    static_assert(std::is_floating_point_v<T>);
    static constexpr std::uint8_t frac_digits = std::numeric_limits<T>::digits10 - 1;
    static constexpr std::uint8_t exp_digits  = log10_ceil(std::numeric_limits<T>::max_exponent10);
    static constexpr std::uint8_t N           = std::max<std::uint8_t>(9, 3 + frac_digits + 2 + exp_digits + 3);

    std::uint8_t            off_ = N;
    std::array<char, N + 1> buf_{}; // Plus 1 because of zero termination.
};
/// This deduction guide is needed only for GCC 11. Eventually, it will be removed.
template <typename T>
FloatAsString(const T&, const typename FloatAsString<T>::Options&) -> FloatAsString<T>;

/// A utility for storing and formatting strings of the specified capacity.
/// Content past the capacity is silently truncated.
/// This is a POD type; instances can be safely moved and copied.
/// The capacity does not include the null terminator; that is, the real storage is one byte larger.
/// This is convertible to/from std::string_view.
/// The most important methods of std::string are implemented here
/// (aside from those related to resizing, of course, as it makes no sense here).
///
/// Note that chars are treated as integers; if you need to add chars, either use push_back() or use strings.
/// Supports not only primitives like strings and numbers but also arrays, optionals, chrono time/duration,
/// enumerations, pairs, variants, and any combinations thereof.
///
/// operator<< is provided for std-like behavior (but there's no endl, use "\n" instead).
/// User code can provide custom formatting by overloading this operator as follows:
///
///     auto& operator<<(auto& str, const UserType& value) { <...formatting...> return str; }
///
/// Or less generically, but requires an explicit dependency on Tolstoy (which is often undesirable in abstract code):
///
///     template <std::size_t N>
///     tolstoy::String<N>& operator<<(tolstoy::String<N>& str, const UserType& value);
///
/// The ADL rules require that the overloaded operator<< is defined in the same namespace as the type being formatted,
/// or in tolstoy (less desirable).
/// Then define the formatting behavior in terms of the primitive types supported by String<>.
template <std::size_t N>
class String
{
  public:
    String() = default;
    // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
    String(const std::string_view str) noexcept { operator=(str); } // NOSONAR implicit by design

    // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
    String& operator=(const std::string_view other) // NOSONAR implicit by design
    {
        clear();
        operator+=(other);
        return *this;
    }

    [[nodiscard]] auto data() noexcept -> char* { return buf_.data(); }
    [[nodiscard]] auto data() const noexcept -> const char* { return buf_.data(); }
    [[nodiscard]] auto c_str() const noexcept -> const char* { return data(); }

    [[nodiscard]] auto length() const noexcept -> std::size_t { return off_; }
    [[nodiscard]] auto size() const noexcept -> std::size_t { return length(); }
    [[nodiscard]] auto empty() const noexcept -> bool { return 0 == size(); }

    /// True if the buffer cannot accept extra data.
    /// This can be used to check for overflow: make the capacity one item greater than needed and ensure this is false.
    [[nodiscard]] auto full() const noexcept -> bool { return off_ >= N; }

    [[nodiscard]] constexpr auto capacity() const noexcept -> std::size_t { return N; }
    [[nodiscard]] constexpr auto max_size() const noexcept -> std::size_t { return N; }

    /// The behavior is undefined if the string is empty.
    [[nodiscard]] auto& front() { return buf_.front(); }
    [[nodiscard]] auto  front() const { return buf_.front(); }
    [[nodiscard]] auto& back() { return buf_.at(off_ - 1); }
    [[nodiscard]] auto  back() const { return buf_.at(off_ - 1); }

    [[nodiscard]] auto begin() noexcept -> char* { return data(); }
    [[nodiscard]] auto begin() const noexcept -> const char* { return data(); }
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    [[nodiscard]] auto end() noexcept -> char* { return data() + size(); }
    [[nodiscard]] auto end() const noexcept -> const char* { return data() + size(); }
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
    [[nodiscard]] operator std::string_view() const noexcept { return { c_str(), size() }; } // NOSONAR implicit

    /// Add one character unless the storage is already full, in which case do nothing.
    void push_back(const char ch) noexcept
    {
        if (off_ < N) {
            buf_.at(off_) = ch;
            off_++;
        }
    }
    String& operator+=(const char ch) noexcept
    {
        push_back(ch);
        return *this;
    }

    /// Remove the last character unless the string is empty, in which case do nothing.
    void pop_back() noexcept
    {
        if (off_ > 0) {
            off_--;
            buf_.at(off_) = '\0';
        }
    }

    /// Add the specified bytes to the string without checking their values.
    /// Items that would overflow the buffer are silently truncated.
    String& operator+=(const std::string_view str) noexcept
    {
        assert(off_ <= N);
        const auto am = std::min(str.size(), N - off_);
        (void)std::memmove(buf_.begin() + off_, str.begin(), am);
        off_ += am;
        assert(off_ <= N);
        return *this;
    }

    void clear()
    {
        off_ = 0;
        buf_.fill(0);
    }

  private:
    std::size_t             off_ = 0;
    std::array<char, N + 1> buf_{};
};

/// Detection trait for String<>.
template <typename>
struct IsString : public std::false_type
{};
template <std::size_t N>
struct IsString<String<N>> : public std::true_type
{};
template <typename T>
constexpr bool is_string = IsString<std::decay_t<T>>::value;

// --------------------------------------------------------------------------------------------------------------------

template <std::size_t A, std::size_t B>
[[nodiscard]] bool
operator==(const String<A>& lhs, const String<B>& rhs) noexcept
{
    return static_cast<std::string_view>(lhs) == static_cast<std::string_view>(rhs);
}
template <std::size_t B>
[[nodiscard]] bool
operator==(const char* const lhs, const String<B>& rhs) noexcept
{
    return static_cast<std::string_view>(lhs) == static_cast<std::string_view>(rhs);
}
template <std::size_t A>
[[nodiscard]] bool
operator==(const String<A>& lhs, const char* const rhs) noexcept
{
    return static_cast<std::string_view>(lhs) == static_cast<std::string_view>(rhs);
}

template <std::size_t A, std::size_t B>
[[nodiscard]] auto
operator<=>(const String<A>& lhs, const String<B>& rhs) noexcept
{
    return static_cast<std::string_view>(lhs) <=> static_cast<std::string_view>(rhs);
}
template <std::size_t B>
[[nodiscard]] auto
operator<=>(const char* const lhs, const String<B>& rhs) noexcept
{
    return static_cast<std::string_view>(lhs) <=> static_cast<std::string_view>(rhs);
}
template <std::size_t A>
[[nodiscard]] auto
operator<=>(const String<A>& lhs, const char* const rhs) noexcept
{
    return static_cast<std::string_view>(lhs) <=> static_cast<std::string_view>(rhs);
}

// --------------------------------------------------------------------------------------------------------------------
// NOSONARBEGIN Sonar analysis breaks here producing loads of false positives, so we disable it completely here.

template <std::size_t C>
String<C>&
operator<<(String<C>& str, const std::string_view x) noexcept
{
    str += x;
    return str;
}

template <std::size_t C>
String<C>&
operator<<(String<C>& str, const char* const x) noexcept
{
    str += x;
    return str;
}

template <std::size_t C, std::size_t Z> // This explicit overload is needed to avoid matching on std::ranges::range.
String<C>&
operator<<(String<C>& str, const String<Z>& x) noexcept
{
    return str << static_cast<std::string_view>(x);
}

template <std::size_t C, std::integral T>
String<C>&
operator<<(String<C>& str, const T& x)
{
    str += IntAsString<T>(x);
    return str;
}

template <std::size_t C, std::floating_point T>
String<C>&
operator<<(String<C>& str, const T& x)
{
    str += (FloatAsString<T>(x));
    return str;
}

template <std::size_t C, typename T>
    requires std::is_enum_v<T>
String<C>&
operator<<(String<C>& str, const T& x)
{
    str << static_cast<std::underlying_type_t<T>>(x);
    return str;
}

template <std::size_t C>
String<C>&
operator<<(String<C>& str, const volatile void* const x) noexcept // NOSONAR void*
{
    str += '0';
    str += 'x';
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    str += (IntAsString<std::size_t, 16>(reinterpret_cast<std::size_t>(x))); // NOSONAR pointer to integer
    return str;
}

template <std::size_t C, typename Rep, typename Period>
String<C>&
operator<<(String<C>& str, const std::chrono::duration<Rep, Period>& x)
{
    const auto s = std::chrono::duration_cast<std::chrono::seconds>(x);
    str << s.count() << ".";
    const IntAsString     ns(std::abs(std::chrono::duration_cast<std::chrono::nanoseconds>(x - s).count()));
    constexpr std::int8_t width  = 9;
    auto                  n_fill = width - static_cast<std::int8_t>(ns.size());
    while (n_fill > 0) {
        n_fill--;
        str += '0';
    }
    str += ns;
    return str;
}

template <std::size_t C, typename Clock, typename Dur>
String<C>&
operator<<(String<C>& str, const std::chrono::time_point<Clock, Dur>& x)
{
    str << x.time_since_epoch();
    return str;
}

template <std::size_t C, std::ranges::range R>
String<C>&
operator<<(String<C>& str, const R& x)
{
    str += '[';
    for (bool first = true; const auto& it : x) {
        if (!first) {
            str += ',';
        }
        first = false;
        str << it;
    }
    str += ']';
    return str;
}

template <std::size_t C, typename... A>
String<C>&
operator<<(String<C>& str, const std::tuple<A...>& x)
{
    str += '(';
    if constexpr (sizeof...(A) > 0) {
        std::apply(
          [&]<typename H, typename... Ts>(H&& head, Ts&&... tail) {
              str << std::forward<H>(head);
              (..., (str += ',', str << std::forward<Ts>(tail)));
          },
          x);
    }
    str += ')';
    return str;
}

template <std::size_t C, typename... A>
String<C>&
operator<<(String<C>& str, const std::variant<A...>& x)
{
    return std::visit([&str](const auto& val) -> String<C>& { return str << val; }, x);
}

template <std::size_t C, typename Left, typename Right>
String<C>&
operator<<(String<C>& str, const std::pair<Left, Right>& x)
{
    str += '(';
    str << x.first;
    str += ':';
    str << x.second;
    str += ')';
    return str;
}

template <std::size_t C, typename M>
String<C>&
operator<<(String<C>& str, const std::optional<M>& x)
{
    if (x.has_value()) {
        str << x.value();
    }
    return str;
}

template <std::size_t C, typename M>
    requires requires(const M m) {
        m(0, 0);
        { m.rows() } -> std::integral;
        { m.cols() } -> std::integral;
    }
String<C>&
operator<<(String<C>& str, const M& matrix)
{
    using Idx = decltype(std::declval<M>().rows() + std::declval<M>().cols());
    str += '[';
    const Idx rows = matrix.rows();
    const Idx cols = matrix.cols();
    for (Idx row = 0; row < rows; row++) {
        if (row > 0) {
            str += (cols > 1) ? ",\n " : ",";
        }
        str += '[';
        for (Idx col = 0; col < cols; col++) {
            if (col > 0) {
                str += ',';
            }
            str << matrix(row, col);
        }
        str += ']';
    }
    str += ']';
    return str;
}

/// Support non-standard numerical types like FixedPoint<>, Saturating<>, boost::mpl, etc.
/// We simply check if std::numeric_limits<> is defined for the type and then convert to a suitable native.
/// This will not work if the value is not representable in the native type.
template <std::size_t C, typename T>
    requires((!std::is_arithmetic_v<T>) && std::numeric_limits<std::decay_t<T>>::is_specialized) //
String<C>&
operator<<(String<C>& str, const T& x)
{
    if constexpr (std::numeric_limits<T>::is_integer && std::numeric_limits<T>::is_signed) {
        str << static_cast<std::intmax_t>(x);
    } else if constexpr (std::numeric_limits<T>::is_integer && (!std::numeric_limits<T>::is_signed)) {
        str << static_cast<std::uintmax_t>(x);
    } else {
        str << static_cast<float>(x);
    }
    return str;
}

// NOSONAREND
// --------------------------------------------------------------------------------------------------------------------

/// A helper that constructs a String<N> and formats the specified arguments into it using operator<<.
/// Users can therefore customize formatting for their types by overloading operator<<.
template <std::size_t N, typename... A>
[[nodiscard]] String<N>
format(A&&... ar)
{
    String<N> sb;
    (sb << ... << std::forward<A>(ar));
    return sb;
}
template <std::size_t N, typename... A>
[[nodiscard]] String<N>
formatln(A&&... ar)
{
    return format<N>(std::forward<A>(ar)..., "\n");
}

} // namespace tolstoy
