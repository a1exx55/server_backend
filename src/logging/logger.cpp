#include <logging/logger.hpp>

BOOST_LOG_GLOBAL_LOGGER_INIT(_logger_mt, src::severity_logger_mt)
{
    logger_mt _logger_mt;
    logging::add_common_attributes();

    const auto _format = expr::stream 
        << "[" << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S")
        << "] <" << expr::attr<logging::trivial::severity_level>("Severity") << "> ["
        << expr::attr<std::string>("File") << ":" 
        << expr::attr<int>("Line") << "] ["
        << expr::attr<std::string>("Function") << "] "
        << expr::smessage;

    logging::add_file_log(
        logging::keywords::file_name = config::LOG_FILE_PATH,
        logging::keywords::open_mode = std::ios::app,
        logging::keywords::format = _format);
        
    if (config::CONSOLE_LOG_ENABLED)
    {
        logging::add_console_log(
            std::clog,
            logging::keywords::format = _format);
    }

    logging::core::get()->set_filter
    (
        logging::trivial::severity >= logging::trivial::debug
    );
    return _logger_mt;
}