#pragma once

// stdexec/std::execution adapter for app: the scheduler posts its completion
// through app::post, so sender and receiver types never cross the DLL boundary -
// only a std::function does.

#include <HeliosView2/App.h>
#include <HeliosView2/Execution.h>

#include <atomic>
#include <memory>
#include <type_traits>
#include <utility>

namespace helios::detail {

/// Shared once-only completion, heap allocated because the posted closure may
/// outlive the operation state.
template <class Receiver>
struct ui_completion final {
    Receiver receiver;
    std::atomic<bool> done{false};

    explicit ui_completion(Receiver value) : receiver(std::move(value)) {}

    void complete_value() noexcept {
        if (!done.exchange(true, std::memory_order_acq_rel)) {
            exec::set_value(std::move(receiver));
        }
    }

    void complete_stopped() noexcept {
        if (!done.exchange(true, std::memory_order_acq_rel)) {
            exec::set_stopped(std::move(receiver));
        }
    }
};

}  // namespace helios::detail

namespace helios {

class ui_scheduler {
public:
    explicit ui_scheduler(app& target) noexcept : m_app(&target) {}

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
            operation(app* app_target, std::shared_ptr<detail::ui_completion<Receiver>> state)
                : m_app(app_target), m_state(std::move(state)) {}

            operation(const operation&) = delete;
            operation& operator=(const operation&) = delete;

            void start() & noexcept {
                auto state = m_state;
                try {
                    m_app->post(
                            [state] { state->complete_value(); },
                            [state](error) { state->complete_stopped(); });
                } catch (...) {
                    state->complete_stopped();
                }
            }

        private:
            app* m_app;
            std::shared_ptr<detail::ui_completion<Receiver>> m_state;
        };

        template <class Receiver>
        auto connect(Receiver receiver) const -> operation<std::decay_t<Receiver>> {
            using receiver_type = std::decay_t<Receiver>;
            auto state = std::make_shared<detail::ui_completion<receiver_type>>(
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
