#pragma once

#include <string>
#include <memory>
#include <fmt/format.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace pqcpp {

    struct connection_options {

        std::string host;
        std::string port;
        std::string db;
        std::string user;
        std::string password;
        bool ssl_enable;
        std::string ca;
        
		connection_options()
			:host("localhost"),
			port("5432"),
			db("postgres"),
			ssl_enable(false)
		{}

		std::string get_conn_str() const {
			return fmt::format("host={} port={} dbname={} user={} password={}",
				this->host, this->port, this->db, this->user, this->password);
		}

		static connection_options from_json(const std::filesystem::path& path) {
			auto conf = nlohmann::json::parse(std::ifstream{ path });
			return from_json(conf);
		}

		static connection_options from_json(const nlohmann::json& conf) {
			connection_options opts;
			opts.host = conf["host"];
			opts.port = conf["port"];
			opts.db = conf["db"];
			opts.user = conf["user"];
			opts.password = conf["password"];
			return opts;
		}
    };
}
