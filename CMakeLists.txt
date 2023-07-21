cmake_minimum_required(VERSION 3.26.4)

project(dataleak_parser)

#set flags for compiler
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -Wall -DBOOST_LOG_DYN_LINK")

#add external libs
find_package(Boost COMPONENTS REQUIRED regex log_setup log)

include_directories("include/" "src/")

#include all source files
set(SRC 
	src/main.cpp
	src/logging/logger.cpp
	src/validation/validation.cpp
	src/network/listener.cpp)

add_executable(${PROJECT_NAME} ${SRC})

#link external libs
target_link_libraries(${PROJECT_NAME} Boost::regex Boost::log_setup Boost::log) 