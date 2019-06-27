#pragma once
#include <string_view>
#include <stdexcept>

namespace pqcpp {
namespace transaction {

enum level {
	SERIALIZABLE,
	REPEATABLE_READ,
	READ_COMMITTED,
	READ_UNCOMMITTED
};

inline std::string_view to_string(level l){
	switch (l)
	{
	case pqcpp::transaction::SERIALIZABLE:
		return "SERIALIZABLE";
	case pqcpp::transaction::REPEATABLE_READ:
		return "REPEATABLE READ";
	case pqcpp::transaction::READ_COMMITTED:
		return "READ COMMITTED";
	case pqcpp::transaction::READ_UNCOMMITTED:
		return "READ UNCOMMITTED";
	default:
		throw std::invalid_argument("unknow transaction level");
	}
}

}
}
