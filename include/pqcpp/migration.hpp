#pragma once

#include <filesystem>
#include <fstream>
#include <pqcpp/coro.hpp>
#include <pqcpp/connection.hpp>
#include <pqcpp/error.hpp>

namespace pqcpp {

namespace detail {
std::string_view create_migrations_table_cmd =
R"(CREATE TABLE IF NOT EXISTS migrations(
    id SERIAL PRIMARY KEY,
	version integer NOT NULL,
	"timestamp" bigint NOT NULL DEFAULT extract(EPOCH FROM now()),
	name text NOT NULL
);)";

struct migration_record {
	int version;
	std::string name;
	std::filesystem::path file;
};

inline migration_record create_migration_record(const std::filesystem::path& path) {
	auto filename = path.filename().string();
	auto delimiter_pos = filename.find("-");
	auto dot_pos = filename.rfind(".");
	auto version_str = filename.substr(0, delimiter_pos);
	migration_record r = {
		std::stoi(version_str),
		filename.substr(delimiter_pos + 1, dot_pos - delimiter_pos - 1),
		path
	};
	return r;
}

}

class migration {
public:
	using result_ptr = std::shared_ptr<result>;

	migration(boost::asio::io_context& io, const connection_options& opts, const std::string& migrations_dir)
		:m_io(io), m_opts(opts), m_migrations_dir(migrations_dir)
	{
		namespace fs = std::filesystem;

		fs::path dir(migrations_dir);
		
		if (!(fs::exists(dir) && fs::is_directory(dir))) {
			error::make_error_code(error::pqcpp_ec::INVALID_MIGRATIONS_DIR);
		}

		for (auto& p : fs::directory_iterator(dir)) {
			if (!p.is_regular_file()) {
				continue;
			}
			auto r = detail::create_migration_record(p);
			m_migrations.emplace(r.version, std::move(r));
		}

	}

	awaitable<void> run() {
		auto conn = connection::make(m_opts, m_io);
		co_await conn->async_connect(use_awaitable);
		auto create_results = co_await conn->async_query(detail::create_migrations_table_cmd);
		ensure_success(create_results);
		co_await conn->transaction([this, conn]() -> awaitable<void> {
			auto results = co_await conn->async_query("SELECT * FROM migrations ORDER BY id DESC LIMIT 1;");
			ensure_success(results);
			auto check_result = results.front();
			auto begin = m_migrations.begin();
			if (check_result->row_count() > 0) {
				auto version = (*check_result->begin()).get<int>("version");
				begin = m_migrations.upper_bound(version);
			}
			while (begin != m_migrations.end())
			{
				const auto& r = begin->second;
				logger()->info("run migration: version={}, name={}", r.version, r.name);
				auto cmd = this->read_file(r.file);
				auto migrate_results = co_await conn->async_query(cmd);
				ensure_success(migrate_results);
				auto save_migration_results = co_await conn->async_query(
					"INSERT INTO public.migrations(version, name) VALUES($1::integer, $2::text);",
					r.version, r.name
				);
				ensure_success(save_migration_results);
				logger()->info("migrate {} success", r.file.filename().string());
				++begin;
			}
		});
	}

private:
	void ensure_success(const std::vector<result_ptr>& results) {
		if (!results.empty()) {
			auto result = results.front();
			if (result->success()) {
				return;
			}
			else {
				logger()->error("{}", result->error_message());
			}
		}
		throw error::make_error_code(error::pqcpp_ec::QUERY_FAILED);
	}

	std::string read_file(const std::filesystem::path& path) {
		std::ifstream t(path);
		std::stringstream buffer;
		buffer << t.rdbuf();
		return buffer.str();
	}

private:
	boost::asio::io_context& m_io;
	connection_options m_opts;
	std::string m_migrations_dir;
	std::map<int, detail::migration_record> m_migrations;
};

} // namespace pqcpp
