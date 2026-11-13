// Rendering lives in the DLL so the header only carries the data. JSON goes
// through glaze like every other JSON in this library; only the one line text
// form is written by hand.

#include <HeliosView2/Error.h>

#include <HeliosView2/detail/Json.h>

#include <glaze/glaze.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace helios::detail {

/// The JSON shape of one frame; `cause` nests the chain, so the output is one
/// object with a single "cause" branch per layer. The string_view fields point at
/// the error's own storage and are only valid while it is being written.
struct error_json_frame {
    std::int32_t code{};
    std::string_view name;
    std::string_view message;
    std::string_view context;
    std::string_view file;
    std::uint32_t line{};
    std::uint32_t column{};
    std::string_view function;
    std::vector<error_json_frame> cause;
};

}  // namespace helios::detail

// Member names are written out rather than reflected: the JSON keys are the wire
// format of the error output and must not follow a C++ rename.
template <>
struct glz::meta<helios::detail::error_json_frame> {
    using T = helios::detail::error_json_frame;
    static constexpr auto value = glz::object(
            "code", &T::code,
            "name", &T::name,
            "message", &T::message,
            "context", &T::context,
            "file", &T::file,
            "line", &T::line,
            "column", &T::column,
            "function", &T::function,
            "cause", &T::cause);
};

namespace helios {

namespace {

[[nodiscard]] detail::error_json_frame build_json(std::span<const error_frame> frames,
                                                  std::size_t index) {
    const error_frame& frame = frames[index];

    detail::error_json_frame node;
    node.code = frame.code;
    node.name = helios::to_string(frame.code);
    node.message = frame.message;
    node.context = frame.context;
    node.file = frame.file;
    node.line = frame.line;
    node.column = frame.column;
    node.function = frame.function;
    if (index + 1 < frames.size()) node.cause.push_back(build_json(frames, index + 1));
    return node;
}

}  // namespace

std::string error::to_string() const {
    std::string text;
    for (const auto& frame : m_frames) {
        if (!text.empty()) text += " <- caused by: ";
        if (!frame.context.empty()) {
            text += frame.context;
            text += ": ";
        }
        text += frame.message;

        if (!frame.file.empty()) {
            text += " at ";
            text += frame.file;
            if (frame.line != 0) {
                text += ':';
                text += std::to_string(frame.line);
                if (frame.column != 0) {
                    text += ':';
                    text += std::to_string(frame.column);
                }
            }
        }
    }
    return text.empty() ? std::string{"no error"} : text;
}

const char* error::what() const noexcept {
    try {
        m_what = to_string();
    } catch (...) {
    }
    return m_what.empty() ? "no error" : m_what.c_str();
}

std::string error::to_json() const {
    if (m_frames.empty()) return "null";

    const detail::error_json_frame node =
            build_json(std::span<const error_frame>{m_frames}, 0);

    std::string out;
    out.reserve(256);
    // glaze leaves control characters to its fast path, which emits NUL bytes
    // and therefore invalid JSON, so escaping is requested explicitly.
    (void)glz::write<detail::json_opts{}>(node, out);
    return out;
}

std::string to_json(const error& value) {
    return value.to_json();
}

}  // namespace helios
