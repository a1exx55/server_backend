#include <parsing/file_preview/file_preview.hpp>

size_t file_preview::get_file_rows_number(const std::string& file_path)
{
    std::ifstream file{file_path};

    // An error occured while opening the file
    if (!file.is_open())
    {
        LOG_ERROR << std::format(
            "Could not open file to count rows number: {}",
            file_path);

        return -1;
    }

    const size_t BUFFER_SIZE = 1024 * 1024;
    std::array<char, BUFFER_SIZE> buffer;

    size_t rows_number = 0;

    // Read file by chunks of 1 MB into static buffer while there are read bytes 
    while (size_t read_bytes = file.read(buffer.data(), BUFFER_SIZE).gcount())
    {
        // Go through the buffer and count the number of line feeds('\n') which represent rows
        for (size_t i = 0; i < read_bytes; ++i)
        {
            if (buffer[i] == '\n')
            {
                ++rows_number;
            }
        }
    }

    return rows_number;
}

json::array file_preview::get_file_raw_rows(
    const std::string& file_path, 
    size_t from_row_number, 
    size_t rows_number)
{
    // Don't process very large amount of rows or zero rows
    if (rows_number > 10000 || rows_number == 0)
    {
        return {};
    }

    std::ifstream file{file_path};

    // An error occured while opening the file
    if (!file.is_open())
    {
        LOG_ERROR << std::format(
            "Could not open file to get raw rows: {}",
            file_path);

        return {};
    }

    const size_t BUFFER_SIZE = 1024 * 1024;
    std::array<char, BUFFER_SIZE> buffer;

    size_t current_row_number = 0, offset = 0, read_bytes;

    // If it is required to read from the start of the file then we don't have to read anything beforehand
    if (from_row_number == 0)
    {
        // Go to the processing of the desired rows
        goto exit_flag;
    }

    // We need to get to the from_row_number to start getting the actual rows
    while (read_bytes = file.read(buffer.data(), BUFFER_SIZE).gcount())
    {
        // Go through the buffer and count the number of line feeds('\n') which represent rows
        for (size_t i = 0; i < read_bytes; ++i)
        {
            if (buffer[i] == '\n')
            {
                ++current_row_number;

                // We reached the start row so stop counting rows
                if (current_row_number == from_row_number)
                {
                    offset = BUFFER_SIZE - i - 1;

                    // Exit from the two loops to process desired rows
                    goto exit_flag;
                }
            }
        }
    }

    // If we get here then the file is over and from_row_number is bigger 
    // than the actual number of rows so we can't get desired rows
    return {};

    exit_flag:
 
    file.seekg(file.tellg() - static_cast<std::streamoff>(offset));

    size_t row_start_position = 0;
    offset = 0;

    // Reset current_row_number to use it as counter of processed rows
    current_row_number = 0;

    // Move the remainder of unprocessed buffer to the beggining
    // std::move(buffer.begin() + offset, buffer.end(), buffer.begin());

    json::array rows_array;

    // Read file by chunks of 1 MB into static buffer while there are read bytes 
    while (read_bytes = file.read(buffer.data() + offset, BUFFER_SIZE - offset).gcount())
    {
        // Go through the buffer and count the number of line feeds('\n') which represent rows
        for (size_t i = offset; i < read_bytes; ++i)
        {
            if (buffer[i] == '\n')
            {
                rows_array.emplace_back(json::string{buffer.data() + row_start_position, buffer.data() + i});

                ++current_row_number;

                // We reached the start row so stop counting rows
                if (current_row_number == rows_number)
                {
                    return rows_array;
                }

                row_start_position = i + 1;
            }
        }

        if (row_start_position == 0)
        {
            return {};
        }

        // Move the remainder of unprocessed buffer to the beggining
        std::move(buffer.begin() + row_start_position, buffer.end(), buffer.begin());

        offset = BUFFER_SIZE - row_start_position;
        row_start_position = 0;
    }

    return rows_array;
}