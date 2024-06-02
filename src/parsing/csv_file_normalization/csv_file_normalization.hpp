#ifndef CSV_FILE_NORMALIZATION_HPP
#define CSV_FILE_NORMALIZATION_HPP

// local
#include <logging/logger.hpp>
#include <config.hpp>

// internal
#include <filesystem>
#include <format>
#include <fstream>

class csv_file_normalization
{
    public:
        // Normalize csv file by deleting \r as useless symbols because we use only \n as newlines, 
        // trimming fields(deleting leading and trailing spaces) and replacing \n to \t in fields 
        // to simlify iterating through rows using \n as newlines for subsequent parsing
        // Return true on successful normalization, and false if file can't be opened or OS error occurred
        static bool normalize_file(const std::filesystem::path& file_path);

        // Split file by rows into some files so that each file except the last one contains 
        // exactly rows_number_in_each_file rows. 
        // Output files are stored in output_folder and have names that are time since epoch in nanoseconds
        // with .csv extension so it is assumed that there won't be any name collisions.
        // If given file has less than rows_number_in_each_file rows then no files will be created 
        // Return vector of paths of output files
        static std::vector<std::filesystem::path> split_file(
            const std::filesystem::path& file_path, 
            const std::filesystem::path& output_folder,
            size_t rows_number_in_each_file);
};

#endif