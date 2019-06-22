#pragma once

#include <queue>
#include <set>
#include <mutex>
#include <pqcpp/connection.hpp>
#include <pqcpp/error.hpp>
#include <pqcpp/logger.hpp>

namespace pqcpp {

	namespace detail {
		template <typename Executor>
		inline awaitable<void> delay(const Executor& executor, std::chrono::nanoseconds duration) {
			boost::asio::steady_timer timer(executor, duration);
			co_await timer.async_wait(use_awaitable);
		}
	}

    struct connection_pool_option {
        int min_size = 3;
        int max_size = -1;
    };

    class connection_pool: public std::enable_shared_from_this<connection_pool> {
        friend class connection_holder;

		struct conn_ptr_deleter {
			std::weak_ptr<connection_pool> _pool;

			conn_ptr_deleter(std::weak_ptr<connection_pool> pool)
				:_pool(pool)
			{
				logger()->trace("create conn_ptr_deleter");
			}

			void operator()(connection* conn) {
				logger()->trace("call conn_ptr_deleter");
				auto pool = _pool.lock();
				if (pool && conn->is_ready()) {
					logger()->trace("conn {} is ready, return conn to pool", conn->id());
					pool->on_conn_ready(conn_ptr_inner(conn));
				}
				else {
					logger()->warn("connection not ready, delete conn and notify pool");
					delete conn;
					if (pool) {
						pool->on_conn_lost();
					}
				}
			}
		};

		friend struct conn_ptr_deleter;

		using conn_ptr_inner = std::unique_ptr<connection>;
    public:
        using conn_ptr = std::shared_ptr<connection>;
        using get_handler = std::function<void(error_code, conn_ptr)>;

        static std::shared_ptr<connection_pool> make(boost::asio::io_context& io, const std::string &conn_str, int min = 3, int max = -1) {
            std::shared_ptr<connection_pool> pool(new connection_pool(io, conn_str, min, max));
            pool->init();
            return pool;
        }

        connection_pool(const connection_pool&) = delete;
        connection_pool(connection_pool&&) = delete;
        connection_pool& operator=(const connection_pool&) = delete;
        connection_pool& operator=(connection_pool&&) = delete;

        template <typename CompletionToken>
        auto get(CompletionToken&& token) {
			return boost::asio::async_initiate<
				CompletionToken,
				void(boost::system::error_code, conn_ptr)
			>(
				[this](auto handler) mutable {
					boost::asio::post(m_strand, [
						handler = std::move(handler), this, self = shared_from_this()
					]() mutable {
						if (!m_conns.empty()) {
							auto it = m_conns.begin();
							auto conn = make_conn_ptr(std::move(it->second));
							m_conns.erase(it);
							handler(error_code{}, conn);
						}
						else {
							this->enqueue_get_handler(std::move(handler));
							if (
								(m_filling && m_pendings.size() <= (m_max - m_min))
								|| m_conn_count < m_max
							) {
								co_spawn(m_strand,[this, self = shared_from_this()]() {
									return create_conn();
								}, detached);
							}
						}
					});
				},
				token
			);
        }

		auto& get_executor() {
			return m_io;
		}

    private:
        connection_pool(boost::asio::io_context& io, const std::string& conn_str, int min, int max)
            :m_io(io), m_conn_str(conn_str), m_min(min), m_max(max)
        {
			m_fill_timer.expires_at(std::chrono::steady_clock::time_point::max());
		}

		void start_fill_conns() {
			co_spawn(m_strand, [this, self = shared_from_this()]() -> awaitable<void> {
				while (true) {
					logger()->trace("start fill connection, m_conn_count={}, m_min={}", m_conn_count, m_min);
					try {
						while (m_conn_count < m_min) {
							co_await create_conn();
						}
						boost::system::error_code ec;
						co_await m_fill_timer.async_wait(redirect_error(use_awaitable, ec));
						continue;
					}
					catch (...) {}
					co_await detail::delay(m_strand, std::chrono::seconds(3));
				}
			}, detached);
		}

        void init() {
			start_fill_conns();
        }

		template <typename Handler>
		void enqueue_get_handler(Handler&& handler) {
			struct completion {
				std::shared_ptr<std::decay_t<Handler>> ptr_;
				completion(Handler&& h)
					:ptr_(std::make_shared<std::decay_t<Handler>>(std::forward<Handler>(h)))
				{}
				void operator()(boost::system::error_code ec, conn_ptr conn) {
					(*ptr_)(ec, conn);
				}
			};
			logger()->trace("enqueue_get_handler");
			this->m_pendings.emplace(completion(std::forward<Handler>(handler)));
		}

		auto make_conn_ptr(conn_ptr_inner&& conn) {
			return conn_ptr(
				conn.release(),
				conn_ptr_deleter(weak_from_this())
			);
		}

		void on_conn_ready(conn_ptr_inner conn) {
			boost::asio::post(m_strand, [
				conn = std::move(conn), this, self = shared_from_this()
			]() mutable {
				auto id = conn->id();
				if (!m_pendings.empty()) {
					auto handler = std::move(m_pendings.front());
					m_pendings.pop();
					handler({}, make_conn_ptr(std::move(conn)));
				}
				else {
				   if (m_conns.size() < m_min) {
					   m_conns.insert(std::make_pair(conn->id(), std::move(conn)));
				   }
				   else {
					   logger()->trace("pool full, drop conn {}", conn->id());
					   delete conn.release();
					   on_conn_lost();
				   }
				}
				logger()->trace(
					"conn ready {}, remain {}, in pool {}",
					id,
					m_conn_count,
					m_conns.size()
				);
			});
		}

		void on_conn_lost() {
			boost::asio::post(m_strand, [
				this, self = shared_from_this()
			]() mutable {
				if (--m_conn_count < m_min) {
					m_fill_timer.cancel_one();
				}
				logger()->trace(
					"conn lost, remain {}, in pool {}",
					m_conn_count,
					m_conns.size()
				);
			});
		}

        awaitable<void> create_conn() {
			if (m_conn_count >= m_max) {
				co_return;
			}
			else {
				++m_conn_count;
			}
			try {
				conn_ptr_inner conn(
					new connection(m_conn_str, m_io)
				);
				co_await conn->async_connect(use_awaitable);
				on_conn_ready(std::move(conn));
			}
			catch (const error_code& ec) {
				logger()->error("create connection error: {}", ec);
				on_conn_lost();
				throw;
			}
			catch (const std::exception& ex) {
				logger()->error("create connection error: {}", ex.what());
				on_conn_lost();
				throw;
			}
        }

    private:
        std::string m_conn_str;
		std::size_t m_conn_count{ 0 };
        std::map<size_t, conn_ptr_inner> m_conns;
        std::queue<get_handler> m_pendings;
        boost::asio::io_context& m_io;
		boost::asio::io_context::strand m_strand{ m_io };
		boost::asio::steady_timer m_fill_timer{ m_strand };
        int m_min;
        int m_max;
        bool m_filling{false};
    };
};