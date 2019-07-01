#pragma once

#include <memory>
#include <iostream>
#include <limits>
#include <atomic>
#include <optional>
#include <boost/asio.hpp>
#include <libpq-fe.h>
#include <fmt/format.h>
#include <spdlog/logger.h>
#include <pqcpp/logger.hpp>
#include <pqcpp/query.hpp>
#include <pqcpp/row.hpp>
#include <pqcpp/result.hpp>
#include <pqcpp/connection_option.hpp>
#include <pqcpp/error.hpp>
#include <pqcpp/transaction.hpp>
#include <pqcpp/detail/connect_op.hpp>
#include <pqcpp/detail/query_op.hpp>
#include <pqcpp/coro.hpp>
#include <pqcpp/detail/concept.hpp>

namespace pqcpp {

	class connection : public std::enable_shared_from_this<connection> {
		friend class connection_pool;
		using error_code = boost::system::error_code;
	public:
		using socket_type = boost::asio::ip::tcp::socket;
		using strand_type = boost::asio::io_context::strand;

		using response_success_handle = std::function<void(const std::vector<std::shared_ptr<pqcpp::result>>&)>;
		using response_failure_handle = std::function<void(const std::string&)>;

		/**
		 * @brief 创建连接
		 *
		 * @param opt 连接选项
		 * @param io
		 * @return std::shared_ptr<connection>
		 */
		static std::shared_ptr<connection> make(const std::string& conn_str, boost::asio::io_context& io) {
			return std::shared_ptr<connection>{ new connection(conn_str, io) };
		}

		static std::shared_ptr<connection> make(const connection_options& opts, boost::asio::io_context& io) {
			return std::shared_ptr<connection>{ new connection(opts.get_conn_str(), io) };
		}

		~connection() {
			if (m_native_conn) {
				PQfinish(m_native_conn);
			}
			--total_;
			logger()->trace("connection {} destroy, total {}", m_id, total_);
		}

		connection(const connection&) = delete;
		connection& operator=(const connection&) = delete;

		/**
		 * @brief 获取连接字符串
		 *
		 * @return const std::string&
		 */
		const std::string& get_conn_str() const {
			return m_conn_str;
		}

		strand_type& get_strand() {
			return m_strand;
		}

		socket_type& get_socket() {
			return *m_socket;
		}

		void set_socket(std::unique_ptr<socket_type>&& socket) {
			m_socket = std::move(socket);
		}

		/**
		 * @brief 获取底层连接
		 *
		 * @return ::pg_conn*
		 */
		::pg_conn* get_native_conn() {
			return m_native_conn;
		}

		std::string error_message() {
			return ::PQerrorMessage(m_native_conn);
		}

		void set_native_conn(PGconn* native_conn) {
			m_native_conn = native_conn;
		}

		/**
		 * @brief 连接是否就绪
		 *
		 * @return true
		 * @return false
		 */
		bool is_ready() {
			return m_native_conn && m_socket && (PQstatus(m_native_conn) == CONNECTION_OK);
		}

		std::size_t id() const {
			return m_id;
		}

		/**
		 * @brief 获取锁
		 *
		 * @return std::unique_lock<std::mutex>
		 */
		std::unique_lock<std::mutex> get_lock() {
			return std::unique_lock(m_mutex);
		}

		/**
		 * @brief 开始异步连接
		 *
		 * @tparam H
		 * @param h void(boost::system::error_code)
		 */
		template <typename CompletionToken>
		auto async_connect(CompletionToken&& token) {
			return boost::asio::async_compose<
				CompletionToken,
				void(boost::system::error_code)
			>(
				detail::connect_op<connection>(*this),
				std::forward<CompletionToken>(token), this->m_io
			);
		}

		/**
		 * @brief 开始异步查询
		 *
		 * @param query
		 * @param token void(boost::system::error_code, std::vector<std::shared_ptr<pqcpp::result>>)
		 */
		template <typename CompletionToken>
		auto async_query(std::shared_ptr<query> query, CompletionToken&& token) {
			using operate_type = detail::query_op<connection>;
			return boost::asio::async_compose<
				CompletionToken,
				void(boost::system::error_code, std::vector<std::shared_ptr<result>>)
			>(
				operate_type(*this, query),
				std::forward<CompletionToken>(token), this->m_io
			);
		}

		template <typename T, typename ...Args>
		awaitable<std::vector<std::shared_ptr<result>>>
		async_query(T&& cmd, Args&& ...args) {
			auto q = std::make_shared<query>(std::forward<T>(cmd));
			if constexpr (sizeof...(Args) > 0) {
				q->set_parameters(std::forward<Args>(args)...);
			}
			return this->async_query(q, use_awaitable);
		}

		template <
			typename F,
			std::enable_if_t<
			concept::is_void_coroutine_function_v<F>,
			int
			> = 1
		>
		auto transaction(transaction::level level, F && f) -> std::invoke_result_t<F> {
			std::exception_ptr ex;
			try {
				co_await this->async_start_transaction(level, use_awaitable);
				co_await f();
			}
			catch (...) {
				ex = std::current_exception();
			}
			if (ex) {
				co_await this->async_rollback_transaction(use_awaitable);
				std::rethrow_exception(ex);
			}
			else {
				co_await this->async_end_transaction(use_awaitable);
				co_return;
			}
		}

		template <
			typename F,
			std::enable_if_t<
			concept::is_non_void_coroutine_function_v<F>,
			int
			> = 1
		>
		auto transaction(transaction::level level, F && f) -> std::invoke_result_t<F> {
			std::exception_ptr ex;
			std::optional<concept::coroutine_function_result_t<F>> r;
			try {
				co_await this->async_start_transaction(level, use_awaitable);
				r = co_await f();
			}
			catch (...) {
				ex = std::current_exception();
			}
			
			if (ex) {
				co_await this->async_rollback_transaction(use_awaitable);
				std::rethrow_exception(ex);
			}
			else {
				co_await this->async_end_transaction(use_awaitable);
				co_return *r;
			}
		}

		template <
			typename F,
			std::enable_if_t<
				concept::is_void_coroutine_function_v<F>,
				int
			> = 1
		>
		auto transaction(F && f) -> std::invoke_result_t<F> {
			return this->transaction(transaction::SERIALIZABLE, std::forward<F>(f));
		}

		template <
			typename F,
			std::enable_if_t<
				concept::is_non_void_coroutine_function_v<F>,
				int
			> = 1
		>
		auto transaction(F&& f) -> std::invoke_result_t<F> {
			return this->transaction(transaction::SERIALIZABLE, std::forward<F>(f));
		}

		template <typename CompletionToken>
		auto async_start_transaction(transaction::level level, CompletionToken&& token) {
			auto q = std::make_shared<query>(
				fmt::format("BEGIN TRANSACTION ISOLATION LEVEL {};", transaction::to_string(level)));
			logger()->trace("conn {} start transaction", this->id());
			return async_query(q, token);
		}

		template <typename CompletionToken>
		auto async_start_transaction(CompletionToken&& token) {
			return async_start_transaction(transaction::SERIALIZABLE, token);
		}

		template <typename CompletionToken>
		auto async_end_transaction(CompletionToken&& token) {
			auto q = std::make_shared<query>("END;");
			logger()->trace("conn {} end transaction", this->id());
			return async_query(q, token);
		}

		template <typename CompletionToken>
		auto async_rollback_transaction(CompletionToken&& token) {
			auto q = std::make_shared<query>("ROLLBACK;");
			logger()->trace("conn {} rollback transaction", this->id());
			return async_query(q, token);
		}

		/**
		 * @brief 断开连接
		 *
		 */
		void disconnect() {
			if (m_socket) {
				m_socket->close();
				m_socket.reset();
			}
			if (m_native_conn) {
				PQfinish(m_native_conn);
				m_native_conn = nullptr;
			}
		}

	private:
		connection(const std::string& conn_str, boost::asio::io_context& io)
			:m_conn_str(conn_str), m_io(io), m_strand(io)
		{
			++total_;
			logger()->trace("connection {} created, total {}", m_id, total_);
		}

	private:
		std::mutex m_mutex;
		const std::size_t m_id = current_id++;
		std::string m_conn_str;
		boost::asio::io_context& m_io;
		boost::asio::io_context::strand m_strand;
		std::unique_ptr<socket_type> m_socket = nullptr;
		::pg_conn* m_native_conn = nullptr;

		inline static std::atomic_size_t current_id = 0;
		inline static std::atomic_size_t total_ = 0;
	};
};
