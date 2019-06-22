#pragma once

#include <boost/system/error_code.hpp>

namespace pqcpp {
	
	using error_code = boost::system::error_code;

namespace error {

    enum class pqcpp_ec
    {
        SUCCESS,
		CONN_ALLOCATE_FAILED,
		CONNECT_FAILED,
		QUERY_FAILED,
		NETWORK_ERROR
    };

	class error_category : public boost::system::error_category
	{
	public:
		const char *name() const noexcept override {
			return "pqcpp error";
		}
		std::string message(int condition) const override {
			switch (static_cast<pqcpp_ec>(condition)) {
			case pqcpp_ec::SUCCESS: return "success";
			case pqcpp_ec::CONN_ALLOCATE_FAILED: return "PGconn allocate failed";
			case pqcpp_ec::CONNECT_FAILED: return "database connect failed";
			case pqcpp_ec::QUERY_FAILED: return "query failed";
			case pqcpp_ec::NETWORK_ERROR: return "network error";
			default:
				return "";
			}
		}
	};

	inline const error_category& pqcpp_category() {
		static const error_category _error_category{};
		return _error_category;
	}

	inline error_code make_error_code(pqcpp_ec ec) {
		return { static_cast<int>(ec), pqcpp_category() };
	}

}
}