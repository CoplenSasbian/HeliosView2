#pragma once

// Cross-DLL task: an ABI callback interface. The caller owns the
// implementation (and its std::function); the DLL only sees abi::task* and
// invokes run() on the UI thread.
//
// This is helios::abi::task, not helios::task: the latter is the stdexec /
// std::execution coroutine task exposed by Execution.h.

#include <HeliosView2/App.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <utility>

namespace helios::abi {

class task : public unknown {
public:
    static constexpr guid iid = make_guid("helios.abi.task/1");

    virtual status run() noexcept = 0;

protected:
    ~task() = default;
};

}  // namespace helios::abi

namespace helios::detail {

class function_task final : public abi::task {
public:
    explicit function_task(std::function<void()> work)
        : m_work(std::move(work)) {}

    std::uint32_t add_ref() noexcept override { return ++m_refs; }

    std::uint32_t release() noexcept override {
        const auto remaining = --m_refs;
        if (remaining == 0) delete this;
        return remaining;
    }

    abi::status query_interface(const abi::guid& id, void** out) noexcept override {
        if (out == nullptr) return abi::invalid_argument;
        *out = nullptr;

        if (id == abi::task::iid) {
            *out = static_cast<abi::task*>(this);
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

    abi::status run() noexcept override {
        try {
            if (m_work) m_work();
            return abi::ok;
        } catch (...) {
            return abi::unsupported;
        }
    }

private:
    std::function<void()> m_work;
    std::atomic<std::uint32_t> m_refs{1};
};

}  // namespace helios::detail

namespace helios {

// Creates a task in the caller's module. The app adds its own reference while
// queued; the wrapper is released when this function returns.
inline abi::status post(app& application, std::function<void()> work) {
    if (!work) return abi::invalid_argument;

    try {
        abi::ptr<abi::task> item(new detail::function_task(std::move(work)));
        return application.post(item.get());
    } catch (...) {
        return abi::out_of_memory;
    }
}

}  // namespace helios
