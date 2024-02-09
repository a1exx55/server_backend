#ifndef FILE_PREVIEW_HPP
#define FILE_PREVIEW_HPP

// internal
#include <filesystem>

// external
#include <boost/json.hpp>

namespace json = boost::json;

class file_preview
{
    public:
        // Return number of rows in the file
        // If the error occured while opening the file then return static_cast<size_t>(-1)
        static size_t get_file_rows_number(const std::string& file_path);

        /* Extract raw rows to json array from file between from_row_number and from_row_number + rows_number rows
        from_row_number must be less than the rows number in file, rows_number must be within (0, 10000]
        Return a pair of error code and json array of raw rows
        On success return 0 and actual json array
        On failure return error code and empty json array
        Error codes:
        1 - error occured while opening file
        2 - invalid row parameters provided
        3 - too large data to preview - either single row weights more than 1 MB or total rows weights more than 1 GB */
        static std::pair<uint8_t, json::array> get_file_raw_rows(
            const std::string& file_path, 
            size_t from_row_number, 
            size_t rows_number);
};

#endif