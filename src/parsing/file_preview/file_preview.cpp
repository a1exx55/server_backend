#include <parsing/file_preview/file_preview.hpp>

size_t file_preview::get_file_rows_number(const std::string& file_path)
{
    std::ifstream file{file_path};

    // An error occured while opening the file
    if (!file.is_open())
    {
        return static_cast<size_t>(-1);
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

std::pair<uint8_t, json::array> file_preview::get_file_raw_rows(
    const std::string& file_path, 
    size_t from_row_number, 
    size_t rows_number)
{
    // Invalid parametes as we don't process very large amount of rows or zero rows
    if (rows_number > 10000 || rows_number == 0)
    {
        return {2, {}};
    }

    std::ifstream file{file_path};

    // An error occured while opening the file
    if (!file.is_open())
    {
        return {1, {}};
    }

    // Set buffer size to 1 MB
    const size_t BUFFER_SIZE = 1024 * 1024;
    std::array<char, BUFFER_SIZE> buffer;

    size_t current_row_number = 0, offset = 0, read_bytes;

    // If it is required to read from the start of the file then we don't have to read anything beforehand
    if (from_row_number == 0)
    {
        // Go to the processing of the desired rows
        goto rows_processing;
    }

    // We need to get to the from_row_number row to start getting the actual rows
    while ((read_bytes = file.read(buffer.data(), BUFFER_SIZE).gcount()))
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
                    // Remember where we stopped to process the remainder of the buffer afterward
                    offset = i + 1;

                    // Exit from the two loops to process desired rows
                    goto rows_processing;
                }
            }
        }
    }

    // If we get here then the file is over and from_row_number is bigger 
    // than the actual number of rows so we can't get desired rows
    return {2, {}};

rows_processing:

    // Move the unprocessed remainder of buffer to the beggining
    std::move(buffer.begin() + offset, buffer.begin() + read_bytes, buffer.begin());

    // Read bytes until the buffer is full
    read_bytes += file.read(buffer.data() + read_bytes - offset, BUFFER_SIZE - read_bytes + offset).gcount();

    // Use variable to store position in buffer where the row starts
    size_t row_start_position = 0;
    offset = 0;

    // Reset current_row_number to use it as counter of processed rows
    current_row_number = 0;

    // Count the number of entirely read buffers which actually represents the size of processed rows 
    size_t read_buffers_number = 0;
    
    json::array rows_array;

    do
    {
        // Go through the buffer and count the number of line feeds('\n') which represent rows
        for (size_t i = offset; i < read_bytes; ++i)
        {
            if (buffer[i] == '\n')
            {
                rows_array.emplace_back(json::string{buffer.data() + row_start_position, buffer.data() + i});

                ++current_row_number;

                // We processed the desired number of rows
                if (current_row_number == rows_number)
                {
                    return {0, rows_array};
                }

                // Remember where the next row starts
                row_start_position = i + 1;
            }
        }

        // First condition means that current row can't fit in the whole buffer so we consider it too large
        // Second condition means that we processed more than 1000 buffers and since buffer size is 1 MB
        // so desired rows weight more than 1 GB that is again too large for us
        if (row_start_position == 0 || ++read_buffers_number > 1000)
        {
            return {3, {}};
        }

        // Move the unprocessed remainder of buffer to the beggining
        std::move(buffer.begin() + row_start_position, buffer.end(), buffer.begin());

        // Set offset to to avoid handling already processed part of buffer
        offset = BUFFER_SIZE - row_start_position;
        row_start_position = 0;
    }
    // Read file by chunks of 1 MB into static buffer while there are read bytes 
    while ((read_bytes = file.read(buffer.data() + offset, BUFFER_SIZE - offset).gcount()));

    return {0, rows_array};
}