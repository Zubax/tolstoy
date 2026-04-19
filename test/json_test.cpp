// Copyright (c)  Zubax Robotics  <zubax.com>
// Author: Pavel Kirienko <pavel.kirienko@zubax.com>

#include <tolstoy/json.hpp>

#include "doctest.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <variant>

namespace tolstoy::test {
namespace {

// An in-memory sink that accumulates emitted bytes into a std::string for inspection.
struct StringSink final
{
    std::string buf;
    auto        writer()
    {
        return [this](std::string_view x) -> bool {
            buf.append(x);
            return true;
        };
    }
};

TEST_CASE("json_api1")
{
    // language=json
    constexpr std::string_view ref =
      R"({"dict":{"\"\\\b\f\n\r\t":1,"123":"abc\n","9.87654e00":null},"list":[-1.00000e00,1,true,false,null]})";
    StringSink sink;
    {
        tolstoy::json::Json j(sink.writer());
        if (auto maybe_obj = j.object()) {
            auto& obj = maybe_obj.value();
            if (auto maybe_dict = obj["dict"]) {
                auto dict = std::move(*maybe_dict).object();
                if (auto v = dict["\"\\\b\f\n\r\t"]) {
                    CHECK(std::move(*v)(1));
                } else {
                    FAIL("");
                }
                CHECK(dict(123, "abc\n"));
                CHECK(dict[9.87654F]); // This emits a null value.
            } else {
                FAIL("");
            }
            if (auto maybe_list = obj["list"]) {
                auto list = std::move(*maybe_list).array();
                if (auto v = list++) {
                    CHECK(std::move(*v)(-1.0F));
                } else {
                    FAIL("");
                }
                CHECK(list(1));
                CHECK(list(true));
                CHECK(list(false));
                CHECK(list++); // This emits a null value.
            } else {
                FAIL("");
            }
        } else {
            FAIL("");
        }
    }
    CHECK(sink.buf == ref);
}

TEST_CASE("json_api2")
{
    // language=json
    constexpr std::string_view ref = R"([null,[],{},{"":null}])";
    StringSink                 sink;
    {
        tolstoy::json::Json j(sink.writer());
        {
            auto arr = j.array().value();
            CHECK(arr++);
            (void)std::move(arr++.value()).array();
            (void)std::move(arr++.value()).object();
            {
                auto obj = arr++.value().object();
                CHECK(obj[""]); // This creates a large backlog pressure: "null}]"
            }
        }
    }
    CHECK(sink.buf == ref);
}

TEST_CASE("json_arrays")
{
    // language=jsonl
    constexpr std::string_view ref = "[1,2,3,4,5]\n"
                                     R"(["a","b","c","d","e"])"
                                     "\n"
                                     "[[1,2,3],[4,5,6]]\n"
                                     R"(["abc",123,[1,2,3]])";
    StringSink                 sink;
    {
        tolstoy::json::Json j(sink.writer());
        CHECK(j++.value()({ 1, 2, 3, 4, 5 }));
        CHECK(j++.value()(std::array{ "a", "b", "c", "d", "e" }));
        const std::array a{
            std::array{ 1, 2, 3 },
            std::array{ 4, 5, 6 },
        };
        CHECK(j++.value()(std::span{ a }));
        CHECK(j++.value()(std::make_tuple("abc", 123, std::array{ 1, 2, 3 })));
    }
    CHECK(sink.buf == ref);
}

// A minimal matrix-like type with rows()/cols()/operator()(r,c) to exercise the matrix json_from() overload.
struct MatrixLike final
{
    int                 rows_;
    int                 cols_;
    std::array<int, 16> data_;
    [[nodiscard]] int   rows() const noexcept { return rows_; }
    [[nodiscard]] int   cols() const noexcept { return cols_; }
    [[nodiscard]] int   operator()(const int r, const int c) const noexcept
    {
        return data_.at((static_cast<std::size_t>(r) * static_cast<std::size_t>(cols_)) + static_cast<std::size_t>(c));
    }
};

TEST_CASE("json_objects")
{
    // language=jsonl
    constexpr std::string_view ref = R"({"a":123,"b":null,"c":[[1,2,3],[4,5,6],[7,8,9]],"d":{"e":456}})";
    StringSink                 sink;
    {
        tolstoy::json::Json       j(sink.writer());
        tolstoy::json::JsonObject obj = j.object().value();
        const MatrixLike          m{ .rows_ = 3, .cols_ = 3, .data_ = { 1, 2, 3, 4, 5, 6, 7, 8, 9 } };
        CHECK(obj("a", 123));
        CHECK(obj("b", std::nullopt));
        CHECK(obj("c", m));
        tolstoy::json::JsonObject obj2 = obj["d"].value().object();
        CHECK(obj2("e", 456));
    }
    CHECK(sink.buf == ref);
}

TEST_CASE("json_matrix")
{
    // language=jsonl
    constexpr std::string_view ref = R"([[1,2,3],[4,5,6],[7,8,9]])";
    StringSink                 sink;
    {
        tolstoy::json::Json j(sink.writer());
        const MatrixLike    m{ .rows_ = 3, .cols_ = 3, .data_ = { 1, 2, 3, 4, 5, 6, 7, 8, 9 } };
        CHECK(j++.value()(m));
    }
    CHECK(sink.buf == ref);
}

TEST_CASE("json_scalar_containers")
{
    // language=jsonl
    constexpr std::string_view ref = R"([2,"a",null,3,null])";
    StringSink                 sink;
    {
        tolstoy::json::Json j(sink.writer());
        auto                arr = j.array().value();
        CHECK(arr(std::variant<int, const char*>(2)));
        CHECK(arr(std::variant<int, const char*>("a")));
        CHECK(arr(std::variant<std::monostate, int, const char*>()));
        CHECK(arr(std::optional<int>(3)));
        CHECK(arr(std::optional<int>{}));
    }
    CHECK(sink.buf == ref);
}

TEST_CASE("json_special_values")
{
    // language=jsonl
    constexpr std::string_view ref = R"({"a":9.123456789,"b":123})";
    StringSink                 sink;
    {
        tolstoy::json::Json       j(sink.writer());
        tolstoy::json::JsonObject obj = j.object().value();
        using Duration                = std::chrono::duration<double>;
        CHECK(obj("a",
                  std::chrono::time_point<std::chrono::steady_clock>(
                    std::chrono::duration_cast<std::chrono::steady_clock::duration>(Duration{ 9.123456789 }))));
        enum class Enum : std::uint8_t
        {
            a = 234,
            b = 123,
        };
        CHECK(obj("b", Enum::b));
    }
    CHECK(sink.buf == ref);
}

} // namespace
} // namespace tolstoy::test
