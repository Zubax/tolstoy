// Copyright (c)  Zubax Robotics  <zubax.com>
// Author: Pavel Kirienko <pavel.kirienko@zubax.com>

#pragma once

#include <tolstoy/tolstoy.hpp>
#include <algorithm>
#include <span>
#include <concepts>
#include <functional>
#include <string_view>

namespace tolstoy::json {
class JsonValue;
class JsonObject;
class JsonArray;

// ========================================  DETAIL FUNDAMENTALS  ========================================

namespace detail {
/// The interface is made move-constructible to avoid limiting the usage options too much.
/// It is not assignable nor copy-constructible to avoid dangling references in the child emitters: JsonValue, etc.
class JsonEmitter
{
  public:
    virtual void               backlog(const std::string_view text) noexcept = 0;
    [[nodiscard]] virtual bool emit(const std::string_view text)             = 0;

    JsonEmitter() noexcept                         = default;
    JsonEmitter(const JsonEmitter&)                = delete;
    JsonEmitter(JsonEmitter&&) noexcept            = default;
    JsonEmitter& operator=(const JsonEmitter&)     = delete;
    JsonEmitter& operator=(JsonEmitter&&) noexcept = delete;

  protected:
    ~JsonEmitter() noexcept = default;
};

/// A minimal RAII helper that emits a fixed literal to the emitter's backlog on destruction unless disarmed.
/// Replaces a general type-erased finalizer because all three call sites only need to emit a small literal.
class FinalizerLiteral final
{
  public:
    FinalizerLiteral(JsonEmitter& em, const std::string_view text) noexcept
      : em_(&em)
      , text_(text)
    {
    }
    FinalizerLiteral(const FinalizerLiteral&)            = delete;
    FinalizerLiteral& operator=(const FinalizerLiteral&) = delete;
    FinalizerLiteral(FinalizerLiteral&& other) noexcept
      : em_(other.em_)
      , text_(other.text_)
    {
        other.em_ = nullptr;
    }
    FinalizerLiteral& operator=(FinalizerLiteral&&) noexcept = delete;

    void disarm() noexcept { em_ = nullptr; }

    ~FinalizerLiteral() noexcept
    {
        if (em_ != nullptr) {
            em_->backlog(text_);
        }
    }

  private:
    JsonEmitter*     em_;
    std::string_view text_;
};

/// Returns an escape sequence for the character; empty string if no escape is needed.
[[nodiscard]] constexpr std::string_view escape(const char ch) noexcept
{
    using namespace std::literals; // NOSONAR acceptable use for a using directive.
    switch (ch) { // clang-format off
            case '"':  { return R"(\")"sv; }
            case '\\': { return R"(\\)"sv; }
            case '\b': { return R"(\b)"sv; }
            case '\f': { return R"(\f)"sv; }
            case '\n': { return R"(\n)"sv; }
            case '\r': { return R"(\r)"sv; }
            case '\t': { return R"(\t)"sv; }
            default:   { return {}; }
        }                              // clang-format on
}

template <typename T>
concept number = std::integral<T> || std::floating_point<T> || std::is_same_v<T, bool>;

template <number T>
struct NumberAsString;
template <std::floating_point T>
struct NumberAsString<T> : public FloatAsString<T>
{
    explicit NumberAsString(const T x) noexcept
      : FloatAsString<T>(x, { .explicit_sign = false })
    {
    }
};
template <std::integral T>
struct NumberAsString<T> : public IntAsString<T>
{
    explicit NumberAsString(const T x) noexcept
      : IntAsString<T>(x)
    {
    }
};
template <>
struct NumberAsString<bool>
{
    explicit NumberAsString(const bool x) noexcept
      : x_(x)
    {
    }
    // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
    [[nodiscard]] operator std::string_view() const noexcept // NOSONAR implicit by design
    {
        using namespace std::literals; // NOSONAR acceptable use for a using directive.
        return x_ ? "true"sv : "false"sv;
    }

  private:
    const bool x_;
};
template <typename T>
NumberAsString(const T&) -> NumberAsString<T>;
} // namespace detail

// ========================================  JSON VALUE  ========================================

/// Objects of this class can only be used as rvalues. If an object is stored as an lvalue, use std::move()
/// to invoke its methods.
/// Only one of the public methods/operators can be invoked at most once during the object's lifetime;
/// otherwise, the output JSON will be malformed. If no methods are invoked, "null" is written out at destruction.
///
/// operator() writes a JSON value and returns true on success, false if the output stream could not be written.
/// The actual serialization behavior is defined by the json_from() function overloads, which are invoked by the
/// operator() via ADL. Some of the standard implementations of json_from() are given out of the box:
///
/// - primitives -- strings, numbers, bools.
/// - variant -- unwrapped.
/// - optional -- value is written as-is if it's engaged, and as a literal "null" if it's empty.
/// - range, tuple, or initializer_list of anything -- written as an array.
/// - matrix -- written out as a row-major nested array.
/// - enumeration -- written as its underlying type.
/// - chrono time_point or duration -- emitted as a floating point number of seconds with nanosecond resolution.
///
/// Check the actual json_from() function overloads for the additional information.
/// Define custom overloads of json_from() in the same namespace as the type of the second argument (not here).
/// Note that we don't have to forward-declare json_from() because it depends on JsonValue and thus pulled in by ADL.
class JsonValue final
{
    // A few of the json_from() overloads are friends because they implement the primitive serialization that all
    // other overloads depend on: string, number (incl. boolean), and null.
    friend bool json_from(JsonValue&& into, const std::string_view x);
    template <detail::number T>
    friend bool json_from(JsonValue&& into, const T x);
    friend bool json_from(JsonValue&& into, const std::monostate); // NOSONAR unnamed parameter
    template <typename Rep, typename Period>
    friend bool json_from(JsonValue&& into, const std::chrono::duration<Rep, Period>& x);

  public:
    /// Make this value a JSON object. This will no longer be usable afterward.
    [[nodiscard]] JsonObject object() && noexcept;

    /// Make this value a JSON array. This will no longer be usable afterward.
    [[nodiscard]] JsonArray array() && noexcept;

    /// operator() for proxying to json_from() -- the customization point for custom types (with some stock overloads).
    template <typename T>
        requires requires(JsonValue&& j, T&& x) {
            {
                json_from(std::move(j), std::forward<T>(x))
            } -> std::same_as<bool>; // NOSONAR T&& is a forwarding reference
        }
    [[nodiscard]] bool operator()(T&& x) &&
    {
        fin_.disarm();
        return json_from(std::move(*this), std::forward<T>(x));
    }

    template <typename T>
        requires requires(JsonValue&& j, std::initializer_list<T> x) {
            { json_from(std::move(j), x) } -> std::same_as<bool>;
        }
    [[nodiscard]] bool operator()(const std::initializer_list<T> x) &&
    {
        fin_.disarm();
        return json_from(std::move(*this), x);
    }

  private:
    template <typename>
    friend class Json;
    friend class JsonObject;
    friend class JsonArray;

    [[nodiscard]] bool emit_raw(const std::string_view x) { return em_.get().emit(x); } // NOSONAR must not be const
    explicit JsonValue(detail::JsonEmitter& em) noexcept
      : em_(em)
      , fin_(em, literal_null)
    {
    }

    static constexpr std::string_view           literal_null = "null";
    std::reference_wrapper<detail::JsonEmitter> em_;
    detail::FinalizerLiteral                    fin_;
};

/// Anything that can be passed into JsonValue::operator() is considered JSON-serializable.
template <typename T>
struct IsJsonSerializable : public std::is_invocable<JsonValue, std::decay_t<T>>
{};
template <typename T>
constexpr bool is_json_serializable = IsJsonSerializable<T>::value;
template <typename T>
concept json_serializable = is_json_serializable<T>;

// ========================================  JSON OBJECT  ========================================

/// The instance shall be destroyed before writing next JSON elements to finalize the JSON object properly.
/// Untimely destruction will result in malformed JSON.
class JsonObject final
{
  public:
    template <typename K, typename T = std::decay_t<K>>
    static constexpr bool is_valid_key = std::is_constructible_v<std::string_view, T> || //
                                         requires(const T k) { IntAsString<T>(k); } ||   //
                                         requires(const T k) { FloatAsString<T>(k); };

    /// Start a new key/value pair. Empty option represents that the key header could not be written;
    /// further usage of the stream is impossible and the output is likely malformed. Non-string (numeric) keys are
    /// automatically converted to strings.
    [[nodiscard]] std::optional<JsonValue> operator[](const std::string_view key) &
    {
        if ((!first_) && (!em_.get().emit(","))) {
            return std::nullopt;
        }
        first_ = false;
        if (!JsonValue{ em_ }(key)) {
            return std::nullopt;
        }
        if (!em_.get().emit(":")) {
            return std::nullopt;
        }
        return JsonValue(em_);
    }
    [[nodiscard]] std::optional<JsonValue> operator[](const detail::number auto key) &
    {
        return (*this)[static_cast<std::string_view>(detail::NumberAsString(key))];
    }
    /// A convenience helper to append a single key/value pair to the object.
    template <typename K, json_serializable V>
        requires is_valid_key<K>
    [[nodiscard]] bool operator()(K&& key, V&& value) &
    {
        if (auto v = (*this)[std::forward<K>(key)]) {
            return std::move(*v)(std::forward<V>(value));
        }
        return false;
    }

  private:
    friend class JsonValue;
    explicit JsonObject(detail::JsonEmitter& em)
      : em_(em)
      , fin_(em, "}")
    {
        em_.get().backlog("{");
    }
    std::reference_wrapper<detail::JsonEmitter> em_;
    detail::FinalizerLiteral                    fin_;
    bool                                        first_ = true;
};

[[nodiscard]] inline JsonObject JsonValue::object() && noexcept
{
    fin_.disarm();
    return JsonObject(em_);
}

// ========================================  JSON ARRAY  ========================================

/// The instance shall be destroyed before writing next JSON elements to finalize the array properly.
/// Untimely destruction will result in malformed JSON.
class JsonArray final
{
  public:
    /// Start a new array element. Empty option represents that the element separator could not be written;
    /// further usage of the stream is impossible and the output is likely malformed.
    /// The postfix operator is needed because it allows syntax like `arr++.value()...`.
    [[nodiscard]] std::optional<JsonValue> operator++(int) & // NOLINT(cert-dcl21-cpp)
    {
        if ((!first_) && (!em_.get().emit(","))) {
            return std::nullopt;
        }
        first_ = false;
        return JsonValue(em_);
    }
    /// A convenience helper to append a single value to the array.
    template <json_serializable V>
    [[nodiscard]] bool operator()(V&& value) &
    {
        if (auto v = (*this)++) {
            return std::move(*v)(std::forward<V>(value));
        }
        return false;
    }

  private:
    friend class JsonValue;
    explicit JsonArray(detail::JsonEmitter& em)
      : em_(em)
      , fin_(em, "]")
    {
        em_.get().backlog("[");
    }
    std::reference_wrapper<detail::JsonEmitter> em_;
    detail::FinalizerLiteral                    fin_;
    bool                                        first_ = true;
};

[[nodiscard]] inline JsonArray JsonValue::array() && noexcept
{
    fin_.disarm();
    return JsonArray(em_);
}

// ========================================  JSON MAIN  ========================================

/// The streaming JSON writer writes the output JSON into the output stream immediately as it is being constructed.
/// This approach has obvious advantages but comes with one limitation: JSON elements can only be constructed
/// sequentially; it is not possible to partially construct one element, then another, then go back to the first one
/// (as that would necessitate holding an intermediate representation in memory).
///
/// The user must invoke flush() at the end of the session to ensure that all data is written to the output.
/// If this is not done, the JSON writer will attempt to flush the output from the destructor, which may fail,
/// in which case the error will be silently ignored.
///
/// The maximum nesting level is limited but the limit is very large -- hundreds of levels.
///
/// The WriterFn template parameter is the callable type that receives data. It defaults to std::function, which
/// allocates on the heap for large captures. Embedded users who cannot afford heap allocation should supply
/// their own fixed-footprint type-erased callable as WriterFn (or pass a lambda directly and rely on CTAD).
template <typename WriterFn = std::function<bool(std::string_view)>>
class Json final : private detail::JsonEmitter
{
  public:
    /// The JSON writer calls this function to write the data. The function must return true on success, false on error;
    /// once false is returned, further writes will cease and the Json object will stay in the error state forever.
    /// The function is invoked with an empty string_view as a hint to flush the write buffer (if any) to disk.
    /// The output should have a large buffer for best performance; flushing per newline is not recommended.
    using Writer = WriterFn;

    explicit Json(WriterFn dest) noexcept
      : dest_(std::move(dest))
    {
    }

    /// Moving the instance invalidates all child emitters, like JsonValue, JsonObject, etc.
    Json(Json&& other) noexcept
      : backlog_(other.backlog_)
      , dest_(std::move(other.dest_))
      , first_(other.first_)
    {
        other.backlog_.clear();
        other.first_ = true;
    }
    Json(const Json&)                = delete;
    Json& operator=(const Json&)     = delete;
    Json& operator=(Json&&) noexcept = delete;

    /// Use operator++ (either prefix or postfix) to begin generation of a new JSON document.
    /// This method can be invoked multiple times, in which case the documents will be separated by a newline.
    /// In this way, one can generate a so called JSON-lines file.
    /// An empty option in the result means that the writer has failed and the object should no longer be used.
    [[nodiscard]] std::optional<JsonValue> operator++(int) noexcept // NOLINT(cert-dcl21-cpp)
    {
        if ((!first_) && (!emit("\n"))) // The newline emission will also flush any pending closing brackets.
        {
            return std::nullopt;
        }
        first_ = false;
        return JsonValue(*this);
    }
    [[nodiscard]] std::optional<JsonValue> operator++() noexcept { return (*this)++; }

    /// A helper that returns ++.object() unless the first value() fails.
    [[nodiscard]] std::optional<JsonObject> object() noexcept
    {
        if (auto v = ++*this) {
            return std::move(*v).object();
        }
        return std::nullopt;
    }

    /// A helper that returns ++.array() unless the first value() fails.
    [[nodiscard]] std::optional<JsonArray> array() noexcept
    {
        if (auto v = ++*this) {
            return std::move(*v).array();
        }
        return std::nullopt;
    }

    /// Hint the underlying file writer that the write buffer, if any, should be dumped to disk.
    /// This may be a no-op depending on the underlying implementation.
    [[nodiscard]] bool flush() { return emit(""); }

    ~Json() noexcept { (void)flush(); }

  private:
    void               backlog(const std::string_view text) noexcept final { backlog_ += text; }
    [[nodiscard]] bool emit(const std::string_view text) final { return flush_backlog() && dest_(text); }
    [[nodiscard]] bool flush_backlog()
    {
        const bool ok = !backlog_.full(); // If full -- backlog overrun, data probably lost, output probably invalid.
        if (!backlog_.empty()) {
            if (dest_(backlog_)) {
                backlog_.clear();
                return true;
            }
            return false;
        }
        return ok;
    }

    // The peak backlog usage is when we emit a null object and then close many brackets at once; in this case,
    // the backlog needs to contain the null literal and then all the brackets: "...null]}]}...".
    // Therefore, the size of the backlog effectively limits the maximum nesting level, so it has to be large.
    // Exceeding the backlog size causes a write error, which effectively halts further JSON generation.
    tolstoy::String<256> backlog_;
    WriterFn             dest_;
    bool                 first_ = true;
};

/// CTAD deduction guide so users can write `Json json(my_lambda);` without explicit template arguments.
template <typename WriterFn>
Json(WriterFn) -> Json<WriterFn>;

// ========================================  json_from STOCK OVERLOADS  ========================================
// Add custom implementations right next to your types (not here); they will be pulled in by ADL as long as the
// definition is in the same namespace as the definition of the type of the second argument. Shall it not be possible
// to define it in the namespace of the type of the second argument, you can define it in this namespace (but not in
// this file, unless it's something very generic, like std::vector etc.).

// primitives

[[nodiscard]] inline bool json_from(JsonValue&& into, const std::string_view x)
{
    if (!into.emit_raw("\"")) {
        return false;
    }
    // There is a much better way to do it: scan the string until a character that needs escaping is found,
    // then emit the prefix, escape the character, and continue scanning.
    for (const auto c : x) {
        std::string_view s{ &c, 1 };
        if (const auto esc = detail::escape(c); !esc.empty()) {
            s = esc;
        }
        if (!into.emit_raw(s)) {
            return false;
        }
    }
    return std::move(into).emit_raw("\"");
}

[[nodiscard]] inline bool json_from(JsonValue&& into, const char* const x) // NOSONAR array to pointer decay
{
    return json_from(std::move(into), std::string_view(x));
}

template <detail::number T>
[[nodiscard]] bool json_from(JsonValue&& into, const T x)
{
    return std::move(into).emit_raw(detail::NumberAsString(x));
}

template <typename E>
    requires std::is_enum_v<E>
[[nodiscard]] bool json_from(JsonValue&& into, const E x)
{
    return json_from(std::move(into), static_cast<std::underlying_type_t<E>>(x));
}

// chrono

template <typename Rep, typename Period>
[[nodiscard]] bool json_from(JsonValue&& into, const std::chrono::duration<Rep, Period>& x)
{
    tolstoy::String<16> s;
    return std::move(into).emit_raw(s << x);
}

template <typename Clock, typename Dur>
[[nodiscard]] bool json_from(JsonValue&& into, const std::chrono::time_point<Clock, Dur>& x)
{
    return json_from(std::move(into), x.time_since_epoch());
}

// matrix-like

template <typename M>
    requires requires(const M m) {
        m(0, 0);
        { m.rows() } -> std::integral;
        { m.cols() } -> std::integral;
    }
[[nodiscard]] bool json_from(JsonValue&& into, const M& x)
{
    using Idx       = decltype(std::declval<M>().rows() + std::declval<M>().cols());
    JsonArray outer = std::move(into).array();
    const Idx rows  = x.rows();
    const Idx cols  = x.cols();
    for (Idx row = 0; row < rows; row++) {
        if (auto maybe_inner = outer++) {
            JsonArray inner = std::move(*maybe_inner).array(); // NOLINT(*-const-correctness)  No, it cannot be const.
            for (Idx col = 0; col < cols; col++) {
                if (!inner(x(row, col))) {
                    return false;
                }
            }
        } else {
            return false;
        }
    }
    return true;
}

// variant, optional

[[nodiscard]] inline bool json_from(JsonValue&& into, const std::monostate)
{
    return std::move(into).emit_raw(JsonValue::literal_null);
}

[[nodiscard]] inline bool json_from(JsonValue&& into, const std::nullopt_t)
{
    return json_from(std::move(into), std::monostate{});
}

template <typename... Ts>
    requires(sizeof...(Ts) > 0) && (is_json_serializable<Ts> && ...)
[[nodiscard]] bool json_from(JsonValue&& into, const std::variant<Ts...>& x)
{
    return std::visit([&into](const auto& v) -> bool { return json_from(std::move(into), v); }, x);
}

template <json_serializable T>
[[nodiscard]] bool json_from(JsonValue&& into, const std::optional<T>& x)
{
    return x ? json_from(std::move(into), *x) : json_from(std::move(into), std::monostate{});
}

// sequences: array, range, tuple, initializer_list

template <std::ranges::range R>
    requires(is_json_serializable<std::iter_reference_t<std::ranges::iterator_t<R>>> && (!is_string<R>))
[[nodiscard]] bool json_from(JsonValue&& into, R&& range)
{
    JsonArray arr = std::move(into).array(); // NOLINT(*-const-correctness)  No, it cannot be const.
    return std::ranges::all_of(std::forward<R>(range), [&arr]<typename T>(T&& x) { return arr(std::forward<T>(x)); });
}

template <json_serializable T>
[[nodiscard]] bool json_from(JsonValue&& into, const std::initializer_list<T> x)
{
    return json_from(std::move(into), std::span{ x });
}

template <typename... Ts>
[[nodiscard]] bool json_from(JsonValue&& into, const std::tuple<Ts...>& x)
{
    JsonArray arr = std::move(into).array(); // NOLINT(*-const-correctness)  No, it cannot be const.
    return std::apply(
      [&arr]<typename... Args>(Args&&... args) {
          return (arr(std::forward<Args>(args)) && ...); //
      },
      x);
}

} // namespace tolstoy::json
