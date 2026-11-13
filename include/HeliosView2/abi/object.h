#pragma once

// Reference-counted interface base and smart-pointer helpers for the DLL
// boundary. Interfaces are plain abstract classes deriving from unknown; object
// implementations provide add_ref/release/query_interface themselves.

#include "guid.h"

#include <atomic>
#include <concepts>
#include <cstdint>
#include <memory>

#if defined(_WIN32) && !defined(_WIN64)
#    define HELIOS_CALL __stdcall
#else
#    define HELIOS_CALL
#endif

namespace helios::abi {

using status = std::int32_t;

inline constexpr status ok = 0;
inline constexpr status no_interface = -1;
inline constexpr status invalid_argument = -2;
inline constexpr status wrong_thread = -3;
inline constexpr status unsupported = -4;
inline constexpr status out_of_memory = -5;

constexpr bool succeeded(status value) noexcept { return value >= 0; }
constexpr bool failed(status value) noexcept { return value < 0; }

// Identifier of the base interface; stable across builds.
inline constexpr guid unknown_iid{
        0x00000000u, 0x0000u, 0x0000u, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

struct unknown {
    virtual std::uint32_t add_ref() noexcept = 0;
    virtual std::uint32_t release() noexcept = 0;
    virtual status query_interface(const guid& iid, void** out) noexcept = 0;

protected:
    ~unknown() = default;
};

template <class T>
struct release_deleter {
    void operator()(T* ptr) const noexcept {
        static_assert(std::derived_from<T, unknown>, "T must implement helios::abi::unknown");
        if (ptr != nullptr) ptr->release();
    }
};

template <class T>
using ptr = std::unique_ptr<T, release_deleter<T>>;

template <class T, class TObject>
ptr<T> query(TObject& object) noexcept
    requires requires(TObject& o, const guid& iid, void** out) {
        { o.query_interface(iid, out) } -> std::convertible_to<status>;
    }
{
    void* raw = nullptr;
    if (failed(object.query_interface(T::iid, &raw)) || raw == nullptr) return {};
    return ptr<T>(static_cast<T*>(raw));
}

}  // namespace helios::abi
