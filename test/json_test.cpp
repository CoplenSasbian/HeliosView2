// Exercises the vendored JSON library (glaze) so that future code can rely on
// it: plain structs, explicit glz::meta names, nested containers, escaping and
// error reporting.

#include <glaze/glaze.hpp>

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {

struct point {
    std::int32_t x{};
    std::int32_t y{};
};

struct shape {
    std::string name;
    std::vector<point> points;
    bool visible{true};
};

}  // namespace

// The JSON keys are fixed on purpose: they are the wire format, not the C++
// member names.
template <>
struct glz::meta<point> {
    using T = point;
    static constexpr auto value = glz::object("x", &T::x, "y", &T::y);
};

template <>
struct glz::meta<shape> {
    using T = shape;
    static constexpr auto value = glz::object(
            "name", &T::name,
            "points", &T::points,
            "visible", &T::visible);
};

// glaze escapes control characters only when the options type opts in; this is
// the documented way to extend glz::opts.
struct escaping_opts : glz::opts {
    bool escape_control_characters = true;
};

TEST_CASE("json writes a struct with the declared member names") {
    std::string text;
    const auto error = glz::write_json(point{3, -4}, text);

    REQUIRE_FALSE(error);
    CHECK(text == R"({"x":3,"y":-4})");
}

TEST_CASE("json round-trips nested containers") {
    const shape original{"triangle", {{0, 0}, {10, 0}, {0, 10}}, false};

    std::string text;
    REQUIRE_FALSE(glz::write_json(original, text));
    CHECK(text.find(R"("points":[{"x":0,"y":0})") != std::string::npos);

    shape parsed;
    REQUIRE_FALSE(glz::read_json(parsed, text));
    CHECK(parsed.name == "triangle");
    REQUIRE(parsed.points.size() == 3);
    CHECK(parsed.points[2].y == 10);
    CHECK_FALSE(parsed.visible);
}

TEST_CASE("json escapes strings and reads them back unchanged") {
    const std::string tricky = "quote \" backslash \\ tab \t newline \n control \x01 end";

    // Control characters are only escaped with this option; glaze's default
    // fast path writes NUL bytes instead.
    std::string text;
    REQUIRE_FALSE(glz::write<escaping_opts{}>(shape{tricky, {}, true}, text));
    CHECK(text.find("\\\"") != std::string::npos);
    CHECK(text.find("\\n") != std::string::npos);
    CHECK(text.find("\\u0001") != std::string::npos);

    shape parsed;
    REQUIRE_FALSE(glz::read_json(parsed, text));
    CHECK(parsed.name == tricky);
}

TEST_CASE("json pretty output is still parseable") {
    std::string text;
    REQUIRE_FALSE(glz::write<glz::opts{.prettify = true}>(point{1, 2}, text));
    CHECK(text.find('\n') != std::string::npos);

    point parsed;
    REQUIRE_FALSE(glz::read_json(parsed, text));
    CHECK(parsed.x == 1);
    CHECK(parsed.y == 2);
}

TEST_CASE("malformed json reports an error instead of throwing") {
    point parsed;
    const auto error = glz::read_json(parsed, std::string_view{R"({"x": })"});
    CHECK(static_cast<bool>(error));
    CHECK(error.ec != glz::error_code::none);

    std::string text;
    REQUIRE_FALSE(glz::read_json(parsed, std::string_view{R"({"x":7})"}));
    CHECK(parsed.x == 7);
    CHECK(parsed.y == 0);
}
