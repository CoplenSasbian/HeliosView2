#pragma once

// Asynchronous, sink based logging.
//
// A call formats its message on the calling thread and hands it to the service,
// which copies it once into a shared record and queues that record to every
// chain. A chain owns a worker thread that formats and writes, so a slow sink
// never blocks the caller.

#include <HeliosView2/Error.h>
#include <HeliosView2/HeliosView2_export.h>
#include <HeliosView2/detail/Format.h>
#include <HeliosView2/detail/Path.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>

namespace helios::log {

enum class level : std::int32_t {
    trace = 0,
    debug = 1,
    info = 2,
    warning = 3,
    error = 4,
    critical = 5,
    off = 6,
};

[[nodiscard]] constexpr std::string_view to_string(level value) noexcept {
    switch (value) {
    case level::trace: return "trace";
    case level::debug: return "debug";
    case level::info: return "info";
    case level::warning: return "warning";
    case level::error: return "error";
    case level::critical: return "critical";
    default: return "off";
    }
}

/// A colourised stdout sink, built inside the DLL.
struct console_sink {
    bool color = false;
};

/// A rotating file sink, built inside the DLL. The parent directory is created.
struct file_sink {
    std::filesystem::path path{"helios.log"};

    /// Rotate once the file would exceed this size; 0 disables rotation.
    std::uint64_t max_bytes{};

    /// Backups kept as `<path>.1` ... `<path>.<max_files-1>`; 1 means no backup.
    std::uint32_t max_files{1};
};

/// A destination implemented by the caller. It must outlive the chain.
class HELIOSVIEW2_EXPORT sink {
public:
    sink() = default;
    sink(const sink&) = delete;
    sink& operator=(const sink&) = delete;

    /// Called on the chain's worker thread; must not throw.
    virtual void write(std::string_view text) noexcept = 0;
    virtual void flush() noexcept = 0;

    virtual ~sink();
};

/// A sink the caller implements. The chain keeps it alive while it exists.
struct external_sink {
    std::shared_ptr<sink> target;
};

struct pattern_formatter {
    std::string pattern;
};

struct json_formatter {};

/// One chain: a formatter plus a destination, plus its own level filter.
struct chain_options {
    level minimum_level{level::trace};
    std::variant<pattern_formatter, json_formatter> formatter;
    std::variant<console_sink, file_sink, external_sink> sink;
};

using chain_id = std::uint64_t;

/// Registers a chain and starts its worker thread; throws helios::error when the
/// pattern or the sink is unusable.
[[nodiscard]] HELIOSVIEW2_EXPORT chain_id add_chain(const chain_options& options);

/// Removes a chain, flushing it and joining its thread before returning.
HELIOSVIEW2_EXPORT void remove_chain(chain_id id);

HELIOSVIEW2_EXPORT void clear_chains();

/// Global level: calls below it are dropped before formatting.
HELIOSVIEW2_EXPORT void set_level(level value) noexcept;
[[nodiscard]] HELIOSVIEW2_EXPORT level current_level() noexcept;

/// Blocks until every chain has written what was submitted before the call.
HELIOSVIEW2_EXPORT void flush();

/// Flushes and drops all chains. Call this (or flush()) before main returns: the
/// module deliberately never joins its worker threads during static destruction,
/// because the C runtime has already started tearing stdio down by then.
HELIOSVIEW2_EXPORT void shutdown();

/// The write path every helper below funnels into. The text is only borrowed for
/// the duration of the call; the service copies what it keeps.
HELIOSVIEW2_EXPORT void submit(level severity,
                               std::string_view group,
                               std::string_view message,
                               std::string_view file,
                               std::string_view function,
                               std::uint32_t line,
                               std::uint32_t column,
                               std::int64_t timestamp_ns,
                               std::uint64_t thread_id);

template <class... Args>
void write(level severity,
           std::string_view group,
           helios::detail::location_fmt_string<Args...> pattern,
           Args&&... args) {
    if (static_cast<std::int32_t>(severity) < static_cast<std::int32_t>(current_level())) return;

    submit(severity, group, pattern.format(std::forward<Args>(args)...),
           helios::detail::relative_path(pattern.location.file_name()),
           helios::detail::short_function_name(pattern.location.function_name()),
           pattern.location.line(), pattern.location.column(),
           std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count(),
           static_cast<std::uint64_t>(
                   std::hash<std::thread::id>{}(std::this_thread::get_id())));
}

template <class... Args>
void trace(helios::detail::location_fmt_string<Args...> pattern, Args&&... args) {
    write(level::trace, {}, pattern, std::forward<Args>(args)...);
}

template <class... Args>
void debug(helios::detail::location_fmt_string<Args...> pattern, Args&&... args) {
    write(level::debug, {}, pattern, std::forward<Args>(args)...);
}

template <class... Args>
void info(helios::detail::location_fmt_string<Args...> pattern, Args&&... args) {
    write(level::info, {}, pattern, std::forward<Args>(args)...);
}

template <class... Args>
void warning(helios::detail::location_fmt_string<Args...> pattern, Args&&... args) {
    write(level::warning, {}, pattern, std::forward<Args>(args)...);
}

template <class... Args>
void error(helios::detail::location_fmt_string<Args...> pattern, Args&&... args) {
    write(level::error, {}, pattern, std::forward<Args>(args)...);
}

template <class... Args>
void critical(helios::detail::location_fmt_string<Args...> pattern, Args&&... args) {
    write(level::critical, {}, pattern, std::forward<Args>(args)...);
}

/// A named category that prefixes every call without touching the global
/// functions: `constexpr auto net = log::grp("net"); net.info("...")`.
struct group_ref {
    std::string_view name;

    template <class... Args>
    void trace(helios::detail::location_fmt_string<Args...> pattern, Args&&... args) const {
        write(level::trace, name, pattern, std::forward<Args>(args)...);
    }

    template <class... Args>
    void debug(helios::detail::location_fmt_string<Args...> pattern, Args&&... args) const {
        write(level::debug, name, pattern, std::forward<Args>(args)...);
    }

    template <class... Args>
    void info(helios::detail::location_fmt_string<Args...> pattern, Args&&... args) const {
        write(level::info, name, pattern, std::forward<Args>(args)...);
    }

    template <class... Args>
    void warning(helios::detail::location_fmt_string<Args...> pattern, Args&&... args) const {
        write(level::warning, name, pattern, std::forward<Args>(args)...);
    }

    template <class... Args>
    void error(helios::detail::location_fmt_string<Args...> pattern, Args&&... args) const {
        write(level::error, name, pattern, std::forward<Args>(args)...);
    }

    template <class... Args>
    void critical(helios::detail::location_fmt_string<Args...> pattern, Args&&... args) const {
        write(level::critical, name, pattern, std::forward<Args>(args)...);
    }
};

inline constexpr group_ref grp(std::string_view name) noexcept { return group_ref{name}; }

inline constexpr group_ref core_log = grp("core");

}  // namespace helios::log
