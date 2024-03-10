#ifndef FILE_NORMALIZATION_HPP
#define FILE_NORMALIZATION_HPP

// local
#include <logging/logger.hpp>

// internal
#include <filesystem>
#include <format>

class file_normalization
{
    public:
        static bool normalize_newlines(const std::filesystem::path& file_path);

        static void split_file();
};

#endif