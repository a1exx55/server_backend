cmake_minimum_required(VERSION 3.26.4)

project(dataleak_parser)

#set flags for compiler
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -Wall -Wextra -pedantic -lssl -lcrypto")

#add external libs
find_package(Boost COMPONENTS REQUIRED 
	regex 
	json 
	log_setup 
	log)
	
find_package(OpenSSL REQUIRED)

include_directories("include/" "src/")

#include all source files
set(SRC 
	src/main.cpp
	src/logging/logger.cpp
	#src/validation/validation.cpp
	src/network/listener.cpp
	src/network/http_session.cpp
	src/request_handlers/request_handlers.cpp
	src/jwt_utils/jwt_utils.cpp)

add_executable(${PROJECT_NAME} ${SRC})

#link external libs
target_link_libraries(${PROJECT_NAME} 
	Boost::regex 
	Boost::json 
	Boost::log_setup 
	Boost::log 
	OpenSSL::SSL 
	OpenSSL::Crypto) 