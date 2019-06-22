# pqcpp

postgres 数据库 libpq c++封装

```c++
#include <boost/asio.hpp>
#include <spdlog/spdlog.h>
#include <pqcpp/pqcpp.hpp>
#include <nlohmann/json.hpp>

using namespace pqcpp;
using nlohmann::json;

awaitable<void> test_coro(std::shared_ptr<connection_pool> pool) {
    try {
        auto conn = co_await pool->get(use_awaitable);
        logger()->info("get conn {}", conn->id());
        co_await conn->async_start_transaction(use_awaitable);
        auto q = std::make_shared<query>(
            R"(SELECT id, field1, "field2" FROM public.foo WHERE id = $1::bigint;)"
        );
        q->set_parameters(42);
        auto results = co_await conn->async_query(q, use_awaitable);
        auto res = results.front();
        if (!res->success()) {
            logger()->error("query failure: {}", res->error_message());
        }
        else {
            json jRes;
            for (auto row : *res) {
                auto [id, field1, field2] = 
                    row.get_tuple<int, std::string, int>("id", "field1", "field2");
                jRes.push_back({
                    { "id", id },
                    { "field1", field1 },
                    { "field2", field2 }
                });
            }
            logger()->info("query result: {}", jRes.dump());
        }
        co_await conn->async_end_transaction(use_awaitable);
    }
    catch (const error_code& ec) {
        spdlog::error("test error: {}", ec.message());
    }
    catch (const std::exception& ex) {
        spdlog::error("exception: {}", ex.what());
    }
}

int main() {
	logger()->set_level(spdlog::level::trace);
	logger()->set_pattern("[%H:%M:%S %z] [thread %t] [%n] [%l] %v");

	boost::asio::io_context io;
	auto worker = boost::asio::make_work_guard(io.get_executor());

	std::string conn_str = "host=localhost dbname=yourdb user=username password=somepassword";

	auto pool = connection_pool::make(io, conn_str, 3, 10);

	co_spawn(io, [&] {
		return test_coro(pool);
	}, detached);

	// for (int i = 0; i < 16; ++i) {
	// 	std::thread([&] {
	// 		io.run();
	// 	}).detach();
	// }

	io.run();
    
    return 0;
}
```
## 安装  
`vcpkg install nlohmann-json fmt boost-asio spdlog`

`git submodule add https://github.com/ywh1357/pqcpp.git`

### CMAKE:  
```cmake
if(MSVC)
    add_compile_options(-await -utf-8)
endif(MSVC)

add_subdirectory(pqcpp)

add_executable(pqcpp_test "src/main.cpp")
target_link_libraries(pqcpp_test pqcpp)
```