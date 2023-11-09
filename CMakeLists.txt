cmake_minimum_required(VERSION 3.26.4)

project(dataleak_parser)

#set flags for compiler
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -Wall -Wextra -g -pedantic -lssl -lcrypto")

#add external libs
find_package(ICU 71.1 COMPONENTS REQUIRED uc i18n)
find_package(Boost COMPONENTS REQUIRED 
	regex 
	json 
	log_setup 
	log)

find_package(OpenSSL REQUIRED)
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CUDA_COMPILER_LAUNCHER "${CCACHE_PROGRAM}") # CMake 3.9+
endif()
include_directories("include/" "src/")

#include all source files
set(SRC 
	src/main.cpp
	src/logging/logger.cpp
	src/database/database.cpp
	src/database/database_pool.cpp
	src/network/listener.cpp
	src/network/http_session.cpp
	src/request_handlers/request_handlers.cpp
	src/jwt_utils/jwt_utils.cpp
	src/cookie_utils/cookie_utils.cpp
	src/validation/validation.cpp
	src/validation/normalization.cpp
	#src/parsing/patterns_storage.cpp
	)

add_executable(${PROJECT_NAME} ${SRC})

target_precompile_headers(${PROJECT_NAME} PRIVATE precompiled_headers.hpp)

#link external libs
target_link_libraries(${PROJECT_NAME} 
	-lpqxx -lpq
	ICU::uc ICU::i18n
	Boost::regex 
	Boost::json 
	Boost::log_setup 
	Boost::log 
	OpenSSL::SSL OpenSSL::Crypto) 