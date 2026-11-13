#pragma once

#include <HeliosView2/HeliosView2_export.h>

#include <cstddef>
#include <string_view>

// GCC 16 defines __cpp_impl_reflection when compiled with -freflection.
#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202603L
#    define HELIOSVIEW2_HAS_REFLECTION 1
#    include <meta>
#else
#    define HELIOSVIEW2_HAS_REFLECTION 0
#endif

namespace helios {

/// Library version injected by CMake.
HELIOSVIEW2_EXPORT std::string_view version() noexcept;

#if HELIOSVIEW2_HAS_REFLECTION

/// Compile-time number of non-static data members.
template <class T>
consteval std::size_t field_count() {
    return std::meta::nonstatic_data_members_of(
                   ^^T, std::meta::access_context::current())
            .size();
}

/// Compile-time type identifier.
template <class T>
consteval std::string_view type_name() {
    return std::meta::identifier_of(^^T);
}

#else

template <class T>
consteval std::size_t field_count() {
    static_assert(sizeof(T) == 0,
                  "C++26 reflection unavailable: compile with -std=c++26 -freflection (GCC 16+)");
    return 0;
}

template <class T>
consteval std::string_view type_name() {
    static_assert(sizeof(T) == 0,
                  "C++26 reflection unavailable: compile with -std=c++26 -freflection (GCC 16+)");
    return {};
}

#endif

}  // namespace helios
