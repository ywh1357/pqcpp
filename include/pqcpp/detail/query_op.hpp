#pragma once

#include <memory>
#include <functional>
#include <libpq-fe.h>
#include <boost/asio.hpp>
#include <pqcpp/logger.hpp>
#include <pqcpp/query.hpp>
#include <fmt/ostream.h>
#include <fmt/ranges.h>

namespace pqcpp {
namespace detail {

    /**
     * @brief 
     * 
     * @tparam Conn 
     * @tparam CompleteHandler void(boost::system::error_code, std::vector<std::shared_ptr<pqcpp::result>>)
     */
    template <typename Conn>
    struct query_op
    {
        using socket_type = typename Conn::socket_type;
		enum { starting, writing, reading, done } state_;

		Conn& m_conn;
		std::shared_ptr<query> m_query;

        query_op(Conn& conn, std::shared_ptr<query> query)
            :m_conn(conn), m_query(query), state_(starting)
        {

			logger()->trace("query: {}\tparameters: {}", query->m_cmd, query->m_params_values);
		}

        bool send_query(const query& q) {
			return PQsendQueryParams(
				m_conn.get_native_conn(),
				q.command(),
				q.params_size(),
				nullptr,
				q.params_values(),
				q.params_lengths(),
				q.params_formats(),
				0
			) == 1;
		}

		template <typename Self>
		void on_query_complete(Self& self) {
            m_conn.get_socket().cancel();
            std::vector<std::shared_ptr<result>> results;
            while (auto pg_res = PQgetResult(m_conn.get_native_conn())) {
                results.push_back(std::make_shared<result>(pg_res));
            }
			if(results.empty()){
				int i  = 1;
			}
			self.complete({}, std::move(results));
		}

		template <typename Self>
		void on_query_failure(Self& self, const error_code& ec) {
			m_conn.disconnect();
			self.complete(ec, {});
		}

		template <typename Self>
		void operator()(Self& self, const error_code& ec = {}) {
			if (state_ == starting) {
				if (!this->send_query(*this->m_query)) {
					logger()->error(
						"connection {} send query error: {}",
						m_conn.id(),
						m_conn.error_message()
					);
					this->on_query_failure(self, error::make_error_code(error::pqcpp_ec::QUERY_FAILED));
					return;
				}
				state_ = writing;
			}
			switch (state_) {
			case writing:
				this->query_write(self, ec);
				break;
			case reading:
				this->query_read(self, ec);
				break;
			}
		}

		template <typename Self>
		void query_write(Self& self, const error_code& ec = {}) {
			if (ec) {
                if(ec == boost::asio::error::operation_aborted){
                    return;
                }
				error_code ignore_ec;
                m_conn.get_socket().cancel(ignore_ec);
				logger()->error("connection {} write error {}: {}", m_conn.id(), ec.value(), ec.message());
				this->on_query_failure(self, ec);
				return;
			}
			int flush_res = PQflush(m_conn.get_native_conn());
			if (flush_res == -1) {
				logger()->error("connection {} query write error: {}", m_conn.id(), m_conn.error_message());
				on_query_failure(self, error::make_error_code(error::pqcpp_ec::NETWORK_ERROR));
				return;
			}
			else if (flush_res == 1) {
				logger()->trace("connection {} query wait write", m_conn.id());
				m_conn.get_socket().async_wait(socket_type::wait_write, boost::asio::bind_executor(
					m_conn.get_strand(),
					std::move(self)
				));
				return;
			}
			else if(flush_res == 0) {
				state_ = reading;
				this->query_read(self);
				return;
			}
			return;
		}

		template <typename Self>
		void query_read(Self& self, const error_code& ec = {}) {
			if (ec) {
                if(ec == boost::asio::error::operation_aborted){
                    return;
                }
				error_code ignore_ec;
                m_conn.get_socket().cancel(ignore_ec);
				logger()->error("connection {} read error {}: {}", m_conn.id(), ec.value(), ec.message());
				on_query_failure(self, ec);
				return;
			}
			if (PQconsumeInput(m_conn.get_native_conn()) == 0) {
				std::string error = PQerrorMessage(m_conn.get_native_conn());
				logger()->error("connection {} query read error: {}", m_conn.id(), error);
				on_query_failure(self ,error::make_error_code(error::pqcpp_ec::QUERY_FAILED));
				return;
			}
			if (PQisBusy(m_conn.get_native_conn()) == 0) {
				logger()->debug("connection {} query success", m_conn.id());
				on_query_complete(self);
                return;
			}
			m_conn.get_socket().async_wait(socket_type::wait_read, boost::asio::bind_executor(
				m_conn.get_strand(),
				std::move(self)
			));
		}
    };

}
}