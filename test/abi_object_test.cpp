#include <HeliosView2/abi/object.h>

#include <doctest/doctest.h>

#include <atomic>
#include <memory>

namespace abi = helios::abi;

namespace {

inline constexpr abi::guid iid_test = abi::make_guid("helios.test.object/1");

struct itest : abi::unknown {
    static constexpr abi::guid iid = iid_test;

    virtual int value() const noexcept = 0;
};

class test_object final : public itest {
public:
    test_object() noexcept { ++alive; }
    ~test_object() { --alive; }

    std::uint32_t add_ref() noexcept override { return ++refs; }

    std::uint32_t release() noexcept override {
        const auto remaining = --refs;
        if (remaining == 0) delete this;
        return remaining;
    }

    abi::status query_interface(const abi::guid& id, void** out) noexcept override {
        if (out == nullptr) return abi::invalid_argument;
        *out = nullptr;

        if (id == iid) {
            *out = static_cast<itest*>(this);
            add_ref();
            return abi::ok;
        }
        if (id == abi::unknown_iid) {
            *out = static_cast<abi::unknown*>(this);
            add_ref();
            return abi::ok;
        }
        return abi::no_interface;
    }

    int value() const noexcept override { return 42; }

    static inline int alive = 0;

private:
    std::atomic<std::uint32_t> refs{1};
};

void create(itest** out) {
    *out = new test_object();
}

}  // namespace

TEST_CASE("query_interface returns the requested interface") {
    test_object object;
    void* raw = nullptr;

    REQUIRE(abi::succeeded(object.query_interface(iid_test, &raw)));
    REQUIRE(raw != nullptr);

    auto* value = static_cast<itest*>(raw);
    CHECK(value->value() == 42);
    value->release();
    CHECK(test_object::alive == 1);
}

TEST_CASE("query_interface rejects unknown identifiers") {
    test_object object;
    void* raw = reinterpret_cast<void*>(0x1);

    CHECK(object.query_interface(abi::make_guid("helios.test.missing/1"), &raw)
          == abi::no_interface);
    CHECK(raw == nullptr);
}

TEST_CASE("the base interface identifier returns the base pointer") {
    test_object object;
    void* raw = nullptr;

    REQUIRE(abi::succeeded(object.query_interface(abi::unknown_iid, &raw)));
    REQUIRE(raw != nullptr);

    auto* base = static_cast<abi::unknown*>(raw);
    CHECK(base->add_ref() == 3);
    CHECK(base->release() == 2);
    CHECK(base->release() == 1);
    CHECK(test_object::alive == 1);
}

TEST_CASE("out_ptr takes ownership through release") {
    const int before = test_object::alive;

    {
        abi::ptr<itest> pointer;
        create(std::out_ptr(pointer));

        REQUIRE(pointer != nullptr);
        CHECK(pointer->value() == 42);
        CHECK(test_object::alive == before + 1);
    }

    CHECK(test_object::alive == before);
}

TEST_CASE("query wraps a raw interface in a smart pointer") {
    test_object object;

    auto pointer = abi::query<itest>(object);
    REQUIRE(pointer != nullptr);
    CHECK(pointer->value() == 42);
}
