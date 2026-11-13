#pragma once

// stdexec/std::execution adapter for app. The scheduler is header-only: it
// posts a completion callback through app::post, so sender/receiver types never
// cross the DLL boundary.

#include <HeliosView2/Execution.h>
#include <HeliosView2/Task.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

namespace helios::detail {

template <class Receiver>
struct ui_completion_state {
    Receiver receiver;
    std::atomic<bool> completed{false};

    explicit ui_completion_state(Receiver receiver_)
        : receiver(std::move(receiver_)) {}

    void complete_value() noexcept {
        if (!completed.exchange(true, std::memory_order_acq_rel)) {
            exec::set_value(std::move(receiver));
        }
    }

    void complete_stopped() noexcept {
        if (!completed.exchange(true, std::memory_order_acq_rel)) {
            exec::set_stopped(std::move(receiver));
        }
    }
};

// Heap-allocated task owned by the app queue. The operation state itself stays
// with stdexec; this task only shares the completion state.
template <class Receiver>
class ui_completion_task final : public abi::task {
public:
    explicit ui_completion_task(std::shared_ptr<ui_completion_state<Receiver>> state)
        : m_state(std::move(state)) {}

    ~ui_completion_task() {
        if (m_state != nullptr) m_state->complete_stopped();
    }

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
        if (m_state != nullptr) m_state->complete_value();
        return abi::ok;
    }

private:
    std::shared_ptr<ui_completion_state<Receiver>> m_state;
    std::atomic<std::uint32_t> m_refs{1};
};

}  // namespace helios::detail

namespace helios {

class ui_scheduler {
public:
    explicit ui_scheduler(app& target) noexcept
        : m_app(&target) {}

    struct sender {
#if !HELIOS_HAS_STD_EXECUTION
        using sender_concept = exec::sender_tag;
#endif
        using completion_signatures =
                exec::completion_signatures<exec::set_value_t(), exec::set_stopped_t()>;

        app* target;

        template <class Receiver>
        struct operation {
#if !HELIOS_HAS_STD_EXECUTION
            using operation_state_concept = exec::operation_state_tag;
#endif

            operation(app* target_app,
                      std::shared_ptr<detail::ui_completion_state<Receiver>> state)
                : m_app(target_app), m_state(std::move(state)) {}

            operation(const operation&) = delete;
            operation& operator=(const operation&) = delete;

            void start() & noexcept {
                detail::ui_completion_task<Receiver>* item = nullptr;
                try {
                    item = new detail::ui_completion_task<Receiver>(m_state);
                } catch (...) {
                    m_state->complete_stopped();
                    return;
                }

                // app::post adds its own reference; release our initial one.
                (void) m_app->post(item);
                item->release();
            }

        private:
            app* m_app;
            std::shared_ptr<detail::ui_completion_state<Receiver>> m_state;
        };

        template <class Receiver>
        auto connect(Receiver receiver) const -> operation<std::decay_t<Receiver>> {
            using receiver_type = std::decay_t<Receiver>;
            auto state = std::make_shared<detail::ui_completion_state<receiver_type>>(
                    std::move(receiver));
            return operation<receiver_type>{target, std::move(state)};
        }
    };

#if !HELIOS_HAS_STD_EXECUTION
    using scheduler_concept = exec::scheduler_tag;
#endif

    sender schedule() const noexcept { return sender{m_app}; }

    bool operator==(const ui_scheduler&) const noexcept = default;

private:
    app* m_app;
};

}  // namespace helios
