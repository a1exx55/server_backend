#ifndef FILE_TYPES_CONVERSION_HPP
#define FILE_TYPES_CONVERSION_HPP

// local
#include <logging/logger.hpp>

// internal
#include <filesystem>
#include <format>

// external
#include <boost/process/system.hpp>
#include <boost/process/io.hpp>

class file_types_conversion
{
    public:
        // Convert file from given type to csv if it is possible
        // If the conversion is succeed then the given file is removed, nothing happens on failure
        // Return true on success, otherwise return false
        static bool convert_file_to_csv(
            std::string_view file_type,
            const std::filesystem::path& file_path, 
            const std::filesystem::path& output_file_path);

    private:        
        static bool convert_txt_to_csv(
            const std::filesystem::path& txt_file_path, 
            const std::filesystem::path& csv_file_path);

        static bool convert_xlsx_to_csv(
            const std::filesystem::path& xlsx_file_path, 
            const std::filesystem::path& csv_file_path);

        static bool convert_sql_to_csv(
            const std::filesystem::path& sql_file_path, 
            const std::filesystem::path& csv_file_path);
};

#endif