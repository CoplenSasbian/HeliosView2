#include <HeliosView2/HeliosView2.h>

#include <doctest/doctest.h>

#include <meta>
#include <string_view>

namespace {

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Named {
    int id;
    std::string_view name;
};

}  // namespace

TEST_CASE("library version is injected by CMake") {
    CHECK(!helios::version().empty());
    CHECK(helios::version() == "0.1.0");
}

TEST_CASE("reflection counts non-static data members") {
    static_assert(helios::field_count<Vec3>() == 3);
    static_assert(helios::field_count<Named>() == 2);
    CHECK(helios::field_count<Vec3>() == 3);
}

TEST_CASE("reflection exposes identifiers at compile time") {
    static_assert(helios::type_name<Vec3>() == "Vec3");
    static_assert(std::meta::identifier_of(^^Vec3::x) == "x");
    static_assert(std::meta::identifier_of(^^Vec3::y) == "y");
    static_assert(std::meta::identifier_of(^^Vec3::z) == "z");
    CHECK(std::meta::identifier_of(^^Named) == "Named");
}
