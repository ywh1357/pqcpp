#pragma once
#include <memory>
#include <filesystem>
#include <mutex>
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/async.h>

namespace pqcpp {

	inline std::shared_ptr<spdlog::logger> make_default_logger() {
        if(!spdlog::thread_pool()){
            spdlog::init_thread_pool(8192, 1);
        }
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		//console_sink->set_pattern("[%H:%M:%S %z] [thread %t] [%n] %v");
		std::error_code ignore_ec;
		std::filesystem::create_directory("logs", ignore_ec);
		auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            (std::filesystem::path("logs") / "pqcpp").string(), 
            1048576 * 5, 
            3
        );
		auto logger = std::make_shared<spdlog::async_logger>(
			"pqcpp", 
			spdlog::sinks_init_list{ console_sink, file_sink },
			spdlog::thread_pool()
		);
		return logger;
	}

	inline std::mutex _global_logger_mutex;

	inline std::shared_ptr<spdlog::logger> _global_logger;

	inline std::shared_ptr<spdlog::logger> logger() {
		std::unique_lock<std::mutex>(_global_logger_mutex);
		if(!_global_logger){
			_global_logger = make_default_logger();
		}
		return _global_logger;
	}

	inline void set_logger(std::shared_ptr<spdlog::logger> new_logger) {
		_global_logger = new_logger;
	}

};
