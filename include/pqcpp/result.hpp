#pragma once

#include <pqcpp/row.hpp>

namespace pqcpp {
    
	class result: public std::enable_shared_from_this<result> {
	public:

		class iterator;

		/**
		 * @brief Construct a new result object
		 * 
		 * @param res 
		 */
		result(PGresult* res)
			:m_res(res)
		{}

		~result() {
			PQclear(m_res);
		}

		result(const result&) = delete;
		result(result&&) = delete;
		result& operator=(const result&) = delete;

		ExecStatusType status() const {
			return PQresultStatus(m_res);
		}

		const char* status_str() const {
			return PQresStatus(this->status());
		}

		const char * error_message() const {
			return PQresultErrorMessage(m_res);
		}

		bool is_succeed() const {
			switch(this->status()){
			case PGRES_COMMAND_OK:
			case PGRES_TUPLES_OK:
				return true;
			default:
				return false;
			}
		}

		/**
		 * @brief 获取总行数
		 * 
		 * @return int 
		 */
		int row_count() const {
			return PQntuples(m_res);
		}
		
		/**
		 * @brief 获取总列数
		 * 
		 * @return int 
		 */
		int col_count() const {
			return PQnfields(m_res);
		}

		bool is_null(int row, int col) const {
			return PQgetisnull(m_res, row, col) == 1;
		}

		/**
		 * @brief 获取原始数据
		 * 
		 * @param row 行号
		 * @param col 列号
		 * @return const char* 
		 */
		const char* get_value(int row, int col) const {
			return PQgetvalue(m_res, row, col);
		}

		/**
		 * @brief 获取数据长度
		 * 
		 * @param row 
		 * @param col 
		 * @return int 
		 */
		int get_length(int row, int col) const {
			return PQgetlength(m_res, row, col);
		}

		/**
		 * @brief 获取表头
		 * 
		 * @return std::shared_ptr<const pqcpp::header> 
		 */
		std::shared_ptr<const pqcpp::header> header() const {
			if (!m_header) {
				m_header = std::make_shared<pqcpp::header>(m_res);
			}
			return m_header;
		}

		/**
		 * @brief 获取行
		 * 
		 * @param row_num 行号
		 * @return pqcpp::row 
		 */
		pqcpp::row row(int row_num) const;

		iterator begin();

		iterator end();

	private:
		PGresult* m_res;
		mutable std::shared_ptr<pqcpp::header> m_header;
	};

};

#include <pqcpp/detail/result_impl.hpp>