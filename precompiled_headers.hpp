#ifndef PRECOMPILED_HEADERS_HPP
#define PRECOMPILED_HEADERS_HPP

#include <thread>
#include <memory>
#include <fstream>
#include <memory>
#include <optional>
#include <unordered_map>
#include <string>
#include <chrono>
#include <stack>
#include <mutex>
#include <stdint.h>
#include <optional>
#include <variant>
#include <queue>
#include <filesystem>

#include <unicode/unistr.h>
#include <boost/regex.hpp>
#include <boost/regex/icu.hpp>
#include <magic_enum.hpp>
#include <jwt-cpp/traits/boost-json/traits.h>
#include <boost/beast/ssl.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/beast/core.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/json.hpp>
#include <boost/functional/hash.hpp>
#include <pqxx/connection>
#include <pqxx/result>
#include <pqxx/transaction>
#include <pqxx/nontransaction>
#include <boost/log/expressions.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>

#include <config.hpp>
#include <logging/logger.hpp>
#include <network/listener.hpp>
#include <network/server_certificate.hpp>
#include <jwt_utils/jwt_utils.hpp>
#include <cookie_utils/cookie_utils.hpp>

#endif