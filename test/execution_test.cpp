#include <HeliosView2/Execution.h>

#include <doctest/doctest.h>

#include <utility>

namespace ex = helios::exec;

TEST_CASE("execution adapter composes senders") {
    auto sender = ex::just(20) | ex::then([](int value) noexcept { return value + 22; });
    auto [result] = helios::sync_wait(std::move(sender)).value();
    CHECK(result == 42);
}

TEST_CASE("execution adapter exposes when_all") {
    auto sender = ex::when_all(ex::just(1), ex::just(2))
                | ex::then([](int a, int b) noexcept { return a + b; });
    auto [result] = helios::sync_wait(std::move(sender)).value();
    CHECK(result == 3);
}

#if HELIOS_HAS_TASK
TEST_CASE("execution adapter exposes task") {
    auto task = []() -> helios::task<int> { co_return 42; }();
    auto [result] = helios::sync_wait(std::move(task)).value();
    CHECK(result == 42);
}
#endif

#if HELIOS_HAS_COUNTING_SCOPE
TEST_CASE("execution adapter exposes counting_scope and spawn") {
    helios::counting_scope scope;
    int value = 0;

    helios::spawn(ex::just(7)
                               | ex::then([&value](int v) noexcept { value = v; }),
                       scope.get_token());
    helios::sync_wait(scope.join());

    CHECK(value == 7);
}
#endif
