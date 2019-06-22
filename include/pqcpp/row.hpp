#pragma once

#include <memory>
#include <vector>
#include <string>
#include <tuple>
#include <libpq-fe.h>
#include <pqcpp/converter.hpp>

namespace pqcpp {

	class header {
	public:

		/**
		 * @brief Construct a new header object
		 * 
		 * @param res 
		 */
		header(const PGresult* res) {
			auto nCol = PQnfields(res);
			for (int i = 0; i < nCol; ++i) {
				field_format format = PQfformat(res, i) == 1 ? binary_format : text_format;
				m_cols.push_back({ PQfname(res, i), format, PQftype(res, i) });
			}
		}

		/**
		 * @brief 列数
		 * 
		 * @return int 
		 */
		int col_count() const {
			return static_cast<int>(m_cols.size());
		}

		/**
		 * @brief 字段名
		 * 
		 * @param col_num 列号
		 * @return const std::string& 
		 */
		const std::string& col_name(int col_num) const {
			return m_cols.at(col_num).name;
		}

		/**
		 * @brief 字段格式
		 * 
		 * @param col_num 
		 * @return field_format 
		 */
		field_format col_format(int col_num) const {
			return m_cols.at(col_num).format;
		}

		/**
		 * @brief 类型
		 * 
		 * @param col_num 
		 * @return Oid 
		 */
		Oid col_type(int col_num) const {
			return m_cols.at(col_num).type;
		}

		/**
		 * @brief 判断是否有该字段
		 * 
		 * @tparam String 
		 * @param field_name 
		 * @return true 
		 * @return false 
		 */
		template <typename String>
		bool has_field(const String& field_name) const {
			return std::any_of(m_cols.begin(), m_cols.end(), [&field_name](const col& c) {
				return c.name == field_name;
			});
		}

		/**
		 * @brief 字段索引
		 * 
		 * @tparam String 
		 * @param field_name 
		 * @return int 
		 */
		template <typename String>
		int field_index(const String& field_name) const {
			auto it = std::find_if(m_cols.begin(), m_cols.end(), [&field_name](const col& c) {
				return c.name == field_name;
			});
			if (it == m_cols.end()) {
				throw std::runtime_error("field not found");
			}
			return it - m_cols.begin();
		}

	private:
		struct col {
			std::string name;
			field_format format{ text_format };
			Oid type{};
		};

		std::vector<col> m_cols;
	};

	class result;

	class row {
	public:
		/**
		 * @brief Construct a new row object
		 * 
		 * @param res
		 * @param row_num 行号
		 */
		row(std::shared_ptr<const result> res, int row_num);

		/**
		 * @brief 列数
		 * 
		 * @return int 
		 */
		int col_count() const;

		/**
		 * @brief 字段名
		 * 
		 * @param col_num 
		 * @return const std::string& 
		 */
		const std::string& col_name(int col_num) const;

		/**
		 * @brief 是否为null
		 * 
		 * @param col_num 
		 * @return true 
		 * @return false 
		 */
		bool is_null(int col_num) const;

		/**
		 * @brief 根据列号转换并获取字段值
		 * 
		 * @tparam std::string 值类型
		 * @param col_num 列号
		 * @return T 
		 */
		template <typename T = std::string>
		auto get(int col_num) const;

		/**
		 * @brief 根据字段名转换并获取字段值
		 * 
		 * @tparam std::string 
		 * @tparam String 
		 * @param field_name 字段名
		 * @return T 
		 */
		template <typename T = std::string, typename String>
		auto get(const String& field_name) const;

		/**
		 * @brief 按顺序获取值
		 * 
		 * @tparam Args 字段类型列表
		 * @return std::tuple<Args...> 
		 */
		template <typename ...Args>
		auto get_tuple() const;

		/**
		 * @brief 按顺序获取值
		 * 
		 * @tparam Args 字段类型列表
		 * @tparam String 
		 * @param field_name 字段名列表
		 * @return std::tuple<Args...> 
		 */
		template <typename ...Args, typename ...String>
		auto get_tuple(const String&... field_name);
	private:
		int m_row_num;
		std::shared_ptr<const result> m_result;
	};

}

#include <pqcpp/detail/row_impl.hpp>