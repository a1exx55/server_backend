#include <parsing/csv_file_normalization/csv_file_normalization.hpp>

bool csv_file_normalization::normalize_file(const std::filesystem::path& file_path)
{
    // Generate temporary file path to write normalized data in it
    const std::filesystem::path temp_file_path = 
        file_path.parent_path() / std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());

    // Define processing of failed normalization that removes temporary file and returns false as the result of 
    // normalization to invoke this function if we won't be able to normalize csv file
    auto process_failed_normalization = 
        [&temp_file_path]
        {
            try
            {
                std::filesystem::remove(temp_file_path);
            }
            catch (const std::exception& ex)
            {
                LOG_ERROR << ex.what();
            }

            return false;
        };

    std::ifstream input_file{file_path};
    std::ofstream temp_file{temp_file_path};

    if (!input_file.is_open() || !temp_file.is_open())
    {
        return process_failed_normalization();
    }

    // Take buffer size of the number of bytes that is enough to process predefined number of rows to be examined
    const size_t buffer_size = config::rows_number_to_examine * config::max_bytes_number_in_row;
    std::string buffer(buffer_size, char());

    input_file.read(buffer.data(), buffer_size);

    // Since we control reading in buffer manually, we have to resize buffer explicitly 
    // if there was the last reading and we couldn't read the full buffer_size bytes
    // so store value of if we still have to read file
    bool is_reading_pending = true;

    // If we read less bytes than it was required then shrink buffer to actual size of read bytes 
    if (static_cast<size_t>(input_file.gcount()) < buffer_size)
    {
        is_reading_pending = false;
        buffer.resize(static_cast<size_t>(input_file.gcount()));
    }

    std::string_view input_row, field;
    std::string output_row;
    size_t row_start_position = 0, field_start_position, field_end_position,
        newline_position, quotes_number, cr_position, i;
    bool is_field_quoted;
    
    while (true)
    {
        // Look for the newline that is assumed to be the end of the current row(this can be changed later
        // because this line feed might be the part of the actual row)
        newline_position = buffer.find('\n', row_start_position);

        if (newline_position != std::string::npos)
        {
            // Get row's data
            input_row = std::string_view{buffer.begin() + row_start_position, buffer.begin() + newline_position};
        }
        else
        {
            // If we didn't find newline then the file is over - last row is always empty
            break;
        }

        field_start_position = 0;
        field_end_position = 0;

        // Parse row until it is over
        do
        {
            // If field starts with double quote then this field has to be quoted and can contain special 
            // symbols like double quote, comma or line feed so parse the field with all rules
            if (input_row[field_start_position] == '"')
            {
                // Use this flag to process field differently depending on if it is quoted
                is_field_quoted = true;

                field_end_position = field_start_position;

                // Quoted field can contain double quotes but they have to be doubled so we have to skip even number
                // of double quotes until we find the odd one to find the end of the field 
                do
                {
                    // If we couldn't find closing double quote then the row is incomplete(newline is in row)
                    // so we have to expand the row to the next newline until we find the closing quote
                    while ((field_end_position = input_row.find('"', field_end_position + 1)) == std::string::npos) 
                    {           
                        // Set field_end_position to the end of current input_row because it will be expanded further
                        // and we have to know from what position to look for the closing quote
                        field_end_position = input_row.size();

                        newline_position = buffer.find('\n', newline_position + 1);

                        if (newline_position != std::string::npos)
                        {
                            input_row = std::string_view{
                                buffer.begin() + row_start_position, 
                                buffer.begin() + newline_position};
                        }
                        else
                        {
                            input_row = std::string_view{
                                buffer.begin() + row_start_position, 
                                buffer.end()};
                            
                            newline_position = buffer.size() - 1;
                        }
                    }
                    
                    quotes_number = 1;
                    ++field_end_position;

                    while (input_row[field_end_position] == '"')
                    {
                        ++quotes_number;
                        ++field_end_position;
                    }
                } 
                while (quotes_number % 2 == 0);

                // Get field without quotes
                field = input_row.substr(field_start_position + 1, field_end_position - field_start_position - 2);
            }
            // If field doesn't start with double quote then it is regular string and it can't contain 
            // special symbols that are described above
            else
            {
                // Use this flag to process field differently depending on if it is quoted
                is_field_quoted = false;

                // The end of the field can be definitely determined by the delimiter
                field_end_position = input_row.find(',', field_start_position);

                field = input_row.substr(field_start_position, field_end_position - field_start_position);
            }

            // Trim whitespaces at the beggining and at the end of the field
            for (i = 0; i < field.size(); ++i)
            {
                if (!isspace(field[i]))
                {
                    break;
                }
            }
            field.remove_prefix(i);

            for (i = field.size() - 1; i != static_cast<size_t>(-1); --i)
            {
                if (!isspace(field[i]))
                {
                    break;
                }
            }
            field.remove_suffix(field.size() - i - 1);

            // Remember where the field starts in output_row to modify it in place afterward  
            field_start_position = output_row.size();

            // Look for the \r and skip them in output row
            while ((cr_position = field.find('\r')) != std::string::npos)
            {
                output_row += field.substr(0, cr_position);
                field.remove_prefix(cr_position + 1);
            }

            output_row += field;

            if (is_field_quoted)
            {
                // Replace all \n to \t in field to simlify subsequent iterating through rows using \n as newline
                for (i = field_start_position; i < output_row.size(); ++i)
                {
                    if (output_row[i] == '\n')
                    {
                        output_row[i] = '\t';
                    }
                }

                // Append opening escaping double quote of the field
                output_row.insert(field_start_position, "\"");

                // Append closing escaping double quote of the field and comma as delimiter 
                output_row += "\",";
            }
            else
            {
                // Append comma as delimiter
                output_row += ',';
            }
            
            field_start_position = field_end_position + 1;
        }
        while (field_end_position < input_row.size());

        // Replace the last comma with line feed as the end of row
        output_row.back() = '\n';

        // Write the row into csv file
        temp_file.write(output_row.c_str(), output_row.size());

        // Move position to the beginning of the next row
        row_start_position = newline_position + 1;

        // We know that each row has to be less than config::max_bytes_number_in_row bytes 
        // so if it is left less in buffer and we didn't read all the file 
        // then move the remainder to the beginning and read up to buffer_size again 
        if (is_reading_pending && buffer_size - row_start_position < config::max_bytes_number_in_row)
        {
            std::move(buffer.begin() + row_start_position, buffer.end(), buffer.begin());

            input_file.read(buffer.data() + buffer_size - row_start_position, row_start_position);

            // If we read less bytes than it was required then
            // shrink buffer to actual size of moved remainder and read bytes 
            if (static_cast<size_t>(input_file.gcount()) < row_start_position)
            {
                is_reading_pending = false;
                buffer.resize(buffer_size - row_start_position + static_cast<size_t>(input_file.gcount()));
            }

            row_start_position = 0;
        }
        
        // Clear just processed csv row to use it for the next one
        output_row.clear();
    }

    try
    {
        std::filesystem::rename(temp_file_path, file_path);
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();

        return process_failed_normalization();
    }

    return true;
}

std::vector<std::filesystem::path> csv_file_normalization::split_file(
    const std::filesystem::path &file_path, 
    const std::filesystem::path& output_folder,
    size_t rows_number_in_each_file)
{
    // Use this vector to store output file paths to return it as the result of function
    std::vector<std::filesystem::path> output_file_paths;

    // Define processing of failed splitting that removes output files and returns empty vector as the result of 
    // splitting to invoke this function if we won't be able to split csv file
    auto process_failed_splitting = 
        [&output_file_paths]() -> std::vector<std::filesystem::path>
        {
            try
            {
                for (const auto& output_file_path : output_file_paths)
                {
                    std::filesystem::remove(output_file_path);
                }
            }
            catch (const std::exception& ex)
            {
                LOG_ERROR << ex.what();
            }

            return {};
        };


    // Define generation of each output file name with its path that is time since epoch in nanoseconds. 
    // It allows us to get random file name with lack of collisions  
    auto generate_file_path = 
        [&output_folder]() -> std::filesystem::path
        {
            return output_folder / 
                std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()).append(".csv");
        };

    // Generate first file path and store it in output_file_paths
    std::filesystem::path current_file_path = generate_file_path();
    output_file_paths.emplace_back(current_file_path);

    std::ifstream input_file{file_path};
    std::ofstream current_file{current_file_path};

    if (!input_file.is_open() || !current_file.is_open())
    {
        return process_failed_splitting();
    }

    // Take buffer size of the number of bytes that is enough to process predefined number of rows to be examined
    const size_t buffer_size = config::rows_number_to_examine * config::max_bytes_number_in_row;
    std::string buffer(buffer_size, char());

    input_file.read(buffer.data(), buffer_size);

    // Since we control reading in buffer manually, we have to resize buffer explicitly 
    // if there was the last reading and we couldn't read the full buffer_size bytes
    // so store value of if we still have to read file
    bool is_reading_pending = true;

    // If we read less bytes than it was required then shrink buffer to actual size of read bytes 
    if (static_cast<size_t>(input_file.gcount()) < buffer_size)
    {
        is_reading_pending = false;
        buffer.resize(static_cast<size_t>(input_file.gcount()));
    }

    size_t newline_position, row_start_position = 0, rows_number_in_current_file = 0;

    while (true)
    {
        // Look for the newline that is the end of the current row
        newline_position = buffer.find('\n', row_start_position);

        // If we didn't find newline then the file is over - last row is always empty
        if (newline_position == std::string::npos)
        {
            break;
        }

        // If we have written rows_number_in_each_file in current file then we have to end up with this file
        // and generate and open the new one to write next rows there
        if (rows_number_in_current_file++ == rows_number_in_each_file)
        {
            current_file.close();

            current_file_path = generate_file_path();
            output_file_paths.emplace_back(current_file_path);

            current_file.open(current_file_path);

            if (!current_file.is_open())
            {
                return process_failed_splitting();
            }

            // Assign rows_number_in_current_file to 1 because we will write the first row right after this statement
            rows_number_in_current_file = 1;
        }

        // Write current row with newline inclusive in current file 
        current_file.write(buffer.data() + row_start_position, newline_position - row_start_position + 1);

        row_start_position = newline_position + 1;

        // We know that each row has to be less than config::max_bytes_number_in_row bytes 
        // so if it is left less in buffer and we didn't read all the file 
        // then move the remainder to the beginning and read up to buffer_size again 
        if (is_reading_pending && buffer_size - row_start_position < config::max_bytes_number_in_row)
        {
            std::move(buffer.begin() + row_start_position, buffer.end(), buffer.begin());

            input_file.read(buffer.data() + buffer_size - row_start_position, row_start_position);

            // If we read less bytes than it was required then
            // shrink buffer to actual size of moved remainder and read bytes 
            if (static_cast<size_t>(input_file.gcount()) < row_start_position)
            {
                is_reading_pending = false;
                buffer.resize(buffer_size - row_start_position + static_cast<size_t>(input_file.gcount()));
            }

            row_start_position = 0;
        }
    } 
    
    // If the input file fits in one file with up to rows_number_in_each_file rows
    // then remove generated file and return original file
    if (output_file_paths.size() == 1)
    {
        try
        {
            std::filesystem::remove(output_file_paths[0]);
        }
        catch (const std::exception& ex)
        {
            LOG_ERROR << ex.what();
        }

        return {file_path};
    }

    return output_file_paths;
}