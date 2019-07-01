#pragma once

#include <string>
#include <memory>
#include <fmt/format.h>

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
			return fmt::format("host={} dbname={} user={} password={}",
				this->host, this->db, this->user, this->password);
		}
    };
}
