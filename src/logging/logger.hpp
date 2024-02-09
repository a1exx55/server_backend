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