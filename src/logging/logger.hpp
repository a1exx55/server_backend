#ifndef LOGGER_HPP
#define LOGGER_HPP

//local
#include <config.hpp>

//external
#include <boost/log/expressions.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>

namespace src = boost::log::sources;
namespace expr = boost::log::expressions;
namespace logging = boost::log;

#define LOG(LEVEL) BOOST_LOG_SEV(_logger_mt::get(), LEVEL)  \
    << logging::add_value("Line", __LINE__)  \
    << logging::add_value("File", boost::filesystem::path(__FILE__).filename().string())  \
    << logging::add_value("Function", __FUNCTION__)

#define LOG_ERROR LOG(logging::trivial::error)
#define LOG_DEBUG LOG(logging::trivial::debug)
#define LOG_INFO LOG(logging::trivial::info)

typedef src::severity_logger_mt<logging::trivial::severity_level> logger_mt;
BOOST_LOG_GLOBAL_LOGGER(_logger_mt, logger_mt)

#endif



















// #include <boost/log/support/date_time.hpp>
// #include <boost/log/core.hpp>
// #include <boost/log/trivial.hpp>
// #include <boost/log/expressions.hpp>
// #include <boost/log/sinks/text_file_backend.hpp>
// #include <boost/log/sinks/sync_frontend.hpp>
// #include <boost/log/utility/setup/file.hpp>
// #include <boost/log/utility/setup/console.hpp>
// #include <boost/log/utility/setup/common_attributes.hpp>
// #include <boost/log/core.hpp>
// #include <boost/log/expressions.hpp>
// #include <boost/log/sinks/sync_frontend.hpp>
// #include <boost/log/sinks/text_file_backend.hpp>
// #include <boost/log/utility/setup/console.hpp>
// #include <boost/log/utility/setup/file.hpp>
// #include <boost/log/sources/logger.hpp>
// namespace logging = boost::log;
// namespace sinks = boost::log::sinks;
// namespace expr = boost::log::expressions;
// namespace src = boost::log::sources;
// void init_logging()
// {
//     // Форматируем вывод в консоль
//     logging::add_console_log(std::cout, logging::keywords::format = "%Message%");

//     // Форматируем вывод в файл "log.txt"
//     logging::add_file_log(
//         logging::keywords::file_name = "log.txt",
//         logging::keywords::format = "%TimeStamp%: %Message%"
//     );

//     // Устанавливаем уровень важности лог-сообщений
//     logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::info);
// }

// int main(int, char*[])
// {
//     // Initialize sinks
//     logging::add_console_log()->set_filter(expr::attr< int >("Severity") >= 4);

//     logging::formatter formatter =
//         expr::stream
//             << expr::attr< unsigned int >("LineID") << ": "
//             << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S") << " *"
//             << expr::attr< int >("Severity") << "* "
//             << expr::message;

//     logging::add_file_log("complete.log")->set_formatter(formatter);

//     boost::shared_ptr<
//         sinks::synchronous_sink< sinks::text_file_backend >
//     > sink = logging::add_file_log("essential.log");
//     sink->set_formatter(formatter);
//     sink->set_filter(expr::attr< int >("Severity") >= 1);

//     // Register common attributes
//     logging::add_common_attributes();

//     // Here we go, we can write logs
//     src::logger lg;
//     BOOST_LOG(lg) << "Hello world!";

//     return 0;
// }
