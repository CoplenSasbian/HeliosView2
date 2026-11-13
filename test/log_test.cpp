// Logging is tested through its public API only: the formatter and the built-in
// sinks live inside the DLL, and a test sink implements the same helios::log::sink
// interface a caller would.

#include <HeliosView2/Error.h>
#include <HeliosView2/Log.h>

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

class memory_sink final : public helios::log::sink {
public:
    void write(std::string_view text) noexcept override {
        try {
            std::lock_guard lock(m_mutex);
            m_lines.emplace_back(text);
        } catch (...) {
        }
    }

    void flush() noexcept override {}

    [[nodiscard]] std::vector<std::string> lines() const {
        std::lock_guard lock(m_mutex);
        return m_lines;
    }

private:
    mutable std::mutex m_mutex;
    std::vector<std::string> m_lines;
};

// Every test starts and ends with a clean global state: the service is
// process-wide.
struct log_reset {
    log_reset() { reset(); }
    ~log_reset() { reset(); }

    static void reset() {
        helios::log::clear_chains();
        helios::log::set_level(helios::log::level::trace);
    }
};

using memory_sink_ptr = std::shared_ptr<memory_sink>;

helios::log::chain_id add_memory_chain(const memory_sink_ptr& sink,
                                       std::string pattern,
                                       helios::log::level minimum = helios::log::level::trace) {
    return helios::log::add_chain({
            .minimum_level = minimum,
            .formatter = helios::log::pattern_formatter{std::move(pattern)},
            .sink = helios::log::external_sink{sink},
    });
}

}  // namespace

TEST_CASE("levels are ordered and named") {
    CHECK(helios::log::to_string(helios::log::level::trace) == "trace");
    CHECK(helios::log::to_string(helios::log::level::warning) == "warning");
    CHECK(helios::log::to_string(helios::log::level::critical) == "critical");
    CHECK(helios::log::to_string(helios::log::level::off) == "off");

    CHECK(static_cast<int>(helios::log::level::trace)
          < static_cast<int>(helios::log::level::error));
}

TEST_CASE("records flow through a chain and keep their fields") {
    log_reset reset;

    const auto sink = std::make_shared<memory_sink>();
    const auto id = add_memory_chain(sink, "[{l}] {g}: {m}");

    helios::log::info("value {}", 42);
    helios::log::trace("plain");
    helios::log::grp("net").warning("careful {}", "now");
    helios::log::core_log.debug("core");
    helios::log::flush();

    const auto lines = sink->lines();
    REQUIRE(lines.size() == 4);
    CHECK(lines[0] == "[info] : value 42");
    CHECK(lines[1] == "[trace] : plain");
    CHECK(lines[2] == "[warning] net: careful now");
    CHECK(lines[3] == "[debug] core: core");

    helios::log::remove_chain(id);
}

TEST_CASE("the pattern renders the call site") {
    log_reset reset;

    const auto sink = std::make_shared<memory_sink>();
    add_memory_chain(sink, "{f}:{n} {u}");

    helios::log::error("boom");
    helios::log::flush();

    const auto lines = sink->lines();
    REQUIRE(lines.size() == 1);
    CHECK(lines[0].starts_with("test/log_test.cpp:"));

    // The function name is compiler specific, so only its presence is checked.
    const auto separator = lines[0].find(' ');
    REQUIRE(separator != std::string::npos);
    CHECK(separator + 1 < lines[0].size());
}

TEST_CASE("the json formatter writes one object per line") {
    log_reset reset;

    const auto sink = std::make_shared<memory_sink>();
    const helios::log::chain_id json_chain = helios::log::add_chain({
            .minimum_level = helios::log::level::trace,
            .formatter = helios::log::json_formatter{},
            .sink = helios::log::external_sink{sink},
    });
    REQUIRE(json_chain != 0);

    helios::log::info("quoted \"text\"");
    helios::log::flush();

    const auto lines = sink->lines();
    REQUIRE(lines.size() == 1);
    CHECK(lines[0].starts_with("{\"level\":\"info\""));
    CHECK(lines[0].find("\"message\":\"quoted \\\"text\\\"\"") != std::string::npos);
    CHECK(lines[0].find("\"file\":\"test/log_test.cpp\"") != std::string::npos);
    CHECK(lines[0].find("\"time\":\"") != std::string::npos);
}

TEST_CASE("a malformed pattern is rejected and explained") {
    log_reset reset;

    const auto sink = std::make_shared<memory_sink>();
    for (const std::string pattern : {"{z}", "{m", "a}b", "{m:>4}"}) {
        try {
            (void) helios::log::add_chain({
                    .minimum_level = helios::log::level::trace,
                    .formatter = helios::log::pattern_formatter{pattern},
                    .sink = helios::log::external_sink{sink},
            });
            FAIL("a malformed pattern should have thrown");
        } catch (const helios::error& failure) {
            CHECK(failure.code() == helios::invalid_argument);
            CHECK(failure.frame().context == "adding a log chain");
        }
    }
}

TEST_CASE("a null external sink is rejected") {
    log_reset reset;

    try {
        (void) helios::log::add_chain({
                .minimum_level = helios::log::level::trace,
                .formatter = helios::log::pattern_formatter{"{m}"},
                .sink = helios::log::external_sink{},
        });
        FAIL("a null sink should have thrown");
    } catch (const helios::error& failure) {
        CHECK(failure.code() == helios::invalid_argument);
    }
}

TEST_CASE("the global level drops calls before formatting") {
    log_reset reset;

    const auto sink = std::make_shared<memory_sink>();
    add_memory_chain(sink, "{m}");

    helios::log::set_level(helios::log::level::error);
    CHECK(helios::log::current_level() == helios::log::level::error);

    helios::log::info("dropped");
    helios::log::warning("dropped");
    helios::log::error("kept");
    helios::log::flush();

    const auto lines = sink->lines();
    REQUIRE(lines.size() == 1);
    CHECK(lines[0] == "kept");
}

TEST_CASE("a chain minimum level filters independently") {
    log_reset reset;

    const auto sink = std::make_shared<memory_sink>();
    add_memory_chain(sink, "{m}", helios::log::level::warning);

    helios::log::debug("dropped");
    helios::log::warning("kept");
    helios::log::critical("kept too");
    helios::log::flush();

    const auto lines = sink->lines();
    REQUIRE(lines.size() == 2);
    CHECK(lines[0] == "kept");
    CHECK(lines[1] == "kept too");
}

TEST_CASE("level off silences every chain") {
    log_reset reset;

    const auto sink = std::make_shared<memory_sink>();
    add_memory_chain(sink, "{m}");

    helios::log::set_level(helios::log::level::off);
    helios::log::critical("dropped");
    helios::log::flush();

    CHECK(sink->lines().empty());
}

TEST_CASE("flush waits for records submitted earlier") {
    log_reset reset;

    const auto sink = std::make_shared<memory_sink>();
    add_memory_chain(sink, "{m}");

    for (int index = 0; index < 200; ++index) helios::log::info("item {}", index);
    helios::log::flush();

    const auto lines = sink->lines();
    REQUIRE(lines.size() == 200);
    CHECK(lines.front() == "item 0");
    CHECK(lines.back() == "item 199");
}

TEST_CASE("remove_chain is idempotent") {
    log_reset reset;

    const auto sink = std::make_shared<memory_sink>();
    const auto id = add_memory_chain(sink, "{m}");

    helios::log::remove_chain(id);
    helios::log::remove_chain(id);
    helios::log::info("after removal");
    helios::log::flush();

    CHECK(sink->lines().empty());
}

TEST_CASE("file sink writes, creates parents and rotates") {
    log_reset reset;

    // The working directory is the build tree, which is always writable; the
    // process temp directory may not be.
    const auto root = std::filesystem::current_path() / "helios_log_sink_test";
    std::filesystem::remove_all(root);
    const auto path = root / "nested" / "app.log";
    const auto backup = std::filesystem::path{path.generic_string() + ".1"};

    {
        const helios::log::chain_id id = helios::log::add_chain({
                .minimum_level = helios::log::level::trace,
                .formatter = helios::log::pattern_formatter{"0123456789"},
                .sink = helios::log::file_sink{path, 30, 2},
        });
        REQUIRE(id != 0);

        for (int index = 0; index < 6; ++index) helios::log::info("line {}", index);
        helios::log::flush();
        helios::log::remove_chain(id);
    }

    REQUIRE(std::filesystem::exists(path));
    REQUIRE(std::filesystem::exists(backup));

    // Ten characters plus the newline is 11 bytes; a 30 byte cap fits two lines
    // per file, so each of the two kept files holds two lines.
    CHECK(std::filesystem::file_size(path) == 22);
    CHECK(std::filesystem::file_size(backup) == 22);

    std::filesystem::remove_all(root);
}
