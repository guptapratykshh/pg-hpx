//  Copyright (c) 2025 Shivansh Singh
//  Copyright (c) 2025 Hartmut Kaiser
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

/// \file future_sender.hpp
/// \brief Adapts an hpx::future<T> as a P2300 stdexec sender.
///
/// Usage:
/// \code
///   hpx::future<int> f = hpx::async([]{ return 42; });
///   auto result = hpx::execution::experimental::future_sender<hpx::future<int>>{std::move(f)}
///       | hpx::execution::experimental::then([](int x){ return x * 2; })
///       | hpx::execution::experimental::sync_wait();
/// \endcode

#pragma once

#include <hpx/config.hpp>
#include <hpx/futures/future.hpp>
#include <hpx/futures/traits/future_traits.hpp>
#include <hpx/modules/errors.hpp>
#include <hpx/modules/execution_base.hpp>

namespace hpx::execution::experimental {
    struct as_sender_t;
}

#include <exception>
#include <type_traits>
#include <utility>

namespace hpx::execution::experimental {

    HPX_CXX_CORE_EXPORT template <typename Receiver, typename Future>
    class future_sender_operation_state
    {
    public:
        using receiver_type = std::decay_t<Receiver>;
        using future_type = std::decay_t<Future>;
        using result_type =
            typename hpx::traits::future_traits<future_type>::type;

        template <typename Receiver_>
        future_sender_operation_state(Receiver_&& r, future_type f)
          : receiver_(HPX_FORWARD(Receiver_, r))
          , future_(HPX_MOVE(f))
        {
        }

        future_sender_operation_state(
            future_sender_operation_state const&) = delete;
        future_sender_operation_state& operator=(
            future_sender_operation_state const&) = delete;
        future_sender_operation_state(future_sender_operation_state&&) = delete;
        future_sender_operation_state& operator=(
            future_sender_operation_state&&) = delete;

        void start() & noexcept
        {
            start_helper();
        }

    private:
        void start_helper() & noexcept
        {
            hpx::detail::try_catch_exception_ptr(
                [&]() {
                    future_.then([this](future_type f) {
                        hpx::detail::try_catch_exception_ptr(
                            [&]() {
                                if constexpr (std::is_void_v<result_type>)
                                {
                                    f.get();
                                    hpx::execution::experimental::set_value(
                                        HPX_MOVE(receiver_));
                                }
                                else
                                {
                                    hpx::execution::experimental::set_value(
                                        HPX_MOVE(receiver_), f.get());
                                }
                            },
                            [&](std::exception_ptr ep) {
                                hpx::execution::experimental::set_error(
                                    HPX_MOVE(receiver_), HPX_MOVE(ep));
                            });
                    });
                },
                [&](std::exception_ptr ep) {
                    hpx::execution::experimental::set_error(
                        HPX_MOVE(receiver_), HPX_MOVE(ep));
                });
        }

        receiver_type receiver_;
        future_type future_;
    };

    HPX_CXX_CORE_EXPORT template <typename Future>
    struct future_sender
    {
        using sender_concept = hpx::execution::experimental::sender_t;
        using future_type = std::decay_t<Future>;
        using result_type =
            typename hpx::traits::future_traits<future_type>::type;

        using sent_type = std::conditional_t<
            std::is_same_v<future_type, hpx::shared_future<result_type>>,
            result_type const&, result_type>;

        using completion_signatures = std::conditional_t<
            std::is_void_v<result_type>,
            hpx::execution::experimental::completion_signatures<
                hpx::execution::experimental::set_value_t(),
                hpx::execution::experimental::set_error_t(std::exception_ptr),
                hpx::execution::experimental::set_stopped_t()>,
            hpx::execution::experimental::completion_signatures<
                hpx::execution::experimental::set_value_t(sent_type),
                hpx::execution::experimental::set_error_t(std::exception_ptr),
                hpx::execution::experimental::set_stopped_t()>>;

        explicit future_sender(future_type f) noexcept
          : future_(HPX_MOVE(f))
        {
        }

        future_sender(future_sender&&) = default;
        future_sender& operator=(future_sender&&) = default;
        future_sender(future_sender const&) = default;
        future_sender& operator=(future_sender const&) = default;

        template <typename... Env>
        constexpr auto get_completion_signatures(Env&&...) const noexcept
            -> completion_signatures
        {
            return {};
        }

        template <typename Receiver>
        auto connect(Receiver&& r) &&
        {
            return future_sender_operation_state<Receiver, future_type>{
                HPX_FORWARD(Receiver, r), HPX_MOVE(future_)};
        }

        template <typename Receiver>
        auto connect(Receiver&& r) &
        {
            return future_sender_operation_state<Receiver, future_type>{
                HPX_FORWARD(Receiver, r), future_};
        }

    private:
        future_type future_;
    };

    template <typename Future,
        typename =
            std::enable_if_t<hpx::traits::is_future_v<std::decay_t<Future>>>>
    future_sender<std::decay_t<Future>> tag_invoke(
        hpx::execution::experimental::as_sender_t const&, Future&& f)
    {
        return future_sender<std::decay_t<Future>>{HPX_FORWARD(Future, f)};
    }

}    // namespace hpx::execution::experimental
