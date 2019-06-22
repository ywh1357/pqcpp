#pragma once
#include <boost/asio.hpp>

namespace pqcpp {

	using boost::asio::executor;
	using boost::asio::awaitable;
	using boost::asio::co_spawn;
	using boost::asio::detached;
	using boost::asio::use_awaitable;
	using boost::asio::redirect_error;
	namespace this_coro = boost::asio::this_coro;

}