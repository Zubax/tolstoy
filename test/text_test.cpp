// Copyright (c)  Zubax Robotics  <zubax.com>
// Author: Pavel Kirienko <pavel.kirienko@zubax.com>

#include <tolstoy.hpp>

#include "doctest.h"

#include <array>
#include <optional>
#include <tuple>
#include <variant>

namespace tolstoy::test {
namespace {

TEST_CASE("int_as_string")
{
    {
        IntAsString s(3);
        CHECK(s.size() == 1);
        CHECK(s.length() == 1);
        CHECK(std::string_view{ s.c_str() } == "3");
        CHECK(s.capacity() == s.max_size());
        CHECK(s.capacity() > 6);
        CHECK(std::string_view{ s.data() } == std::string_view{ s.c_str() });
    }
    {
        const IntAsString s(-3);
        CHECK(s.size() == 2);
        CHECK(s.length() == 2);
        CHECK(std::string_view{ s.c_str() } == "-3");
        CHECK(s.capacity() == s.max_size());
        CHECK(s.capacity() > 6);
    }
    {
        const IntAsString s(0);
        CHECK(s.size() == 1);
        CHECK(s.length() == 1);
        CHECK(std::string_view{ s.c_str() } == "0");
        CHECK(s.capacity() == s.max_size());
        CHECK(s.capacity() > 6);
    }
    {
        const IntAsString<std::int8_t> s(-128);
        CHECK(s.size() == 4);
        CHECK(s.length() == 4);
        CHECK(std::string_view{ s.c_str() } == "-128");
        CHECK(s.capacity() == s.max_size());
        CHECK(s.capacity() > 3);
    }
    {
        const IntAsString<std::int8_t> s(127);
        CHECK(s.size() == 3);
        CHECK(s.length() == 3);
        CHECK(std::string_view{ s.c_str() } == "127");
        CHECK(s.capacity() == s.max_size());
        CHECK(s.capacity() > 3);
    }
    {
        const IntAsString<std::int32_t, 16> s(0xDEADBEE);
        CHECK(s.size() == 7);
        CHECK(s.length() == 7);
        CHECK(std::string_view{ s.c_str() } == "deadbee");
        CHECK(s.capacity() == s.max_size());
        CHECK(s.capacity() > 8);
    }
    {
        const IntAsString<std::int32_t, 16> s(-0xDEADBEE);
        CHECK(s.size() == 8);
        CHECK(s.length() == 8);
        CHECK(std::string_view{ s.c_str() } == "-deadbee");
        CHECK(s.capacity() == s.max_size());
        CHECK(s.capacity() > 8);
    }
    {
        const IntAsString<std::int8_t, 2> s(-0b1010111);
        CHECK(s.size() == 8);
        CHECK(s.length() == 8);
        CHECK(std::string_view{ s.c_str() } == "-1010111");
        CHECK(s.capacity() == s.max_size());
        CHECK(s.capacity() > 8);
    }
    {
        const IntAsString<std::uint8_t, 2> s(0b1010111);
        CHECK(s.size() == 7);
        CHECK(s.length() == 7);
        CHECK(std::string_view{ s.c_str() } == "1010111");
        CHECK(s.capacity() == s.max_size());
        CHECK(s.capacity() > 7);
    }
    {
        const IntAsString<bool> s(true);
        CHECK(s.size() == 1);
        CHECK(s.length() == 1);
        CHECK(std::string_view{ s.c_str() } == "1");
        CHECK(s.capacity() == s.max_size());
        CHECK(s.capacity() >= 1);
    }
    {
        const IntAsString<bool> s(false);
        CHECK(s.size() == 1);
        CHECK(s.length() == 1);
        CHECK(std::string_view{ s.c_str() } == "0");
    }
    {
        CHECK(std::string_view{ IntAsString<std::int32_t, 36>(+0xDEADBEE).c_str() } == "3v0mb2");
        CHECK(std::string_view{ IntAsString<std::int32_t, 36>(-0xDEADBEE).c_str() } == "-3v0mb2");
        CHECK(std::string_view{ IntAsString<std::int32_t, 62>(+0xDEADBEE).c_str() } == "fNIR0");
        CHECK(std::string_view{ IntAsString<std::int32_t, 62>(-0xDEADBEE).c_str() } == "-fNIR0");
    }
}

TEST_CASE("float_as_string")
{
    // regular float
    {
        FloatAsString s(0.0F);
        CHECK(std::string_view{ s.c_str() } == "+0.00000e+00");
        CHECK(s.size() == 12);
        CHECK(s.length() == 12);
        CHECK(s.capacity() == s.max_size());
        CHECK(s.capacity() >= 12);
        CHECK(std::string_view{ s.data() } == std::string_view{ s.c_str() });
    }
    {
        const FloatAsString s(1.000123F);
        CHECK(std::string_view{ s.c_str() } == "+1.00012e+00");
        CHECK(s.size() == 12);
    }
    {
        const FloatAsString s(1.000777F);
        CHECK(std::string_view{ s.c_str() } == "+1.00078e+00");
        CHECK(s.size() == 12);
    }
    {
        const FloatAsString s(1.000123F, { .explicit_sign = false });
        CHECK(std::string_view{ s.c_str() } == "1.00012e00");
        CHECK(s.size() == 10);
    }
    {
        const FloatAsString s(0.123456F, { .explicit_sign = false });
        CHECK(std::string_view{ s.c_str() } == "1.23456e-01");
        CHECK(s.size() == 11);
    }
    {
        const FloatAsString s(-1.000777F);
        CHECK(std::string_view{ s.c_str() } == "-1.00078e+00");
        CHECK(s.size() == 12);
    }
    {
        const FloatAsString s(-1.000777F, { .explicit_sign = false });
        CHECK(std::string_view{ s.c_str() } == "-1.00078e00");
        CHECK(s.size() == 11);
    }
    {
        const FloatAsString s(-1000.0777F);
        CHECK(std::string_view{ s.c_str() } == "-1.00008e+03");
        CHECK(s.size() == 12);
    }
    {
        const FloatAsString s(-0.000000777F);
        CHECK(std::string_view{ s.c_str() } == "-7.77000e-07");
        CHECK(s.size() == 12);
    }
    {
        const FloatAsString s(0.0000007770007F);
        CHECK(std::string_view{ s.c_str() } == "+7.77001e-07");
        CHECK(s.size() == 12);
    }
    {
        const FloatAsString s(9.999999F);
        CHECK(std::string_view{ s.c_str() } == "+1.00000e+01");
        CHECK(s.size() == 12);
    }
    {
        const FloatAsString s(-9.99999F);
        CHECK(std::string_view{ s.c_str() } == "-9.99999e+00");
        CHECK(s.size() == 12);
    }
    // regular double
    {
        const FloatAsString s(0.0);
        CHECK(std::string_view{ s.c_str() } == "+0.00000000000000e+000");
        CHECK(s.size() == 22);
    }
    {
        const FloatAsString s(1.000000000000123);
        CHECK(std::string_view{ s.c_str() } == "+1.00000000000012e+000");
        CHECK(s.size() == 22);
    }
    {
        const FloatAsString s(1.000000000000777);
        CHECK(std::string_view{ s.c_str() } == "+1.00000000000078e+000");
        CHECK(s.size() == 22);
    }
    {
        const FloatAsString s(-1.000000000000777);
        CHECK(std::string_view{ s.c_str() } == "-1.00000000000078e+000");
        CHECK(s.size() == 22);
    }
    {
        const FloatAsString s(-1000000000000.0777);
        CHECK(std::string_view{ s.c_str() } == "-1.00000000000008e+012");
        CHECK(s.size() == 22);
    }
    {
        const FloatAsString s(-0.000000000000000777);
        CHECK(std::string_view{ s.c_str() } == "-7.77000000000000e-016");
        CHECK(s.size() == 22);
    }
    {
        const FloatAsString s(0.0000007770000000000007);
        CHECK(std::string_view{ s.c_str() } == "+7.77000000000001e-007");
        CHECK(s.size() == 22);
    }
    {
        const FloatAsString s(9.9999999999999999);
        CHECK(std::string_view{ s.c_str() } == "+1.00000000000000e+001");
        CHECK(s.size() == 22);
    }
    {
        const FloatAsString s(-9.99999999999999);
        CHECK(std::string_view{ s.c_str() } == "-9.99999999999999e+000");
        CHECK(s.size() == 22);
    }
    // special float
    {
        const FloatAsString s(std::numeric_limits<float>::quiet_NaN());
        CHECK(std::string_view{ s.c_str() } == "NaN");
        CHECK(s.size() == 3);
    }
    {
        const FloatAsString s(std::numeric_limits<float>::quiet_NaN(), { .explicit_sign = false });
        CHECK(std::string_view{ s.c_str() } == "NaN");
        CHECK(s.size() == 3);
    }
    {
        const FloatAsString s(std::numeric_limits<float>::infinity());
        CHECK(std::string_view{ s.c_str() } == "+Infinity");
        CHECK(s.size() == 9);
    }
    {
        const FloatAsString s(std::numeric_limits<float>::infinity(), { .explicit_sign = false });
        CHECK(std::string_view{ s.c_str() } == "Infinity");
        CHECK(s.size() == 8);
    }
    {
        const FloatAsString s(-std::numeric_limits<float>::infinity());
        CHECK(std::string_view{ s.c_str() } == "-Infinity");
        CHECK(s.size() == 9);
    }
    {
        const FloatAsString s(-std::numeric_limits<float>::infinity(), { .explicit_sign = false });
        CHECK(std::string_view{ s.c_str() } == "-Infinity");
        CHECK(s.size() == 9);
    }
    // special double
    {
        const FloatAsString s(std::numeric_limits<double>::quiet_NaN());
        CHECK(std::string_view{ s.c_str() } == "NaN");
        CHECK(s.size() == 3);
    }
    {
        const FloatAsString s(std::numeric_limits<double>::infinity());
        CHECK(std::string_view{ s.c_str() } == "+Infinity");
        CHECK(s.size() == 9);
    }
    {
        const FloatAsString s(-std::numeric_limits<double>::infinity());
        CHECK(std::string_view{ s.c_str() } == "-Infinity");
        CHECK(s.size() == 9);
    }
    {
        const FloatAsString s(std::numeric_limits<double>::infinity(), { .explicit_sign = false });
        CHECK(std::string_view{ s.c_str() } == "Infinity");
        CHECK(s.size() == 8);
    }
    {
        const FloatAsString s(-std::numeric_limits<double>::infinity(), { .explicit_sign = false });
        CHECK(std::string_view{ s.c_str() } == "-Infinity");
        CHECK(s.size() == 9);
    }
}

struct DummyClock final
{
    using rep        = std::int64_t;
    using period     = std::ratio<1, 180'000'000>;
    using duration   = std::chrono::duration<rep, period>;
    using time_point = std::chrono::time_point<DummyClock>;
};

// A minimal matrix-like type with rows()/cols()/operator()(r,c) to exercise the duck-typed matrix formatter.
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

TEST_CASE("string")
{
    {
        const String<2> sb("hui");
        CHECK(sb.size() == 2);
        CHECK(sb.length() == 2);
        CHECK(sb.max_size() == 2);
        CHECK(sb.capacity() == 2);
        CHECK(sb.front() == 'h');
        CHECK(sb.back() == 'u');
    }
    {
        String<1> sb;
        CHECK(!sb.full());
        CHECK(sb.empty());
        CHECK(sb.length() == 0);
        CHECK(sb.max_size() == 1);
        CHECK(sb.capacity() == 1);
        sb += "123";
        CHECK(sb.full());
        CHECK(sb.size() == 1);
        CHECK(sb.length() == 1);
        CHECK(sb.front() == '1');
        CHECK(sb.back() == '1');
        sb.clear();
        CHECK(sb.empty());
        CHECK(sb.length() == 0);
    }
    {
        String<8> sb;
        CHECK(sb.c_str() == sb.data());
        CHECK(!sb.full());
        CHECK(sb.empty());
        CHECK(sb.length() == 0);
        sb.push_back('9');
        CHECK(!sb.full());
        CHECK(sb.size() == 1);
        sb << "hello" << 3;
        CHECK(sb.length() == 7);
        CHECK(!sb.full());
        sb << 283 << "\n"; // Only 2 is accepted the rest dropped
        CHECK(sb.full());
        CHECK(sb.length() == 8);
        CHECK(std::string_view{ sb.c_str() } == "9hello32");
        sb.push_back('+'); // No change because full
        CHECK(sb.full());
        CHECK(sb.length() == 8);
        CHECK(std::string_view{ sb.c_str() } == "9hello32");
    }
    {
        enum class A : std::uint8_t
        {
            Z = 9
        };
        String<200> sb;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        sb << "ptr=" << reinterpret_cast<int*>(0x1234'5670) << "\n"
           << "arr=" << std::optional<std::array<std::uint8_t, 3>>{ { { 1, 2, 3 } } } << "\n"
           << "arr=" << std::optional<std::array<std::uint8_t, 3>>{} << "\n"
           << "arr=" << std::array<std::uint8_t, 0>{} << "\n"
           << "tup=" << std::make_tuple("abc", 123) << "\n"
           << "tup_opt_enum=" << std::make_tuple(std::optional<A>(A::Z)) << "\n"
           << "tup_empty=" << std::make_tuple() << "\n"
           << "pair=" << std::make_pair(123, "hello!") << "\n"
           << "tim="
           << DummyClock::time_point(
                std::chrono::duration_cast<DummyClock::duration>(std::chrono::nanoseconds(123'456'789'000)))
           << "\n"
           << "enum=" << A::Z;
        CHECK(std::string_view{ sb.c_str() } == "ptr=0x12345670\n"
                                                "arr=[1,2,3]\n"
                                                "arr=\n"
                                                "arr=[]\n"
                                                "tup=(abc,123)\n"
                                                "tup_opt_enum=(9)\n"
                                                "tup_empty=()\n"
                                                "pair=(123:hello!)\n"
                                                "tim=123.456789000\n"
                                                "enum=9");
    }
    {
        String<100>                            sb;
        std::variant<std::uint8_t, String<10>> var{};
        sb << "var=" << var;
        CHECK(std::string_view{ sb.c_str() } == "var=0");
        sb.clear();
        var.emplace<std::uint8_t>(42);
        sb << "var=" << var;
        CHECK(std::string_view{ sb.c_str() } == "var=42");
        sb.clear();
        var.emplace<String<10>>("hello");
        sb << "var=" << var;
        CHECK(std::string_view{ sb.c_str() } == "var=hello");
    }
    {
        // Duck-typed matrix formatter -- any type with rows()/cols()/operator()(r,c) works.
        String<200>      sb;
        const MatrixLike m_2_3{ .rows_ = 2, .cols_ = 3, .data_ = { 1, 2, 3, 4, 5, 6 } };
        const MatrixLike vec_4_1{ .rows_ = 4, .cols_ = 1, .data_ = { 6, 7, 8, 9 } };
        sb << "\n" << m_2_3 << "\n" << vec_4_1 << "\n";
        CHECK(std::string_view{ sb.c_str() } == "\n"
                                                "[[1,2,3],\n"
                                                " [4,5,6]]\n"
                                                "[[6],[7],[8],[9]]\n");
    }
    {
        CHECK(String<10>("abc") == String<10>("abc"));
        CHECK(String<10>("abc") != String<10>("Abc"));
        CHECK(String<10>("abc") < String<10>("bbc"));
        CHECK(String<10>("abc") >= String<10>("abc"));
        CHECK(String<10>("abd") >= String<10>("abc"));

        CHECK("abc" == String<10>("abc"));
        CHECK("abc" != String<10>("Abc"));
        CHECK("abc" < String<10>("bbc"));
        CHECK("abc" >= String<10>("abc"));
        CHECK("abd" >= String<10>("abc"));

        CHECK(String<10>("abc") == "abc");
        CHECK(String<10>("abc") != "Abc");
        CHECK(String<10>("abc") < "bbc");
        CHECK(String<10>("abc") >= "abc");
        CHECK(String<10>("abd") >= "abc");
    }
    {
        String<10> sb;
        CHECK(sb.empty());
        sb.pop_back();
        CHECK(sb.empty());
        CHECK(std::string_view{ sb.c_str() } == "");
        sb.push_back('a');
        CHECK(std::string_view{ sb.c_str() } == "a");
        sb.push_back('b');
        CHECK(std::string_view{ sb.c_str() } == "ab");
        sb.pop_back();
        CHECK(std::string_view{ sb.c_str() } == "a");
        CHECK(sb.length() == 1);
        sb.pop_back();
        CHECK(std::string_view{ sb.c_str() } == "");
        CHECK(sb.empty());
    }
}

TEST_CASE("iterator")
{
    {
        String<10> sb;
        sb << "hello";
        char* it = sb.begin();
        CHECK(it != sb.end());
        CHECK(*it == 'h');
        CHECK(*++it == 'e');
        CHECK(*++it == 'l');
        CHECK(*++it == 'l');
        CHECK(*++it == 'o');
        CHECK(++it == sb.end());
    }
    {
        const String<10> sb("hello");
        const char*      it = sb.begin();
        CHECK(it != sb.end());
        CHECK(*it == 'h');
        CHECK(*++it == 'e');
        CHECK(*++it == 'l');
        CHECK(*++it == 'l');
        CHECK(*++it == 'o');
        CHECK(++it == sb.end());
    }
}

struct CustomType
{
    std::uint8_t value;
};

template <typename stream>
stream& operator<<(stream& s, const CustomType& value)
{
    return s << "custom(" << value.value << ")";
}

TEST_CASE("format") { CHECK(std::string_view{ formatln<64>(123, CustomType{ 210 }).c_str() } == "123custom(210)\n"); }

TEST_CASE("stream_operator")
{
    {
        String<64> sb;
        sb << "hello" << 123 << " " << CustomType{ 123 };
        CHECK(std::string_view{ sb.c_str() } == "hello123 custom(123)");
    }
}

} // namespace
} // namespace tolstoy::test
