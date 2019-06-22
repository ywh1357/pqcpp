#pragma once

#include <pqcpp/result.hpp>

namespace pqcpp {

    class result::iterator {
	public:
		using iterator_category = std::input_iterator_tag;
		using value_type = pqcpp::row;
		using difference_type = int;

		iterator(int row_num, result& res)
			:m_row_num(row_num), m_res(res)
		{}

		bool operator==(const iterator& r) const {
			return m_row_num == r.m_row_num;
		}

		bool operator!=(const iterator& r) const {
			return m_row_num != r.m_row_num;
		}

		value_type operator*() const {
			return m_res.row(m_row_num);
		}

		iterator& operator++() {
			++m_row_num;
			return *this;
		}

		iterator operator++(int) {
			auto temp = *this;
			++m_row_num;
			return temp;
		}

	private:
		result& m_res;
		int m_row_num;
	};
  
    inline pqcpp::row result::row(int row_num) const {
        return pqcpp::row(shared_from_this(), row_num);
	}

	inline result::iterator result::begin() {
		return {0, *this};
	}

	inline result::iterator result::end() {
		return {this->row_count(), *this};
	}
};