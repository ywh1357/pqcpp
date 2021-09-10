#pragma once

#include <type_traits>
#include <charconv>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <boost/lexical_cast.hpp>
#include <nlohmann/json.hpp>
#include <pqcpp/data.hpp>

namespace pqcpp {

	using json = nlohmann::json;

	template <typename T, typename Enable = void>
	struct field_converter {
		
	};

	template <>
	struct field_converter<char*> {
		static field to_field(const char* input, field_format format = text_format) {
			std::string_view str{ input };
			return { str.data(), str.size(), format };
		}
	};

	template <typename T>
	struct field_converter<T, typename std::enable_if<std::is_arithmetic_v<T>>::type>
	{
		static field to_field(const T& input, field_format format = text_format) {
			auto str = std::to_string(input);
			return { str.data(), str.size(), format };
		}

		static T from_field(const abstract_field& field) {
			//return boost::lexical_cast<T>(field.data(), field.size());
			T result{0};
			std::from_chars(field.data(), field.data() + field.size(), result);
			return result;
		}
	};

	template <>
	struct field_converter<std::string>
	{
		static field to_field(const std::string& input, field_format format = text_format) {
			return { input.data(), input.size(), format };
		}

		static std::string from_field(const abstract_field& field) {
			return { field.data(), field.data() + field.size() };
		}
	};

	template <>
	struct field_converter<std::string_view> {

		static field to_field(const std::string_view& input, field_format format = text_format) {
			return { input.data(), input.size(), format };
		}

		static std::string_view from_field(const abstract_field& field) {
			return { field.data() };
		}
	};

	template <>
	struct field_converter<json> {

		static field to_field(const json& input, field_format format = text_format) {
			auto str = input.dump();
			return { str.data(), str.size(), format };
		}

		static json from_field(const abstract_field& field) {
			return json::parse(std::string_view{ field.data(), static_cast<std::size_t>(field.size()) });
		}
	};

	template <typename T, typename Alloc>
	struct field_converter<std::vector<T, Alloc>, typename std::enable_if<sizeof(T) == 1>::type> {

		using data_type = std::vector<T, Alloc>;

		static field to_field(const data_type& input, field_format format = binary_format) {
			return { (const char*)input.data(), input.size(), format };
		}

		static data_type from_field(const abstract_field& field) {
			return data_type{ 
				(const T*)field.data(), 
				(const T*)(field.data() + static_cast<std::size_t>(field.size())) 
			};
		}
	};

	template <typename T>
	struct field_converter<std::optional<T>> {

		using data_type = std::optional<T>;

		static field to_field(const data_type& input, field_format format = text_format) {
			if(input){
				return field_converter<T>::to_field(*input);
			}else{
				field f{ nullptr, 0, field_format::text_format };
				f.is_null = true;
				return f;
			}
		}

		static data_type from_field(const abstract_field& field) {
			if(field.null()){
				return std::nullopt;
			}else{
				return field_converter<T>::from_field(field);
			}
		}
	};
}
