#pragma once
#include <vector>

namespace pqcpp {

	enum field_format {
		text_format,
		binary_format
	};

	struct abstract_field {
		virtual const char* data() const = 0;
		virtual int size() const = 0;
		virtual bool null() const = 0;
		virtual ~abstract_field() {}
	};

	struct field: abstract_field {
		std::vector<char> storage;
		field_format format;
		bool is_null{ false };

		const char* data() const override {
			return storage.data();
		}
		int size() const override {
			return static_cast<int>(storage.size() - 1);
		}

		bool null() const override {
			return is_null;
		}

		field() {}

		template <typename SIZE>
		field(const char* _data, SIZE _size, field_format _format)
			:storage(std::max<SIZE>(_size + 1, 1)), format(_format)
		{
			storage.back() = 0;
			std::memcpy(storage.data(), _data, _size);
		}
	};

	struct field_view: abstract_field {
		const char* data_ptr;
		int data_size;
		field_format format;
		bool is_null{ false };

		const char* data() const override {
			return data_ptr;
		}
		int size() const override {
			return data_size;
		}

		bool null() const override {
			return is_null;
		}

		field_view() {}

		template <typename SIZE>
		field_view(const char* _data, SIZE _size, field_format _format)
			:data_ptr(_data), data_size(_size), format(_format)
		{}
	};

}