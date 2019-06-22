#pragma once

#include <string>
#include <vector>
#include <memory>
#include <boost/lexical_cast.hpp>
#include <pqcpp/converter.hpp>

namespace pqcpp {


	class result;

	class connection;

	class query {
	public:

		/**
		 * @brief Construct a new query object
		 * 
		 * @param cmd SQL预处理字符串
		 */
		template <typename T>
		query(T&& cmd)
			:m_cmd(cmd)
		{}

		/**
		 * @brief 设置参数
		 * 
		 * @tparam Args 参数类型
		 * @param args 参数
		 */
		template <typename ...Args>
		void set_parameters(const Args& ...args) {
			if constexpr (sizeof...(args) > 0) {
				m_params_values.clear();
				m_params_lengths.clear();
				m_params_formats.clear();
				m_position_params = std::vector<field>({ field_converter<std::decay_t<Args>>::to_field(args)... });
				for (const auto& f : m_position_params) {
					m_params_values.push_back(f.data());
					m_params_lengths.push_back(f.size());
					m_params_formats.push_back(f.format);
				}
			}
		}

		bool not_result() const {
			return m_not_result;
		}

		void set_not_result(bool is_not_result) {
			m_not_result = is_not_result;
		}

		/**
		 * @brief 获取SQL字符串
		 * 
		 * @return const char* 
		 */
		const char* command() const {
			return m_cmd.data();
		}

		const char* const * params_values() const {
			return data_or_null(m_params_values);
		}

		int params_size() const {
			return static_cast<int>(m_position_params.size());
		}

		const int* params_lengths() const {
			return data_or_null(m_params_lengths);
		}

		const int* params_formats() const {
			return data_or_null(m_params_formats);
		}

	private:

		template <typename Member>
		auto data_or_null(const Member& member) const -> decltype(member.data()) {
			if (member.empty()) {
				return (decltype(member.data()))0;
			}
			else {
				return member.data();
			}
		}

	private:
		std::string m_cmd;
		std::vector<field> m_position_params;
		std::vector<const char*> m_params_values;
		std::vector<int> m_params_lengths;
		std::vector<int> m_params_formats;
		bool m_not_result{ false };
	};

}
