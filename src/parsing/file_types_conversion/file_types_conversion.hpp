#ifndef FILE_TYPES_CONVERSION_HPP
#define FILE_TYPES_CONVERSION_HPP

// local
#include <logging/logger.hpp>
#include <parsing/delimiter_finder/delimiter_finder.hpp>

// internal
#include <filesystem>
#include <format>
#include <fstream>

// external
#include <boost/process/system.hpp>
#include <boost/process/io.hpp>

class file_types_conversion
{
    public:
        // Convert file from its type to valid csv format if it is possible
        // If conversion fails then the output file is removed
        // Return true on success, otherwise return false
        static bool convert_file_to_csv(
            const std::filesystem::path& file_path, 
            const std::filesystem::path& output_file_path);

    private:
        // Convert text file to csv by invoking corresponding conversion(sql-like or csv-like)
        // depending on the file type that is determined beforehand
        static bool convert_text_file_to_csv(
            const std::filesystem::path& text_file_path, 
            const std::filesystem::path& csv_file_path);

        // Check if the file represents sql-like file 
        // that is it contains rows as corteges from INSERT statements from sql
        // and consider it sql-like if it contains 80% valid sql-like rows
        static bool is_text_file_sql_like(const std::filesystem::path& text_file_path);

        // Convert sql-like file to csv:
        // Process each row as sql INSERT INTO cortege, clearing it from remainder of sql format,
        // splitting it into fields and validating them with csv format rules
        // Return true on successful conversion, and false if file can't be opened
        static bool convert_sql_like_file_to_csv(
            const std::filesystem::path& sql_like_file_path, 
            const std::filesystem::path& csv_file_path);

        // Determine the number of fields in sql-like file 
        // It processes only a part of file with config::rows_number_to_examine rows
        // Return the most common fields number among processed rows on success or -1 if the file can't be opened
        static size_t determine_fields_number_in_sql_like_file(const std::filesystem::path& sql_like_file_path);

        // Determine the number of fields in csv-like file 
        // It processes only a part of file with config::rows_number_to_examine rows
        // Return the most common fields number among processed rows on success or -1 if the file can't be opened
        static size_t determine_fields_number_in_csv_like_file(
            const std::filesystem::path& csv_like_file_path,
            const std::string& delimiter);

        // Convert csv-like files to valid csv format.
        // Firsly file is being processed as csv but with any delimiter. If something goes wrong and csv format 
        // is violated then try to process file as non-csv-like, it means that table-like structure is still required
        // but corresponding opening and closing double quotes can be omitted so process rows without them.
        // Invalid rows are skipped.
        // Return true on successful conversion, and false if file can't be opened or delimiter or fields number
        // can't be determined 
        static bool convert_csv_like_file_to_csv(
            const std::filesystem::path& text_file_path, 
            const std::filesystem::path& csv_file_path);

        static bool convert_xlsx_to_csv(
            const std::filesystem::path& xlsx_file_path, 
            const std::filesystem::path& csv_file_path);

        // Convert sql dump file to csv:
        // Process CREATE TABLE statement to get fields number and field names to produce header line in csv
        // Process INSERT INTO statement corteges to get actual data, splitting them into fields 
        // and validating obtained fields with csv format rules
        // Return true on successful conversion, and false if sql format is broken somewhere  
        static bool convert_sql_to_csv(
            const std::filesystem::path& sql_file_path, 
            const std::filesystem::path& csv_file_path);
};

#endif