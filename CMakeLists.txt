cmake_minimum_required(VERSION 3.27.7)

project(server_backend)

#set flags for compiler
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -lssl -lcrypto")
set(CMAKE_CXX_FLAGS_DEBUG "-Wall -Wextra -g -pedantic")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native")

#add external libs
find_package(ICU 74.1 COMPONENTS REQUIRED uc i18n)
find_package(Boost COMPONENTS REQUIRED 
	regex 
	json 
	log_setup 
	log)

find_package(OpenSSL REQUIRED)
 
#include all source files
set(SRC 
	src/main.cpp
	src/logging/logger.cpp
	src/database/database_connection.cpp
	src/database/database_connections_pool.cpp
	src/network/listener.cpp
	src/network/http_session.cpp
	src/network/uri_params.cpp
	src/jwt_utils/jwt_utils.cpp
	src/cookie_utils/cookie_utils.cpp
	# src/request_handlers/request_handlers.cpp
	src/request_handlers/user/user_request_handlers.cpp
	src/request_handlers/file_system/file_system_request_handlers.cpp
	src/parsing/file_types_conversion/file_types_conversion.cpp
	src/parsing/file_preview/file_preview.cpp
	# src/parsing/validation/validation.cpp
	# src/parsing/validation/normalization.cpp
	)

add_executable(${PROJECT_NAME} ${SRC})

target_include_directories(${PROJECT_NAME} PRIVATE "include/" "src/")

target_precompile_headers(${PROJECT_NAME} PRIVATE precompiled_headers.hpp)

#link external libs
target_link_libraries(${PROJECT_NAME} 
	-lpqxx -lpq
	-lbit7z64
	ICU::uc ICU::i18n
	Boost::regex 
	Boost::json 
	Boost::log_setup 
	Boost::log 
	OpenSSL::SSL OpenSSL::Crypto) 