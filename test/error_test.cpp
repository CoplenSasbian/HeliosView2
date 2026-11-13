#include <HeliosView2/App.h>
#include <HeliosView2/Error.h>
#include <HeliosView2/Log.h>
#include <HeliosView2/Window.h>

#include "test_support.h"

#include <doctest/doctest.h>

#include <glaze/glaze.hpp>

#include <cstdint>
#include <format>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace {

// Parsed back from to_json(), so the writer is checked against a real parser.
struct parsed_error {
    std::int32_t code{};
    std::string name;
    std::string message;
    std::string context;
    std::string file;
    std::uint32_t line{};
    std::uint32_t column{};
    std::string function;
};

}  // namespace

template <>
struct glz::meta<parsed_error> {
    using T = parsed_error;
    static constexpr auto value = glz::object(
            "code", &T::code,
            "name", &T::name,
            "message", &T::message,
            "context", &T::context,
            "file", &T::file,
            "line", &T::line,
            "column", &T::column,
            "function", &T::function);
};

TEST_CASE("status codes have stable names") {
    CHECK(helios::to_string(helios::ok) == "ok");
    CHECK(helios::to_string(helios::invalid_argument) == "invalid_argument");
    CHECK(helios::to_string(helios::wrong_thread) == "wrong_thread");
    CHECK(helios::to_string(helios::platform_failure) == "platform_failure");
    CHECK(helios::to_string(helios::unhandled_exception) == "unhandled_exception");
    CHECK(helios::to_string(-999) == "unknown");

    CHECK(helios::succeeded(helios::ok));
    CHECK(helios::failed(helios::invalid_state));
}

TEST_CASE("make_error records the message, context and call site") {
    const auto failure = helios::make_error(helios::invalid_state, "starting the loop",
                                            "already running {} times", 3);

    REQUIRE(failure.valid());
    CHECK(failure.code() == helios::invalid_state);
    CHECK(failure.frame().context == "starting the loop");
    CHECK(failure.frame().message == "already running 3 times");
    CHECK(failure.frame().file == "test/error_test.cpp");
    CHECK(failure.frame().line > 0);
    CHECK_FALSE(failure.frame().function.empty());

    const auto text = failure.to_string();
    CHECK(text.starts_with("starting the loop: already running 3 times"));
    CHECK(text.find("test/error_test.cpp:") != std::string::npos);
}

TEST_CASE("a platform failure carries its system description as text") {
    const auto failure = helios::make_error_text(
            helios::platform_failure, "opening the log",
            "CreateFileW failed (error 5: Access is denied.)",
            std::source_location::current());

    CHECK(failure.code() == helios::platform_failure);
    CHECK(failure.frame().message.find("(error 5") != std::string::npos);
    CHECK(failure.to_string().starts_with("opening the log: CreateFileW failed (error 5"));
}

TEST_CASE("with_context keeps the inner failure as the cause") {
    const auto inner = helios::make_error(helios::platform_failure, "opening the file",
                                          "CreateFileW failed (error 5)");
    const auto outer = helios::with_context(inner, helios::not_found, "loading the config",
                                            "{} is unreadable", "config.json");

    REQUIRE(outer.frames().size() == 2);
    CHECK(outer.code() == helios::not_found);
    CHECK(outer.frames().front().message == "config.json is unreadable");

    const auto text = outer.to_string();
    CHECK(text.find("loading the config: config.json is unreadable") != std::string::npos);
    CHECK(text.find("<- caused by: opening the file: CreateFileW failed (error 5)")
          != std::string::npos);
}

TEST_CASE("the formatter renders text and json") {
    const auto failure = helios::make_error(helios::invalid_argument, "creating a window",
                                            "options is null");

    CHECK(std::format("{}", failure) == failure.to_string());

    const auto json = std::format("{:json}", failure);
    CHECK(json.find("\"name\":\"invalid_argument\"") != std::string::npos);
    CHECK(json.find("\"code\":-2") != std::string::npos);
    CHECK(json.find("\"context\":\"creating a window\"") != std::string::npos);
    CHECK(json.find("\"file\":\"test/error_test.cpp\"") != std::string::npos);

    parsed_error parsed;
    const auto error = glz::read<glz::opts{.error_on_unknown_keys = false}>(parsed, json);
    REQUIRE_FALSE(error);
    CHECK(parsed.code == helios::invalid_argument);
    CHECK(parsed.name == "invalid_argument");
    CHECK(parsed.message == "options is null");
    CHECK(parsed.context == "creating a window");
    CHECK(parsed.file == "test/error_test.cpp");
    CHECK(parsed.line > 0);
}

TEST_CASE("a cause chain becomes nested json") {
    const auto inner = helios::make_error(helios::platform_failure, "opening the file",
                                          "CreateFileW failed (error 87)");
    const auto outer = helios::with_context(inner, helios::not_found, "loading the config",
                                            "config.json");
    const auto json = helios::to_json(outer);

    CHECK(json.find("\"cause\":[{") != std::string::npos);
    CHECK(json.find("(error 87)") != std::string::npos);
    CHECK(helios::to_json(helios::error{}) == "null");
}

TEST_CASE("an empty error is inert") {
    const helios::error nothing;
    CHECK_FALSE(nothing.valid());
    CHECK(nothing.code() == helios::ok);
    CHECK(nothing.to_string() == "no error");
    CHECK(std::format("{}", nothing) == "no error");
}

TEST_CASE("error_for names a bare status") {
    const auto generic = helios::error_for(helios::not_found);
    REQUIRE(generic.valid());
    CHECK(generic.code() == helios::not_found);
    CHECK(generic.frame().message.find("not_found") != std::string::npos);
    CHECK(std::string_view{generic.what()} == generic.to_string());
}

#if defined(_WIN32)
TEST_CASE("a failing call throws its detail") {
    std::unique_ptr<helios::app> application = test_support::app();

    helios::window_options options{.title = "main"};

    std::thread worker([&] {
        // Windows must be created on the UI thread.
        try {
            (void) application->create_window(options);
            FAIL("create_window should have thrown");
        } catch (const helios::error& failure) {
            CHECK(failure.code() == helios::wrong_thread);
            CHECK(failure.frame().context == "creating a window");
            CHECK_FALSE(failure.to_string().empty());
        }
    });
    worker.join();

    application->quit();
    std::int32_t exit_code = -1;
    CHECK_NOTHROW(application->run(exit_code));
}

TEST_CASE("a log chain reports a malformed pattern") {
    try {
        (void) helios::log::add_chain({
                .minimum_level = helios::log::level::trace,
                .formatter = helios::log::pattern_formatter{"{z}"},
                .sink = helios::log::console_sink{},
        });
        FAIL("a malformed pattern should have thrown");
    } catch (const helios::error& failure) {
        CHECK(failure.code() == helios::invalid_argument);
        CHECK(failure.frame().context == "adding a log chain");
        CHECK(failure.frame().message.find("unknown field") != std::string::npos);
    }
}
#endif
