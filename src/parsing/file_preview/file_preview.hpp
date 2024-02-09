#ifndef FILE_PREVIEW_HPP
#define FILE_PREVIEW_HPP

// local
#include <logging/logger.hpp>

// internal
#include <filesystem>

// external
#include <boost/json.hpp>

namespace json = boost::json;

class file_preview
{
    public:
        // Return number of rows in the file
        // If the error occures with file then return -1
        static size_t get_file_rows_number(const std::string& file_path);

        // Return json array of raw rows from file between from_row_number and from_row_number + rows_number rows
        // If the error occures with file then return emptry json::array
        static json::array get_file_raw_rows(
            const std::string& file_path, 
            size_t from_row_number, 
            size_t rows_number);
};

#endif