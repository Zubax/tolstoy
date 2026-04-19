// Copyright (c)  Zubax Robotics  <zubax.com>
// Author: Pavel Kirienko <pavel.kirienko@zubax.com>

#include <tolstoy/tsv.hpp>

#include "doctest.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <variant>

namespace tolstoy::test {
namespace {

TEST_CASE("tsv")
{
    std::string output;
    auto        sink = [&output](const std::string_view x) -> bool {
        output.append(x);
        return true;
    };
    auto tsv = tolstoy::tsv::Tsv<6, decltype(sink)>::make(
                 std::move(sink), std::array<std::string_view, 6>{ "a", "b", "c", "d", "e", "f" })
                 .value();
    CHECK(output == R"("a"	"b"	"c"	"d"	"e"	"f")"
                    "\n");

    // 4 floats (formerly a Matrix<2,2>) + bool + variant = 6 columns
    CHECK(tsv.row(std::array<float, 4>{ 1.0F, 2.0F, 3.0F, 4.0F }, true, std::variant<int, float>(9)));

    // 5 uint8_t + chrono::nanoseconds = 6 columns
    CHECK(tsv.row(std::array<std::uint8_t, 5>{ 6, 5, 4, 3, 2 }, std::chrono::nanoseconds{ 1'234'567'890LL }));

    // nested array auto-flattens in row-major order, giving 6 float columns
    CHECK(tsv.row(std::array<std::array<float, 3>, 2>{ { { 7.0F, 8.0F, 9.0F }, { 4.0F, 5.0F, 6.0F } } }));

    CHECK(tsv.flush());

    CHECK(output ==
          R"===("a"	"b"	"c"	"d"	"e"	"f"
+1.00000e+00	+2.00000e+00	+3.00000e+00	+4.00000e+00	1	9
6	5	4	3	2	1.234567890
+7.00000e+00	+8.00000e+00	+9.00000e+00	+4.00000e+00	+5.00000e+00	+6.00000e+00
)===");
}

} // namespace
} // namespace tolstoy::test
