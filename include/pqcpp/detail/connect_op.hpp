#pragma once

#include <memory>
#include <functional>
#include <libpq-fe.h>
#include <boost/asio.hpp>
#include <pqcpp/logger.hpp>
#include <pqcpp/connection_option.hpp>
#include <pqcpp/error.hpp>
#include <pqcpp/detail/concept.hpp>

namespace pqcpp {
namespace detail {

    /**
     * @brief async connect operation
     * 
     * @tparam CompleteHandler void(boost::system::error_code)
     */
    template <typename Conn>
    struct connect_op
    {
        using socket_type = typename Conn::socket_type;
		using handler_type = void(error_code);

        connect_op(Conn& conn)
            : m_conn(conn)
        {
            const auto& conn_str = conn.get_conn_str();
			const char* keywords[] = { "dbname", nullptr };
			const char* values[] = { conn_str.c_str(), nullptr };
			m_native_conn = PQconnectdbParams(keywords, values, 1);
			m_socket = make_socket(PQsocket(m_native_conn));
        }

        std::unique_ptr<socket_type> make_socket(const typename socket_type::native_handle_type& fd) {
			return std::make_unique<socket_type>(
				m_conn.get_strand(), 
				boost::asio::ip::tcp::v4(), 
				fd
			);
		}

		template <typename Self>
		void operator()(Self& self, error_code ec = {}) {
			if (!m_native_conn) {
				self.complete(error::make_error_code(error::pqcpp_ec::CONN_ALLOCATE_FAILED));
				return;
			}
			if (ec) {
				logger()->error("connection {} connect error {}: {}", m_conn.id(), ec.value(), ec.message());
				if (m_native_conn) {
					PQfinish(m_native_conn);
				}
				self.complete(ec);
				return;
			}
			auto status = PQconnectPoll(m_native_conn);
			if (status != PGRES_POLLING_FAILED) {
				auto fd = PQsocket(m_native_conn);
				if (fd != m_socket->native_handle()) {
					logger()->debug("connection {} socket changed", m_conn.id());
					m_socket = make_socket(fd);
				}
			}
			switch (status) {
			case PGRES_POLLING_READING:
				m_socket->async_wait(socket_type::wait_read, boost::asio::bind_executor(
					m_conn.get_strand(),
					std::move(self)
				));
				break;
			case PGRES_POLLING_WRITING:
				m_socket->async_wait(socket_type::wait_write, boost::asio::bind_executor(
					m_conn.get_strand(),
					std::move(self)
				));
				break;
			case PGRES_POLLING_OK:
			{
				logger()->info("connection {} connected", m_conn.id());
				m_conn.set_socket(std::move(m_socket));
				m_conn.set_native_conn(m_native_conn);
				self.complete(error_code{});
				break;
			}
			case PGRES_POLLING_FAILED:
			default:
			{
				logger()->error("connection {} connect failure: {}", m_conn.id(), PQerrorMessage(m_native_conn));
				self.complete(error::make_error_code(error::pqcpp_ec::CONNECT_FAILED));
			}
			}
		}

		Conn& m_conn;
        std::unique_ptr<socket_type> m_socket;
        ::pg_conn* m_native_conn;
    };

}
}