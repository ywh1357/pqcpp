#pragma once

#include <pqcpp/row.hpp>
#include <pqcpp/result.hpp>

namespace pqcpp {

    inline row::row(std::shared_ptr<const result> res, int row_num)
        :m_result(res), m_row_num(row_num)
	{}

    inline int row::col_count() const {
        return m_result->col_count();
    }

    inline const std::string& row::col_name(int col_num) const {
        return m_result->header()->col_name(col_num);
    }

    inline bool row::is_null(int col_num) const {
        return m_result->is_null(m_row_num, col_num);
    }

    template <typename T>
    inline auto row::get(int col_num) const {
        int col_count = m_result->col_count();
        if (col_num > col_count) {
            throw std::out_of_range("colume number out of colume count");
        }
        auto data = m_result->get_value(m_row_num, col_num);
        auto size = m_result->get_length(m_row_num, col_num);
        auto format = m_result->header()->col_format(col_num);
        field f{ data, size, format };
        if(this->is_null(col_num)){
            f.is_null = true;
        }
        return field_converter<T>::from_field(f);
    }

    template <typename T, typename String>
    inline auto row::get(const String& field_name) const {
        int col_num = m_result->header()->field_index(field_name);
        return get<T>(col_num);
    }

    template <typename ...Args>
    inline auto row::get_tuple() const {
        int col_num = 0;
        return std::make_tuple(get<Args>(col_num++)...);
    }

    template <typename ...Args, typename ...String>
    inline auto row::get_tuple(const String&... field_name) {
        return std::make_tuple(get<Args>(field_name)...);
    }
};