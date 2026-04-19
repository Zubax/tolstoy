// Copyright (c)  Zubax Robotics  <zubax.com>
// Author: Pavel Kirienko <pavel.kirienko@zubax.com>

#pragma once

#include <tolstoy/tolstoy.hpp>
#include <variant>
#include <optional>
#include <chrono>
#include <concepts>
#include <functional>

namespace tolstoy::tsv {
/// Exports tab-separated values into a text file or elsewhere via the provided writer.
/// The column count is a compile-time constant. Each .row() call validates the correct column count at compile time.
/// The row() method can accept statically sized arrays, which are expanded automatically in the row-major order.
///
/// String conversion is done via tolstoy::String<>; this class can accept anything that is valid for String<>.
///
/// For example:
///
///     auto tsv = Tsv<4>::make([&](std::string_view x) -> bool { ... },
///                             std::array<std::string_view, 4>{"t", "a", "b", "c"}).value();
///     tsv.row(123.456, std::array<std::int8_t, 3>{1, 2, 3});  // Writes: 123.456, 1, 2, 3
///
/// Tools like Pandas can be used to load and process the TSV files.
/// Multiple Pandas dataframes (from different TSV files) can be joined on shared keys (like timestamp) as follows:
///      pd.merge(df1, df2, how="outer")
/// For more info, read the docs: https://pandas.pydata.org/pandas-docs/stable/user_guide/merging.html
///
/// The WriterFn template parameter is the callable type that receives data. It defaults to std::function, which
/// allocates on the heap for large captures. Embedded users who cannot afford heap allocation should supply
/// their own fixed-footprint type-erased callable as WriterFn (or pass a lambda directly and rely on CTAD).
template <std::size_t column_count, typename WriterFn = std::function<bool(std::string_view)>>
class Tsv final
{
  public:
    /// The maximum number of characters per TSV table cell. This can be made a template parameter if necessary.
    static constexpr std::size_t cell_capacity = 128;

    static constexpr const char* const separator = "\t"; // NOLINT(bugprone-dynamic-static-initializers)

    /// The TSV writer calls this function to write the data. The function must return true on success.
    /// The function is invoked with an empty string_view as a hint to flush the write buffer to disk.
    /// The output should have a large buffer for best performance; flushing per newline is not recommended.
    using Writer = WriterFn;

    /// Creates a new instance and writes the TSV column header row immediately.
    /// Returns an empty option if the write was unsuccessful.
    [[nodiscard]] static std::optional<Tsv> make(WriterFn                                          dst,
                                                 const std::array<std::string_view, column_count>& column_names)
    {
        if (Tsv result(std::move(dst)); result.row(column_names)) {
            return result;
        }
        return std::nullopt;
    }

    /// Nested containers such as arrays will be expanded automatically in the row major ordering.
    template <typename... A>
    [[nodiscard]] bool row(A&&... args)
    {
        const auto result = expand<0>(std::forward<A>(args)...);
        static_assert(column_count == decltype(result)::index, "Column count mismatch");
        return result.success && dest_("\n");
    }

    /// Hint the underlying file writer that the write buffer, if any, should be dumped to disk.
    /// This may be a no-op depending on the underlying implementation.
    [[nodiscard]] bool flush() { return dest_(""); } // NOSONAR non-const by design

  private:
    explicit Tsv(WriterFn dest)
      : dest_(std::move(dest))
    {
    }

    template <std::size_t idx>
    struct EmitResult final
    {
        static const constinit std::size_t index   = idx; // NOLINT(bugprone-dynamic-static-initializers)
        bool                               success = false;
    };

    template <std::size_t idx, typename H, typename... Ts>
        requires(sizeof...(Ts) > 0)
    [[nodiscard]] auto expand(H&& hd, Ts&&... tl)
    {
        const auto res_head = emit<idx>(std::forward<H>(hd));
        using Result        = decltype(expand<decltype(res_head)::index>(std::forward<Ts>(tl)...));
        Result out{ false };
        if (res_head.success) {
            out = expand<decltype(res_head)::index>(std::forward<Ts>(tl)...);
        }
        return out;
    }
    template <std::size_t idx, typename H>
    [[nodiscard]] auto expand(H&& hd)
    {
        return emit<idx>(std::forward<H>(hd));
    }

    template <std::size_t idx, typename T>
        requires((!std::integral<T>) && (!std::floating_point<T>))
    auto emit(const T& val)
    {
        tolstoy::String<cell_capacity> conv;
        if constexpr (requires { std::string_view(val); }) {
            conv << "\"" << val << "\"";
        } else {
            conv << val;
        }
        return EmitResult<idx + 1>{ (!conv.full()) && sep<idx>() && dest_(conv) };
    }
    template <std::size_t idx, std::integral T>
    auto emit(const T& val)
    {
        const IntAsString<T> conv(val);
        return EmitResult<idx + 1>{ sep<idx>() && dest_(conv) };
    }
    template <std::size_t idx, std::floating_point T>
    auto emit(const T& val)
    {
        const FloatAsString<T> conv(val);
        return EmitResult<idx + 1>{ sep<idx>() && dest_(conv) };
    }

    // Expand array into separate columns in the row major order.
    template <std::size_t idx, std::size_t S, typename E>
    [[nodiscard]] auto emit(const std::array<E, S>& val)
    {
        return emit<idx, S, E>(val.data());
    }

    // Generalized contiguous array emitter.
    template <std::size_t idx, std::size_t S, typename T>
    [[nodiscard]] auto emit(const T* const ptr) // NOSONAR one return statement is not possible
    {
        if constexpr (S > 0) {
            const auto     res  = emit<idx>(*ptr);
            const T* const next = ptr + 1U; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            using Result        = decltype(emit<decltype(res)::index, S - 1, T>(next));
            if (!res.success) {
                return Result{ false };
            }
            return emit<decltype(res)::index, S - 1, T>(next);
        }
        if constexpr (0 == S) {
            return EmitResult<idx>{ true };
        }
    }

    template <std::size_t idx, typename... Ts>
    [[nodiscard]] auto emit(const std::variant<Ts...>& val)
    {
        return std::visit([this](const auto& x) { return this->emit<idx>(x); }, val);
    }

    template <std::size_t idx>
    [[nodiscard]] bool sep() // NOSONAR non-const by design
    {
        return (idx > 0) ? dest_(separator) : true;
    }

    WriterFn dest_;
};

} // namespace tolstoy::tsv
