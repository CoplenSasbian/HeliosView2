#pragma once

// Errors, without any ABI ceremony.
//
// helios::error is a plain value: a chain of frames, each with the message, the
// context it happened in and the call site. Rendering (text and JSON) lives in
// the DLL, so this header only carries the data and the small builders.
//
// Every fallible call throws helios::error, which derives from std::exception and
// carries the whole chain. There is no "last error" thread-local and no result
// wrapper: the detail travels with the exception, and the caller decides whether
// to unwind or to inspect it.

#include <HeliosView2/HeliosView2_export.h>
#include <HeliosView2/detail/Format.h>
#include <HeliosView2/detail/Path.h>

#include <cstdint>
#include <cstdio>
#include <exception>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace helios {

using status = std::int32_t;

inline constexpr status ok = 0;
inline constexpr status invalid_argument = -2;
inline constexpr status wrong_thread = -3;
inline constexpr status unsupported = -4;
inline constexpr status out_of_memory = -5;
inline constexpr status not_found = -6;
inline constexpr status invalid_state = -7;
inline constexpr status cancelled = -8;
inline constexpr status timeout = -9;

/// An operating system call failed; the system description is already part of
/// the message, so nothing platform specific appears in this type.
inline constexpr status platform_failure = -10;

/// A callable handed to the library threw; its message becomes the detail.
inline constexpr status unhandled_exception = -11;

[[nodiscard]] constexpr bool succeeded(status code) noexcept { return code >= 0; }
[[nodiscard]] constexpr bool failed(status code) noexcept { return code < 0; }

/// Stable name of a status code, for logs and messages.
[[nodiscard]] constexpr std::string_view to_string(status code) noexcept {
    switch (code) {
    case ok: return "ok";
    case invalid_argument: return "invalid_argument";
    case wrong_thread: return "wrong_thread";
    case unsupported: return "unsupported";
    case out_of_memory: return "out_of_memory";
    case not_found: return "not_found";
    case invalid_state: return "invalid_state";
    case cancelled: return "cancelled";
    case timeout: return "timeout";
    case platform_failure: return "platform_failure";
    case unhandled_exception: return "unhandled_exception";
    default: return "unknown";
    }
}

/// One layer of an error: what failed, while doing what, and where.
struct error_frame {
    status code{};
    std::string message;
    std::string context;
    std::string file;
    std::string function;
    std::uint32_t line{};
    std::uint32_t column{};

    bool operator==(const error_frame&) const = default;
};

class HELIOSVIEW2_EXPORT error : public std::exception {
public:
    error() = default;
    explicit error(std::vector<error_frame> frames) : m_frames(std::move(frames)) {}

    [[nodiscard]] bool valid() const noexcept { return !m_frames.empty(); }

    [[nodiscard]] status code() const noexcept {
        return m_frames.empty() ? ok : m_frames.front().code;
    }

    /// The outermost frame: frame() requires valid() to be true.
    [[nodiscard]] const error_frame& frame() const noexcept { return m_frames.front(); }

    /// Outermost frame first, innermost cause last.
    [[nodiscard]] std::span<const error_frame> frames() const noexcept { return m_frames; }

    /// One line: "context: message at src/x.cpp:12 <- caused by: ...".
    [[nodiscard]] HELIOSVIEW2_EXPORT std::string to_string() const;

    /// std::exception hook; the same text as to_string(), cached.
    const char* what() const noexcept override;

    /// JSON object; the cause chain nests under "cause".
    [[nodiscard]] HELIOSVIEW2_EXPORT std::string to_json() const;

    bool operator==(const error&) const = default;

private:
    std::vector<error_frame> m_frames;
    mutable std::string m_what;
};


/// Builds a frame whose call site is `location`.
[[nodiscard]] inline error_frame make_frame(status code,
                                            std::string_view context,
                                            std::string message,
                                            std::source_location location) {
    error_frame frame;
    frame.code = code;
    frame.message = std::move(message);
    frame.context = std::string(context);
    frame.file = std::string(detail::relative_path(location.file_name()));
    frame.line = location.line();
    frame.column = location.column();
    frame.function = std::string(detail::short_function_name(location.function_name()));
    return frame;
}

/// Builds an error whose call site is the call itself.
///
///     throw helios::make_error(helios::invalid_state, "starting the loop",
///                              "already running: {}", count);
template <class... Args>
[[nodiscard]] error make_error(status code,
                               std::string_view context,
                               detail::location_fmt_string<Args...> pattern,
                               Args&&... args) {
    return error{std::vector<error_frame>{make_frame(
            code, context, pattern.format(std::forward<Args>(args)...), pattern.location)}};
}

/// Builds an error from an already formatted message; the layer that saw a
/// platform failure uses it to append the system description.
[[nodiscard]] inline error make_error_text(status code,
                                           std::string_view context,
                                           std::string message,
                                           std::source_location location = std::source_location::current()) {
    return error{std::vector<error_frame>{make_frame(code, context, std::move(message), location)}};
}

/// Adds a layer in front of an existing error, keeping it as the cause.
template <class... Args>
[[nodiscard]] error with_context(error inner,
                                 status code,
                                 std::string_view context,
                                 detail::location_fmt_string<Args...> pattern,
                                 Args&&... args) {
    std::vector<error_frame> frames;
    frames.reserve(inner.frames().size() + 1);
    frames.push_back(make_frame(code, context, pattern.format(std::forward<Args>(args)...),
                                pattern.location));
    for (const auto& frame : inner.frames()) frames.push_back(frame);
    return error{std::move(frames)};
}

/// A generic error for a bare status, for the rare place that cannot say more.
[[nodiscard]] inline error error_for(status code) {
    return make_error(code, "call", "failed with status {}", helios::to_string(code));
}


/// JSON form of an error.
[[nodiscard]] HELIOSVIEW2_EXPORT std::string to_json(const error& value);

}  // namespace helios

template <>
struct std::formatter<helios::error> {
    bool m_json = false;

    constexpr auto parse(std::format_parse_context& context) {
        auto it = context.begin();
        const auto end = context.end();
        if (it == end || *it == '}') return it;

        const auto name_begin = it;
        while (it != end && *it != '}') ++it;

        const std::string_view name{name_begin, it};
        if (name == "json") {
            m_json = true;
        } else if (name != "fmt") {
            throw std::format_error("invalid error format specifier: use {} or {:json}");
        }
        return it;
    }

    template <class Context>
    auto format(const helios::error& value, Context& context) const {
        const std::string text = m_json ? value.to_json() : value.to_string();
        return std::format_to(context.out(), "{}", text);
    }
};
