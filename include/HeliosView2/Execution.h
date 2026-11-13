#pragma once

// Execution adapter: code targets std::execution (C++26, P2300). Until the
// standard library implements it, NVIDIA stdexec provides the same vocabulary.
//
// std::execution cannot be namespace-aliased to stdexec: <execution> already
// declares that namespace for the parallel-algorithm policies, and adding
// names to namespace std is undefined behaviour. Call sites therefore use
// helios::exec (or helios::sync_wait / task / counting_scope);
// when the standard library implements P2300, only the alias below changes.
// HELIOS_EXECUTION_IMPL selects the backend in CMake; the macros below let
// call sites adapt when a standard library implements only part of P2300.

#include <version>

#if defined(HELIOS_USE_STD_EXECUTION)
#    define HELIOS_HAS_STD_EXECUTION HELIOS_USE_STD_EXECUTION
#elif defined(__cpp_lib_senders) && __cpp_lib_senders >= 202406L
#    define HELIOS_HAS_STD_EXECUTION 1
#else
#    define HELIOS_HAS_STD_EXECUTION 0
#endif

#if HELIOS_HAS_STD_EXECUTION
#    include <execution>
#    include <thread>  // std::this_thread::sync_wait
#else
#    include <stdexec/execution.hpp>
#endif

#include <utility>

namespace helios {

#if HELIOS_HAS_STD_EXECUTION
namespace exec = std::execution;
#else
namespace exec = stdexec;
#endif

// Optional C++26 facilities; stdexec implements all of them today.
#if HELIOS_HAS_STD_EXECUTION
#    if defined(__cpp_lib_task) && __cpp_lib_task >= 202506L
#        define HELIOS_HAS_TASK 1
#    else
#        define HELIOS_HAS_TASK 0
#    endif
#    if defined(__cpp_lib_counting_scope) && __cpp_lib_counting_scope >= 202506L
#        define HELIOS_HAS_COUNTING_SCOPE 1
#    else
#        define HELIOS_HAS_COUNTING_SCOPE 0
#    endif
#else
#    define HELIOS_HAS_TASK 1
#    define HELIOS_HAS_COUNTING_SCOPE 1
#endif

#if HELIOS_HAS_TASK
using exec::task;
#endif

#if HELIOS_HAS_COUNTING_SCOPE
using exec::counting_scope;
using exec::spawn;
#endif

// std::execution::sync_wait lives in std::this_thread; stdexec keeps it in
// stdexec. Both return an optional tuple.
template <class Sender>
auto sync_wait(Sender&& sender) {
#if HELIOS_HAS_STD_EXECUTION
    return std::this_thread::sync_wait(std::forward<Sender>(sender));
#else
    return stdexec::sync_wait(std::forward<Sender>(sender));
#endif
}

}  // namespace helios
