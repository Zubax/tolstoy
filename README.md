# Tolstoy

[![CI](https://github.com/Zubax/tolstoy/actions/workflows/ci.yml/badge.svg)](https://github.com/Zubax/tolstoy/actions/workflows/ci.yml)
[![Style](https://github.com/Zubax/tolstoy/actions/workflows/style.yml/badge.svg)](https://github.com/Zubax/tolstoy/actions/workflows/style.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Standard: C++23](https://img.shields.io/badge/standard-C%2B%2B23-blue.svg)](https://en.cppreference.com/w/cpp/23)

**Tolstoy** is a header-only C++23 string and formatting library for deeply embedded systems.
The `tolstoy::String<N>` class holds an N-byte buffer internally.
There is no heap allocation, no exceptions, no RTTI, and no macros.
Tolstoy has no dependencies outside of a small subset of the C++ standard library.

JSON and TSV serializers are provided out of the box since these are often useful in semihosted verification suites.

The string operators are extensible via ADL.

## Headers and namespaces

| Header                       | Namespace       | Purpose                                              |
|------------------------------|-----------------|------------------------------------------------------|
| `#include <tolstoy.hpp>`     | `tolstoy`       | `String<N>`, `format<N>`, `operator<<`               |
| `#include <tolstoy/json.hpp>`| `tolstoy::json` | Streaming JSON writer with ADL customization         |
| `#include <tolstoy/tsv.hpp>` | `tolstoy::tsv`  | Streaming TSV writer with compile-time column count  |

## Installation

Add `tolstoy/include/` to your include path — the library has no built artefacts.
You can add it as a Git submodule or simply copy the sources.

## Usage

`tolstoy::String<N>` holds the string internally and has a streaming `operator<<`:

```cpp
String<64> sb;
sb << "value=" << 42 << " pi=" << 3.14;
std::puts(sb.c_str());           // value=42 pi=+3.14000e+00

String<8> tiny;
tiny += "this is too long";      // silently truncated
assert(tiny.full());
assert(tiny == "this is ");

// Iterable, comparable, std::string_view-convertible.
for (char c : tiny) { /* ... */ }
if (tiny == "this is ") { /* ... */ }
```

`operator<<` knows a lot of types:

```cpp
String<256> sb;
sb << "int="    << 42                                           << "\n"
   << "float="  << 3.14F                                        << "\n"
   << "ptr="    << reinterpret_cast<void*>(0xDEADBEEF)          << "\n"
   << "pair="   << std::make_pair(1, "two")                     << "\n"
   << "tuple="  << std::make_tuple("abc", 123, 4.5)             << "\n"
   << "opt="    << std::optional<int>{99}                       << "\n"
   << "empty="  << std::optional<int>{}                         << "\n"
   << "array="  << std::array<int, 3>{1, 2, 3}                  << "\n"
   << "dur="    << std::chrono::nanoseconds{1'234'567'890}      << "\n";
```

Thin helpers `format<N>` / `formatln<N>` are available:

```cpp
auto greeting = tolstoy::format<64>("Hello, ", "world! n=", 42);
// greeting is a String<64> containing "Hello, world! n=42"

auto line = tolstoy::formatln<64>("x=", 1, " y=", 2);  // trailing "\n"
```

Custom types via [argument-dependent lookup (ADL)](https://en.cppreference.com/cpp/language/adl):

```cpp
namespace myapp
{
struct Voltage { float value; };

// Define operator<< in the same namespace as your type; tolstoy will find it via ADL.
template <std::size_t N>
tolstoy::String<N>& operator<<(tolstoy::String<N>& s, const Voltage& v)
{
    return s << v.value << "V";
}
}  // namespace myapp

tolstoy::String<32> sb;
sb << "batt=" << myapp::Voltage{12.34F};   // "batt=+1.23400e+01V"
```

### Streaming JSON

JSON can serialize anything that `String::operator<<` can, including ADL customization points:

```cpp
#include <tolstoy/json.hpp>

std::string out;
auto writer = [&](std::string_view x) -> bool { out.append(x); return true; };

tolstoy::json::Json j(writer);
if (auto obj = j.object())
{
    (void) obj->operator()("name",  "tolstoy");
    (void) obj->operator()("vers",  1);
    (void) obj->operator()("empty", std::nullopt);
    (void) obj->operator()("tags",  std::array{"text", "embedded", "c++23"});
}
// out == R"({"name":"tolstoy","vers":1,"empty":null,"tags":["text","embedded","c++23"]})"
```

JSON-lines is natural: every use of `++j` starts a fresh document separated by a newline.

```cpp
tolstoy::json::Json j(writer);
(void) j++.value()(std::array{1, 2, 3});
(void) j++.value()(std::array{"a", "b"});
// out == "[1,2,3]\n[\"a\",\"b\"]"
```

Custom types hook into JSON via ADL:

```cpp
namespace myapp
{
struct Point { int x, y; };

inline bool json_from(tolstoy::json::JsonValue&& into, const Point& p)
{
    auto obj = std::move(into).object();
    return obj("x", p.x) && obj("y", p.y);
}
}  // namespace myapp

std::string out;
tolstoy::json::Json j([&](std::string_view x) { out.append(x); return true; });
(void) j++.value()(myapp::Point{3, 4});
// out == R"({"x":3,"y":4})"
```

### Streaming TSV

```cpp
#include <tolstoy/tsv.hpp>

std::string out;
auto writer = [&](std::string_view x) -> bool { out.append(x); return true; };

auto tsv = tolstoy::tsv::Tsv<4, decltype(writer)>::make(
               std::move(writer),
               std::array<std::string_view, 4>{"t", "x", "y", "z"}).value();

(void) tsv.row(0.0F, 1, 2, 3);
(void) tsv.row(1.0F, std::array<int, 3>{4, 5, 6});   // expands in row-major
// out ==
// "t"   "x"   "y"   "z"
// +0.00000e+00   1   2   3
// +1.00000e+00   4   5   6
```

The column count is a template parameter; `row(...)` validates it at compile time.

### Embedded use (no `std::function`)

The JSON and TSV writers accept any callable with signature `bool(std::string_view)`.
The default is `std::function<>`, which may allocate on the heap for large captures.
To avoid that, you have the option to pass a lambda directly (CTAD deduces the lambda's type),
use [`ramen::Function<>`](https://github.com/Zubax/ramen),
or supply your own function wrapper as the `WriterFn` template parameter.

```cpp
auto writer = [](std::string_view s) -> bool { serial_write(s); return true; };
tolstoy::json::Json j(writer);   // WriterFn deduced to the lambda's type; no std::function
```
