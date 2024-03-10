#include <parsing/file_normalization/file_normalization.hpp>

bool file_normalization::normalize_newlines(const std::filesystem::path& file_path)
{
    // Generate temporary file path to write normalized data in it
    const std::filesystem::path new_file_path = 
        std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());

    // Define removal of input file to invoke it if we won't be able to convert sql file to csv
    auto remove_new_file = 
        [&new_file_path]
        {
            try
            {
                std::filesystem::remove(new_file_path);
            }
            catch (const std::exception& ex)
            {
                LOG_ERROR << ex.what();
            }
        };

    std::ifstream input_file{file_path};
    std::ofstream new_file{
        file_path.parent_path() / 
        std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count())};

    if (!input_file.is_open() || !new_file.is_open())
    {
        remove_new_file();

        return false;
    }

    const size_t BUFFER_SIZE = 1024 * 1024;
    std::string input_buffer(BUFFER_SIZE, char()), output_buffer;
    size_t start_position = 0, end_position, quotes_number = 0;

    // Read file by chunks of 1 MB into static buffer while there are read bytes 
    while (size_t read_bytes = input_file.read(input_buffer.data(), BUFFER_SIZE).gcount())
    {
        while ((end_position = input_buffer.find('\n', start_position)) != std::string::npos)
        {
            if (end_position && input_buffer[end_position - 1] == '\r')
            {
                std::move(
                    input_buffer.begin() + start_position, 
                    input_buffer.begin() + end_position - 1, 
                    output_buffer.end());

                output_buffer += '\n';
            }
            else
            {
                std::move(
                    input_buffer.begin() + start_position, 
                    input_buffer.begin() + end_position + 1, 
                    output_buffer.end());
            }

            start_position = end_position + 1;
        }

        std::move(
            input_buffer.begin() + start_position, 
            input_buffer.end(), 
            output_buffer.end());

        while ((start_position = input_buffer.find('"', end_position + 1)) != std::string::npos)
        {
                end_position = start_position + 1;
            do
            {
                if ((end_position = input_buffer.find('"', end_position + 1)) == std::string::npos)
                {
                    remove_new_file();

                    return false;
                }

                ++end_position;
                quotes_number = 1;

                while (end_position < read_bytes && input_buffer[end_position] == '"')
                {
                    ++end_position;
                    ++quotes_number;
                }
            } 
            while (quotes_number % 2 == 0);
        }
    }
}