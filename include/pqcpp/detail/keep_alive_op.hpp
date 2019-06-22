#pragma once

#include <memory>
#include <functional>
#include <libpq-fe.h>
#include <boost/asio.hpp>
#include <pqcpp/logger.hpp>

namespace pqcpp {
namespace detail {
    
    template <typename Conn, typename CompleteHandler>
    class keep_alive_op: public std::enable_shared_from_this<keep_alive_op<Conn, CompleteHandler>>
    {
    public:

        keep_alive_op(Conn& conn, CompleteHandler&& handler)
            :m_conn(conn), m_handler(handler)
        {}

    private:

        

    private:
        Conn& m_conn;
        CompleteHandler m_handler;
        ::pg_conn* m_native_conn;
    };

}
}