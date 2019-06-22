#pragma once

#include <string>
#include <memory>

namespace pqcpp {

    struct connection_option {

        std::string host;
        std::string port;
        std::string db;
        std::string user;
        std::string password;
        bool ssl_enable;
        std::string ca;
        
		connection_option()
			:host("localhost"),
			port("5432"),
			db("postgres"),
			ssl_enable(false)
		{}
    };
}
