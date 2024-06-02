#include <parsing/file_types_conversion/file_types_conversion.hpp>

bool file_types_conversion::convert_file_to_csv(
    const std::filesystem::path& file_path, 
    const std::filesystem::path& output_file_path)
{
    std::string file_extension = file_path.extension().string();

    if (file_extension == ".csv" || file_extension == ".txt")
    {
        return convert_text_file_to_csv(file_path, output_file_path);
    }
    else if (file_extension == ".xlsx")
    {
        return convert_xlsx_to_csv(file_path, output_file_path);
    }
    else if (file_extension == ".sql")
    {
        return convert_sql_to_csv(file_path, output_file_path);
    }
    else
    {
        return false;
    }
}

bool file_types_conversion::convert_text_file_to_csv(
    const std::filesystem::path& text_file_path, 
    const std::filesystem::path& csv_file_path)
{
    if (is_text_file_sql_like(text_file_path))
    {
        return convert_sql_like_file_to_csv(text_file_path, csv_file_path);
    }
    else
    {
        return convert_csv_like_file_to_csv(text_file_path, csv_file_path);
    }
}

bool file_types_conversion::is_text_file_sql_like(const std::filesystem::path& text_file_path)
{
    std::ifstream text_file{text_file_path};

    // Return false if couldn't open file as it will be opened 
    // in actual conversion and reported impossibility to convert file anyway
    if (!text_file.is_open())
    {
        return false;
    }

    // Take buffer size of the number of bytes that is enough to process predefined number of rows to be examined
    const size_t buffer_size = config::ROWS_NUMBER_TO_EXAMINE * config::MAX_BYTES_NUMBER_IN_ROW;
    std::string buffer(buffer_size, char());

    text_file.read(buffer.data(), buffer_size);

    // If file is less than we expected then resize buffer to the amount of read bytes
    if (static_cast<size_t>(text_file.gcount()) < buffer_size)
    {
        buffer.resize(static_cast<size_t>(text_file.gcount()));
    }

    size_t start_position = 0, end_position, newline_position = 0, backslashes_number, invalid_rows_number = 0, i;
    double numeric_field;
    std::string_view row, field;

    // Process rows until we process predefined number or file ends up
    for (i = 0; i < config::ROWS_NUMBER_TO_EXAMINE && newline_position != std::string::npos; ++i)
    {
        // Since in sql like files newlines can't be in field values(they are represented as two characters \n instead)
        // then we can just split file rows by line feeds
        newline_position = buffer.find('\n', start_position);

        // Skip empty rows
        if (start_position == newline_position)
        {
            // Don't count empty row as processed
            --i;
            goto next_row_processing;
        }

        // Get row's data
        if (newline_position != std::string::npos)
        {
            row = std::string_view{buffer.begin() + start_position, buffer.begin() + newline_position};
        }
        else
        {
            row = std::string_view{buffer.begin() + start_position, buffer.end()};
        }

        // There can be opening bracket as the remainder of INSERT cortege in sql
        if (row.front() == '(')
        {
            row.remove_prefix(1);
        }

        // There can be INSERT INTO statement as the remainder of actual sql cortege 
        if (row.starts_with("INSERT INTO"))
        {
            // With the INSERT INTO there has to be opening bracket of sql cortege
            if ((start_position = row.find('(')) == std::string::npos) 
            {
                ++invalid_rows_number;
                goto next_row_processing;
            }

            row.remove_prefix(start_position + 1);
        }

        // Newline can be presented as \r\n so we have to remove the useless \r
        if (row.back() == '\r')
        {
            row.remove_suffix(1);
        }

        // There can be comma in the end of the row
        if (row.back() == ',')
        {
            row.remove_suffix(1);
        }

        // There can be closing bracket as the remainder of INSERT cortege in sql
        if (row.back() == ')')
        {
            row.remove_suffix(1);
        }

        // Some rows can end with bracket and semicolon as the remainder of the end of INSERT statement
        if (row.ends_with(");"))
        {
            row.remove_suffix(2);
        }

        start_position = 0;

        // Process field values until the end of row or the error occurrence
        do
        {
            // If the field value starts with single quote then it is string value
            if (row[start_position] == '\'')
            {
                end_position = start_position;

                // In string field there can be single quote but it can be escaped with odd number of backslashes
                // because of escaped backslashes: ///' -> /' that is just quote within field,
                // but //' is a escaped backslash with quote that is the end of field
                // so search for single quote with even number of preceding backslashes to find the end of field
                do
                {
                    // Couldn't find closing single quote
                    if ((end_position = row.find('\'', end_position + 1)) == std::string::npos) 
                    {
                        ++invalid_rows_number;
                        goto next_row_processing;
                    }
                    
                    backslashes_number = 0;

                    while (row[end_position - backslashes_number - 1] == '\\')
                    {
                        ++backslashes_number;
                    }
                } 
                while (backslashes_number % 2 == 1);

                // Move to the next field
                start_position = end_position + 2; 
            }
            // If the field value is not string then either it is NULL or number(bool is represented by 0 or 1)
            else
            {
                // Look for the end of the field
                end_position = row.find(',', start_position);

                // Get the field value
                field = row.substr(start_position, end_position - start_position);

                // Check if the field is NULL
                if (field != "NULL")
                {
                    // If it is not NULL then try to convert it to the number
                    if (std::from_chars(field.begin(), field.end(), numeric_field).ptr != field.end())
                    {
                        ++invalid_rows_number;
                        goto next_row_processing;
                    }
                }

                // Move to the next field
                start_position = end_position + 1; 
            }
        }
        while (end_position < row.size() - 1);

        // Use this label to move here if we found invalid row to process next row immediately
        next_row_processing:

        // Move to the next row
        start_position = newline_position + 1;
    }

    // We assume that there can be invalid rows but we allow only 20% of the total to still remain sql like file
    if (static_cast<double>(invalid_rows_number) / i < 0.2)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool file_types_conversion::convert_sql_like_file_to_csv(
    const std::filesystem::path& sql_like_file_path, 
    const std::filesystem::path& csv_file_path)
{
    // Define processing of failed conversion that removes csv file and returns false as the result of conversion
    // to invoke this function if we won't be able to convert sql like file to csv
    auto process_failed_conversion = 
        [&csv_file_path]
        {
            try
            {
                std::filesystem::remove(csv_file_path);
            }
            catch (const std::exception& ex)
            {
                LOG_ERROR << ex.what();
            }

            return false;
        };

    std::ifstream sql_like_file{sql_like_file_path};
    std::ofstream csv_file{csv_file_path};

    if (!sql_like_file.is_open() || !csv_file.is_open())
    {
        return process_failed_conversion();
    }

    // Determine the fields number of the file
    size_t fields_number = determine_fields_number_in_sql_like_file(sql_like_file_path);

    // Couldn't open the file
    if (fields_number == static_cast<size_t>(-1))
    {
        return process_failed_conversion();
    }

    // Take buffer size of the number of bytes that is enough to process predefined number of rows to be examined
    const size_t buffer_size = config::ROWS_NUMBER_TO_EXAMINE * config::MAX_BYTES_NUMBER_IN_ROW;
    std::string buffer(buffer_size, char());

    sql_like_file.read(buffer.data(), buffer_size);

    // Since we control reading in buffer manually, we have to resize buffer explicitly 
    // if there was the last reading and we couldn't read the full buffer_size bytes
    // so store value of if we still have to read file
    bool is_reading_pending = true;

    // If we read less bytes than it was required then exchange flag value
    // and shrink buffer to actual size of read bytes 
    if (static_cast<size_t>(sql_like_file.gcount()) < buffer_size)
    {
        is_reading_pending = false;
        buffer.resize(static_cast<size_t>(sql_like_file.gcount()));
    }

    std::string_view sql_row, field_view;
    std::string csv_row, field;
    size_t backslashes_number, newline_position, start_position = 0, end_position, i;
    double numeric_field;
    bool has_to_be_field_quoted;

    // Process rows in file while there are by getting each row, splitting it into the fields, 
    // converting them to the csv format and writing completed csv rows to the output file
    do
    {
        // Since in sql like files newlines can't be in field values(they are represented as two characters \n instead)
        // then we can just split file rows by line feeds
        newline_position = buffer.find('\n', start_position);

        // Skip empty rows
        if (start_position == newline_position)
        {
            goto next_row_processing;
        }

        // Get row's data
        if (newline_position != std::string::npos)
        {
            sql_row = std::string_view{buffer.begin() + start_position, buffer.begin() + newline_position};
        }
        else
        {
            // If we didn't read the whole file then we have to find the newline so
            // if there are no one then the row can't fit in our buffer and we can't process it 
            if (is_reading_pending)
            {
                goto next_row_processing;
            }

            sql_row = std::string_view{buffer.begin() + start_position, buffer.end()};
        }

        // There can be opening bracket as the remainder of INSERT cortege in sql
        if (sql_row.front() == '(')
        {
            sql_row.remove_prefix(1);
        }

        // There can be INSERT INTO statement as the remainder of actual sql cortege 
        if (sql_row.starts_with("INSERT INTO"))
        {
            // With the INSERT INTO there has to be opening bracket of sql cortege
            if ((start_position = sql_row.find('(')) == std::string::npos) 
            {
                goto next_row_processing;
            }

            sql_row.remove_prefix(start_position + 1);
        }

        // Newline can be presented as \r\n so we have to remove the useless \r
        if (sql_row.back() == '\r')
        {
            sql_row.remove_suffix(1);
        }

        // There can be comma in the end of the row
        if (sql_row.back() == ',')
        {
            sql_row.remove_suffix(1);
        }

        // There can be closing bracket as the remainder of INSERT cortege in sql
        if (sql_row.back() == ')')
        {
            sql_row.remove_suffix(1);
        }

        // Some rows can end with bracket and semicolon as the remainder of the end of INSERT statement
        if (sql_row.ends_with(");"))
        {
            sql_row.remove_suffix(2);
        }

        start_position = 0;
        end_position = 0;

        // Parse sql row while we don't process all fields or the row is over
        for (i = 0; i < fields_number && end_position < sql_row.size() - 1; ++i)
        {
            // If field value starts with single quote then it is string
            if (sql_row[start_position] == '\'')
            {
                end_position = start_position;

                // In string field there can be single quote but it can be escaped with odd number of backslashes
                // because of escaped backslashes: ///' -> /' that is just quote within field,
                // but //' is a escaped backslash with quote that is the end of field
                // so search for single quote with even number of preceding backslashes to find the end of field
                do
                {
                    if ((end_position = sql_row.find('\'', end_position + 1)) == std::string::npos) 
                    {
                        goto next_row_processing;
                    }
                    
                    backslashes_number = 0;

                    while (sql_row[end_position - backslashes_number - 1] == '\\')
                    {
                        ++backslashes_number;
                    }
                } 
                while (backslashes_number % 2 == 1);
                                
                // Get string field value between single quotes
                field = sql_row.substr(start_position + 1, end_position - start_position - 1);

                // Move to the data after the field value to parse
                start_position = end_position + 2;

                // Not escaped first double quote is forbidden so the row is invalid 
                if (field.front() == '"')
                {
                    goto next_row_processing;
                }

                // Parse special sql sequences starting with backslash
                // by replacing them with appropriate symbols in csv
                for (size_t j = field.size() - 1; j > 0 && j < field.size(); --j)
                {
                    // Double quote has to be doubled in csv so just replace backslash with double qoute
                    if (field[j] == '"')
                    {
                        if (field[j - 1] == '\\')
                        {
                            field[j - 1] = '"';
                            --j;
                        }
                        else
                        {
                            goto next_row_processing;
                        }
                    }
                    // Single quote and backslash are reqular symbols is csv so just erase first backslash
                    else if (field[j] == '\'' || field[j] == '\\')
                    {
                        if (field[j - 1] == '\\')
                        {
                            field.erase(j - 1, 1);
                            --j;
                        }
                        else
                        {
                            goto next_row_processing;
                        }
                    }
                    // Replace \n string with actual line feed character that is newline
                    else if (field[j] == 'n' && field[j - 1] == '\\')
                    {
                        field.replace(j - 1, 2, "\n");
                        --j;
                    }
                    // Erase \r as it is useless
                    else if (field[j] == 'r' && field[j - 1] == '\\')
                    {
                        field.erase(j - 1, 2);
                        --j;
                    }
                }

                has_to_be_field_quoted = false;

                // In csv format field has to be double quoted if it contains comma, double quote or line feed 
                for (auto symbol : field)
                {
                    if (symbol == ',' || symbol == '"' || symbol == '\n')
                    {
                        has_to_be_field_quoted = true;

                        break;
                    }
                }

                if (has_to_be_field_quoted)
                {
                    // Append double quotes to the beginning and to the end of the string
                    csv_row += '"' + field + "\",";
                }
                else
                {
                    csv_row += field + ',';
                }
            }
            // If the field value is not string then either it is NULL or number(bool is represented by 0 or 1)
            else
            {
                end_position = sql_row.find(',', start_position);

                // Get the field value
                field_view = sql_row.substr(start_position, end_position - start_position);

                // Check if the field is NULL
                if (field_view != "NULL")
                {
                    // If it is not NULL then try to convert it to the number
                    if (std::from_chars(field_view.begin(), field_view.end(), numeric_field).ptr != field_view.end())
                    {
                        goto next_row_processing;
                    }
                }

                // Append field value and comma to the output row
                csv_row.append(field_view);
                csv_row += ',';

                // Move to the data after the field value to parse
                start_position = end_position + 1;
            }
        }

        // Skip the row if it contains invalid fields number or it was not entirely processed
        if (i != fields_number || end_position < sql_row.size() - 1)
        {
            goto next_row_processing;
        }

        // Replace the last comma with line feed as the end of row
        csv_row.back() = '\n';

        // Write the row into csv file
        csv_file.write(csv_row.c_str(), csv_row.size());

        // Use this label to move here if we found invalid row to process next row immediately
        next_row_processing:

        if (newline_position != std::string::npos)
        {
            // Move position to the beginning of the next row
            start_position = newline_position + 1;
        }
        else
        {
            // Assign start position to the buffer size to erase the incomplete row from the buffer further
            start_position = buffer_size;
        }

        // We assume that each row has to be less than config::MAX_BYTES_NUMBER_IN_ROW bytes 
        // so if it is left less in buffer and we didn't read all the file 
        // then move the remainder to the beginning and read up to buffer_size again 
        if (is_reading_pending && buffer_size - start_position < config::MAX_BYTES_NUMBER_IN_ROW)
        {
            std::move(buffer.begin() + start_position, buffer.end(), buffer.begin());

            sql_like_file.read(buffer.data() + buffer_size - start_position, start_position);

            // If we read less bytes than it was required then exchange flag value
            // and shrink buffer to actual size of moved remainder and read bytes 
            if (static_cast<size_t>(sql_like_file.gcount()) < start_position)
            {
                is_reading_pending = false;
                buffer.resize(buffer_size - start_position + static_cast<size_t>(sql_like_file.gcount()));
            }

            // If newline position was found previously then start the next row from the beginnig of the buffer
            if (newline_position != std::string::npos)
            {
                start_position = 0;
            }
            // If newline position was not found then the previous row was incomplete so
            // we have to find the beginning of the new row in the updated buffer
            else
            {
                newline_position = buffer.find('\n');

                // If the beginning of the new row was found and the remaining buffer is enough for that row
                // then just assign found beginning to the start position
                if (newline_position != std::string::npos && 
                    buffer_size - newline_position - 1 >= config::MAX_BYTES_NUMBER_IN_ROW)
                {
                    start_position = newline_position + 1;
                }
                // If the whole buffer still doesn't contain newline then repeat erasing the data and 
                // reading the new one until either we find the valid row or the file ends up
                else
                {
                    goto next_row_processing;
                }
            }
        }

        // Clear just processed csv row to use it for the next one
        csv_row.clear();
    }
    while (is_reading_pending || newline_position != std::string::npos);

    return true;
}

size_t file_types_conversion::determine_fields_number_in_sql_like_file(
    const std::filesystem::path& sql_like_file_path)
{
    std::ifstream sql_like_file{sql_like_file_path};

    if (!sql_like_file.is_open())
    {
        return static_cast<size_t>(-1);
    }

    // Take buffer size of the number of bytes that is enough to process predefined number of rows to be examined
    const size_t buffer_size = config::ROWS_NUMBER_TO_EXAMINE * config::MAX_BYTES_NUMBER_IN_ROW;
    std::string buffer(buffer_size, char());

    sql_like_file.read(buffer.data(), buffer_size);

    // If file is less than we expected then resize buffer to the amount of read bytes
    if (static_cast<size_t>(sql_like_file.gcount()) < buffer_size)
    {
        buffer.resize(static_cast<size_t>(sql_like_file.gcount()));
    }

    size_t start_position = 0, end_position, newline_position = 0, backslashes_number, fields_number;
    std::string_view row, field;
    double numeric_field;
    // Count a number of different field numbers to know the most common fields number
    std::unordered_map<size_t, size_t> different_field_numbers;

    // Process rows until we process predefined number or file ends up
    for (size_t i = 0; i < config::ROWS_NUMBER_TO_EXAMINE && newline_position != std::string::npos; ++i)
    {
        // Reset fields number to count it for each row
        fields_number = 0;

        // Since in sql like files newlines can't be in field values(they are represented as two characters \n instead)
        // then we can just split file rows by line feeds
        newline_position = buffer.find('\n', start_position);

        // Skip empty rows
        if (start_position == newline_position)
        {
            // Don't count empty row as processed
            --i;
            goto next_row_processing;
        }

        // Get row's data
        if (newline_position != std::string::npos)
        {
            row = std::string_view{buffer.begin() + start_position, buffer.begin() + newline_position};
        }
        else
        {
            row = std::string_view{buffer.begin() + start_position, buffer.end()};
        }

        // There can be opening bracket as the remainder of INSERT cortege in sql
        if (row.front() == '(')
        {
            row.remove_prefix(1);
        }

        // There can be INSERT INTO statement as the remainder of actual sql cortege 
        if (row.starts_with("INSERT INTO"))
        {
            // With the INSERT INTO there has to be opening bracket of sql cortege
            if ((start_position = row.find('(')) == std::string::npos) 
            {
                // Don't count invalid row as processed
                --i;
                goto next_row_processing;
            }

            row.remove_prefix(start_position + 1);
        }

        // Newline can be presented as \r\n so we have to remove the useless \r
        if (row.back() == '\r')
        {
            row.remove_suffix(1);
        }

        // There can be comma in the end of the row
        if (row.back() == ',')
        {
            row.remove_suffix(1);
        }

        // There can be closing bracket as the remainder of INSERT cortege in sql
        if (row.back() == ')')
        {
            row.remove_suffix(1);
        }

        // Some rows can end with bracket and semicolon as the remainder of the end of INSERT statement
        if (row.ends_with(");"))
        {
            row.remove_suffix(2);
        }

        start_position = 0;

        // Process field values until the end of row or the error occurrence
        do
        {
            // If the field value starts with single quote then it is string value
            if (row[start_position] == '\'')
            {
                end_position = start_position;

                // In string field there can be single quote but it can be escaped with odd number of backslashes
                // because of escaped backslashes: ///' -> /' that is just quote within field,
                // but //' is a escaped backslash with quote that is the end of field
                // so search for single quote with even number of preceding backslashes to find the end of field
                do
                {
                    // Couldn't find closing single quote
                    if ((end_position = row.find('\'', end_position + 1)) == std::string::npos) 
                    {                        
                        // Don't count invalid row as processed
                        --i;
                       goto next_row_processing;
                    }
                    
                    backslashes_number = 0;

                    while (row[end_position - backslashes_number - 1] == '\\')
                    {
                        ++backslashes_number;
                    }
                } 
                while (backslashes_number % 2 == 1);

                ++fields_number;

                // Move to the next field
                start_position = end_position + 2; 
            }
            // If the field value is not string then either it is NULL or number(bool is represented by 0 or 1)
            else
            {
                // Look for the end of the field
                end_position = row.find(',', start_position);

                // Get the field value
                field = row.substr(start_position, end_position - start_position);

                // Check if the field is NULL
                if (field != "NULL")
                {
                    // If it is not NULL then try to convert it to the number
                    if (std::from_chars(field.begin(), field.end(), numeric_field).ptr != field.end())
                    {
                        // Don't count invalid row as processed
                        --i;
                        goto next_row_processing;
                    }
                }

                ++fields_number;

                // Move to the next field
                start_position = end_position + 1; 
            }
        }
        while (end_position < row.size() - 1);

        // Increment number of rows with current row's fields number 
        ++different_field_numbers[fields_number];

        // Use this label to move here if we found invalid row to process next row immediately
        // Don't count fields number of invalid rows
        next_row_processing:

        // Move to the next row
        start_position = newline_position + 1;
    }

    // Return the maximum fields number among the examined rows
    return std::max_element(
        different_field_numbers.begin(), 
        different_field_numbers.end(), 
        [](const std::pair<size_t, size_t>& pair1, const std::pair<size_t, size_t>& pair2)
        {
            return pair1.second < pair2.second;
        })->first;
}

size_t file_types_conversion::determine_fields_number_in_csv_like_file(
    const std::filesystem::path& csv_like_file_path,
    const std::string& delimiter)
{
    std::ifstream csv_like_file{csv_like_file_path};

    if (!csv_like_file.is_open())
    {
        return static_cast<size_t>(-1);
    }

    // Take buffer size of the number of bytes that is enough to process predefined number of rows to be examined
    const size_t buffer_size = config::ROWS_NUMBER_TO_EXAMINE * config::MAX_BYTES_NUMBER_IN_ROW;
    std::string buffer(buffer_size, char());

    csv_like_file.read(buffer.data(), buffer_size);

    // If file is less than we expected then resize buffer to the amount of read bytes
    if (static_cast<size_t>(csv_like_file.gcount()) < buffer_size)
    {
        buffer.resize(static_cast<size_t>(csv_like_file.gcount()));
    }

    std::string_view input_row, field;
    size_t row_start_position = 0, field_start_position, field_end_position,
        newline_position, first_newline_position, quotes_number, fields_number;
    // Count a number of different field numbers to know the most common fields number
    std::unordered_map<size_t, size_t> different_field_numbers;
    
    // Process rows until we process predefined number or file ends up.
    // Process rows in file as they are in csv format by getting each row, splitting it into the fields, 
    // refining them to the appropriate csv style and writing completed csv row to the output file.     
    // If the row violates csv format rules then try to process it as regular text row(i.e. the row still 
    // has to contain permanent fields number, separated by delimiter, but double quotes are ignored 
    // so any line feed means new row) by converting it to rigth csv format. If even these simplified rules
    // can't be accepted then skip the row as invalid.
    for (size_t i = 0; i < config::ROWS_NUMBER_TO_EXAMINE && newline_position != std::string::npos; ++i)
    {
        // Look for the newline that is assumed to be the end of the current row(this can be changed later
        // because this line feed might be the part of the actual row)
        newline_position = first_newline_position = buffer.find('\n', row_start_position);

        // Skip empty rows
        if (row_start_position == newline_position)
        {
            // Don't count empty row as processed
            --i;
            goto next_row_processing;
        }

        // Get row's data
        if (newline_position != std::string::npos)
        {
            input_row = std::string_view{buffer.begin() + row_start_position, buffer.begin() + newline_position};
        }
        else
        {
            input_row = std::string_view{buffer.begin() + row_start_position, buffer.end()};
        }

        fields_number = 0;
        field_start_position = 0;

        // Parse row as csv like until it is over
        do
        {
            // If field starts with double quote then this field has to be quoted and can contain special 
            // symbols like double quote, comma or line feed so parse the field with all rules
            if (input_row[field_start_position] == '"')
            {
                field_end_position = field_start_position;

                // Quoted field can contain double quotes but they have to be doubled so we have to skip even number
                // of double quotes until we find the odd one to find the end of the field 
                do
                {
                    // If we couldn't find closing double quote then the row is incomplete(newline is in row)
                    // so we have to expand the row to the next newline until we find the closing quote
                    while ((field_end_position = input_row.find('"', field_end_position + 1)) == std::string::npos) 
                    {     
                        // Current newline was the last in the buffer so we can't complete the row
                        // in terms of correct csv like file
                        if (newline_position == std::string::npos)
                        {
                            goto not_csv_like_row_processing;
                        }

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
                            input_row = std::string_view{buffer.begin() + row_start_position, buffer.end()};
                        }

                        // If the row can't fit in config::MAX_BYTES_NUMBER_IN_ROW then assume that it is not csv like
                        if (input_row.size() > config::MAX_BYTES_NUMBER_IN_ROW)
                        {
                            goto not_csv_like_row_processing;
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

                // If the row is not entirely processed and there is no delimiter after the field
                // then the row is not csv like
                if (field_end_position != input_row.size() && 
                    input_row.substr(field_end_position, delimiter.size()) != delimiter)
                {
                    goto not_csv_like_row_processing;
                }
            }
            // If field doesn't start with double quote then it is regular string and it can't contain 
            // special symbols that are described above, but comma still might be there because double quotes 
            // escape only delimiter in field but it could be not comma that will be used as the one in output file 
            else
            {
                // The end of the field can be definitely determined by the delimiter
                field_end_position = input_row.find(delimiter, field_start_position);

                field = input_row.substr(field_start_position, field_end_position - field_start_position);

                // If field contains double quote then it should has been quoted so the whole row is not csv like
                if (field.find('"') != std::string::npos)
                {
                    goto not_csv_like_row_processing;
                }
            }

            // Count current field
            ++fields_number;

            field_start_position = field_end_position + delimiter.size();
        } 
        while (field_end_position < input_row.size());

        // Count current row's fields number
        ++different_field_numbers[fields_number];

        // Skip processing of not csv like row because we just successfully processed it as csv
        goto next_row_processing;

        // Use this label to get here if the row violates requirements of the csv format
        not_csv_like_row_processing:

        // If we previously found the new newline in the row then the row was expanded with line feed inside
        // but in not csv like format we have to consider that each row ends with line feed
        // so shrink the row to the first newline
        if (newline_position != first_newline_position)
        {
            newline_position = first_newline_position;

            if (newline_position != std::string::npos)
            {
                input_row = std::string_view{buffer.begin() + row_start_position, buffer.begin() + newline_position};
            }
            else
            {
                input_row = std::string_view{buffer.begin() + row_start_position, buffer.end()};
            }
        }

        // Reset fields number to process the row again
        fields_number = 1;
        field_start_position = 0;

        // Count fields number in the row by splitting it into fields by delimiter
        while ((field_start_position = input_row.find(delimiter, field_start_position)) != std::string::npos)
        {
            // Count current field
            ++fields_number;

            field_start_position += delimiter.size();
        }

        // Count current row's fields number
        ++different_field_numbers[fields_number];

        // Use this label to get here if the row can't be processed and converted to csv format to skip it
        next_row_processing:

        // Move position to the beginning of the next row
        row_start_position = newline_position + 1;
    }

    // Return the maximum fields number among the examined rows
    return std::max_element(
        different_field_numbers.begin(), 
        different_field_numbers.end(), 
        [](const std::pair<size_t, size_t>& pair1, const std::pair<size_t, size_t>& pair2)
        {
            return pair1.second < pair2.second;
        })->first;
}

bool file_types_conversion::convert_csv_like_file_to_csv(
    const std::filesystem::path& text_file_path, 
    const std::filesystem::path& csv_file_path)
{
    // Define processing of failed conversion that removes csv file and returns false as the result of conversion
    // to invoke this function if we won't be able to convert sql like file to csv
    auto process_failed_conversion = 
        [&csv_file_path]
        {
            try
            {
                std::filesystem::remove(csv_file_path);
            }
            catch (const std::exception& ex)
            {
                LOG_ERROR << ex.what();
            }

            return false;
        };

    std::ifstream text_file{text_file_path};
    std::ofstream csv_file{csv_file_path};

    if (!text_file.is_open() || !csv_file.is_open())
    {
        return process_failed_conversion();
    }

    // Determine the delimiter of the text file
    std::string delimiter = delimiter_finder::determine_text_file_delimiter(text_file_path);
    
    // Couldn't determine the delimiter
    if (delimiter.empty())
    {
        return process_failed_conversion();
    }

    // Determine the fields number of the file
    size_t fields_number = determine_fields_number_in_csv_like_file(text_file_path, delimiter);
    
    // Couldn't open the file
    if (fields_number == static_cast<size_t>(-1))
    {
        return process_failed_conversion();
    }

    // Take buffer size of the number of bytes that is enough to process predefined number of rows to be examined
    const size_t buffer_size = config::ROWS_NUMBER_TO_EXAMINE * config::MAX_BYTES_NUMBER_IN_ROW;
    std::string buffer(buffer_size, char());

    text_file.read(buffer.data(), buffer_size);

    // Since we control reading in buffer manually, we have to resize buffer explicitly 
    // if there was the last reading and we couldn't read the full buffer_size bytes
    // so store value of if we still have to read file
    bool is_reading_pending = true;

    // If we read less bytes than it was required then exchange flag value
    // and shrink buffer to actual size of read bytes 
    if (static_cast<size_t>(text_file.gcount()) < buffer_size)
    {
        is_reading_pending = false;
        buffer.resize(static_cast<size_t>(text_file.gcount()));
    }

    std::string_view input_row, field;
    std::string output_row;
    size_t row_start_position = 0, field_start_position, field_end_position,
        newline_position, first_newline_position, quotes_number, i;
    bool has_to_be_field_quoted;
    
    // Process rows in file as they are in csv format by getting each row, splitting it into the fields, 
    // refining them to the appropriate csv style and writing completed csv row to the output file.     
    // If the row violates csv format rules then try to process it as regular text row(i.e. the row still 
    // has to contain permanent fields number, separated by delimiter, but double quotes are ignored 
    // so any line feed means new row) by converting it to rigth csv format. If even these simplified rules
    // can't be accepted then skip the row as invalid.
    do
    {
        // Look for the newline that is assumed to be the end of the current row(this can be changed later
        // because this line feed might be the part of the actual row)
        newline_position = first_newline_position = buffer.find('\n', row_start_position);

        // Skip empty rows
        if (row_start_position == newline_position || row_start_position == buffer.size())
        {
            goto next_row_processing;
        }

        // Get row's data
        if (newline_position != std::string::npos)
        {
            input_row = std::string_view{buffer.begin() + row_start_position, buffer.begin() + newline_position};
        }
        else
        {
            // If we didn't read the whole file then we have to find the newline so
            // if there are no one then the row can't fit in our buffer and we can't process it 
            if (is_reading_pending)
            {
                goto next_row_processing;
            }
            
            input_row = std::string_view{buffer.begin() + row_start_position, buffer.end()};
        }

        field_start_position = 0;
        field_end_position = 0;

        // Parse row as csv like until we process all fields or the row is over
        for (i = 0; i < fields_number && field_end_position < input_row.size(); ++i)
        {
            // If field starts with double quote then this field has to be quoted and can contain special 
            // symbols like double quote, comma or line feed so parse the field with all rules
            if (input_row[field_start_position] == '"')
            {
                field_end_position = field_start_position;

                // Quoted field can contain double quotes but they have to be doubled so we have to skip even number
                // of double quotes until we find the odd one to find the end of the field 
                do
                {
                    // If we couldn't find closing double quote then the row is incomplete(newline is in row)
                    // so we have to expand the row to the next newline until we find the closing quote
                    while ((field_end_position = input_row.find('"', field_end_position + 1)) == std::string::npos) 
                    {           
                        // Current newline was the last in the buffer so we can't complete the row
                        // in terms of correct csv like file
                        if (newline_position == std::string::npos)
                        {
                            goto not_csv_like_row_processing;
                        }

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
                            input_row = std::string_view{buffer.begin() + row_start_position, buffer.end()};
                        }

                        // If the row can't fit in config::MAX_BYTES_NUMBER_IN_ROW then assume that it is not csv like
                        if (input_row.size() > config::MAX_BYTES_NUMBER_IN_ROW)
                        {
                            goto not_csv_like_row_processing;
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

                // If the row is not entirely processed and there is no delimiter after the field
                // then the row is not csv like
                if (i != fields_number - 1 && 
                    input_row.substr(field_end_position, delimiter.size()) != delimiter)
                {
                    goto not_csv_like_row_processing;
                }

                // Get field without quotes
                field = input_row.substr(field_start_position + 1, field_end_position - field_start_position - 2);

                has_to_be_field_quoted = false;

                // If newline position was changed then there is line feed in the field so it has to be quoted 
                if (newline_position != first_newline_position)
                {
                    has_to_be_field_quoted = true;
                }
                else
                {
                    // In csv format field has to be quoted if it contains comma or double quote
                    for (auto symbol : field)
                    {
                        if (symbol == ',' || symbol == '"')
                        {
                            has_to_be_field_quoted = true;
                            break;
                        }
                    }
                }

                if (has_to_be_field_quoted)
                {
                    // Append double quotes to the beginning and to the end of the field
                    output_row += '"';
                    output_row += field;
                    output_row += "\",";
                }
                else
                {
                    output_row += field;
                    output_row += ',';
                }
            }
            // If field doesn't start with double quote then it is regular string and it can't contain 
            // special symbols that are described above, but comma still might be there because double quotes 
            // escape only delimiter in field but it could be not comma that will be used as the one in output file 
            else
            {
                // The end of the field can be definitely determined by the delimiter
                field_end_position = input_row.find(delimiter, field_start_position);

                field = input_row.substr(field_start_position, field_end_position - field_start_position);

                has_to_be_field_quoted = false;

                for (auto symbol : field)
                {
                    // In csv format field has to be quoted if it contains comma
                    if (symbol == ',')
                    {
                        has_to_be_field_quoted = true;
                        break;
                    }

                    // If field contains double quote then it should has been quoted so the whole row is not csv like
                    if (symbol == '"')
                    {
                        goto not_csv_like_row_processing;
                    }
                }

                if (has_to_be_field_quoted)
                {
                    // Append double quotes to the beginning and to the end of the field
                    output_row += '"';
                    output_row += field;
                    output_row += "\",";
                }
                else
                {
                    output_row += field;
                    output_row += ',';
                }
            }
            
            field_start_position = field_end_position + delimiter.size();
        } 

        // Skip the row if it contains invalid fields number or it was not entirely processed
        if (i != fields_number || field_end_position < input_row.size() - 1)
        {
            goto next_row_processing;
        }

        // Replace the last comma with line feed as the end of row
        output_row.back() = '\n';

        // Write the row into csv file
        csv_file.write(output_row.c_str(), output_row.size());

        // Skip processing of not csv like row because we just successfully processed it as csv
        goto next_row_processing;

        // Use this label to get here if the row violates requirements of the csv format
        not_csv_like_row_processing:

        // If we previously found the new newline in the row then the row was expanded with line feed inside
        // but in not csv like format we have to consider that each row ends with line feed
        // so shrink the row to the first newline
        if (newline_position != first_newline_position)
        {
            newline_position = first_newline_position;

            if (newline_position != std::string::npos)
            {
                input_row = std::string_view{buffer.begin() + row_start_position, buffer.begin() + newline_position};
            }
            else
            {
                input_row = std::string_view{buffer.begin() + row_start_position, buffer.end()};
            }
        }

        output_row.clear();

        field_start_position = 0;
        field_end_position = 0;

        for (i = 0; i < fields_number && field_end_position < input_row.size(); ++i)
        {
            field_end_position = input_row.find(delimiter, field_start_position);

            field = input_row.substr(field_start_position, field_end_position - field_start_position);

            // Remember position where current field starts in output row to quote the whole field if it is necessary 
            field_start_position = output_row.size();

            has_to_be_field_quoted = false;

            for (auto symbol : field)
            {
                // Don't append double quote because since row is not csv like 
                // it can be incomplete in terms of lack of necessary double quotes
                if (symbol != '"')
                {
                    // If row contains comma then it has to be quoted in csv format
                    if (!has_to_be_field_quoted && symbol == ',')
                    {
                        has_to_be_field_quoted = true;
                    }

                    output_row += symbol;
                }
            }
            
            if (has_to_be_field_quoted)
            {
                // Append double quotes to the beginning and to the end of the field
                output_row.insert(field_start_position, "\"");
                output_row += "\"";
            }

            output_row += ',';

            field_start_position = field_end_position + delimiter.size();
        }

        // Skip the row if it contains invalid fields number or it was not entirely processed
        if (i != fields_number || field_end_position < input_row.size())
        {
            goto next_row_processing;
        }

        // Replace the last comma with line feed as the end of row
        output_row.back() = '\n';

        // Write the row into csv file
        csv_file.write(output_row.c_str(), output_row.size());

        // Use this label to get here if the row can't be processed and converted to csv format to skip it
        next_row_processing:

        if (newline_position != std::string::npos)
        {
            // Move position to the beginning of the next row
            row_start_position = newline_position + 1;
        }
        else
        {
            // Assign start position to the buffer size to erase the incomplete row from the buffer further
            row_start_position = buffer_size;
        }

        // We assume that each row has to be less than config::MAX_BYTES_NUMBER_IN_ROW bytes 
        // so if it is left less in buffer and we didn't read all the file 
        // then move the remainder to the beginning and read up to buffer_size again 
        if (is_reading_pending && buffer_size - row_start_position < config::MAX_BYTES_NUMBER_IN_ROW)
        {
            std::move(buffer.begin() + row_start_position, buffer.end(), buffer.begin());

            text_file.read(buffer.data() + buffer_size - row_start_position, row_start_position);

            // If we read less bytes than it was required then exchange flag value
            // and shrink buffer to actual size of moved remainder and read bytes 
            if (static_cast<size_t>(text_file.gcount()) < row_start_position)
            {
                is_reading_pending = false;
                buffer.resize(buffer_size - row_start_position + static_cast<size_t>(text_file.gcount()));
            }

            // If newline position was found previously then start the next row from the beginnig of the buffer
            if (newline_position != std::string::npos)
            {
                row_start_position = 0;
            }
            // If newline position was not found then the previous row was incomplete so
            // we have to find the beginning of the new row in the updated buffer
            else
            {
                newline_position = buffer.find('\n');

                // If the beginning of the new row was found and the remaining buffer is enough for that row
                // then just assign found beginning to the start position
                if (newline_position != std::string::npos && 
                    buffer_size - newline_position - 1 >= config::MAX_BYTES_NUMBER_IN_ROW)
                {
                    row_start_position = newline_position + 1;
                }
                // If the whole buffer still doesn't contain newline then repeat erasing the data and 
                // reading the new one until either we find the valid row or the file ends up
                else
                {
                    goto next_row_processing;
                }
            }
        }
        
        // Clear just processed csv row to use it for the next one
        output_row.clear();
    }
    while (is_reading_pending || newline_position != std::string::npos);

    return true;
}

bool file_types_conversion::convert_xlsx_to_csv(
    const std::filesystem::path& xlsx_file_path, 
    const std::filesystem::path& csv_file_path)
{
    // Create a pipe stream to get the error output in it
    boost::process::ipstream pipe_stream;
    int error_code;
    
    try
    {
        // Run the command to convert xlsx format to csv by in2csv terminal tool
        // Override standard output to csv file 
        // Override error output to pipe stream 
        error_code = boost::process::system(
            std::format(
                "in2csv \"{}\"", 
                xlsx_file_path.c_str()),
            boost::process::std_out > csv_file_path.c_str(),
            boost::process::std_err > pipe_stream);
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << std::format(
            "Could not convert '{}' to csv format:\n{}", 
            xlsx_file_path.c_str(),
            ex.what());

        // We have to remove just created csv file because it is useless now
        try
        {
            std::filesystem::remove(csv_file_path);
        }
        catch (const std::exception& ex)
        {
            LOG_ERROR << ex.what();
        }
        
        return false;
    }
    
    // If the command returned non zero code then the error occured
    if (error_code) 
    {
        // Get the pipe stream data - error message and log it
        LOG_ERROR << std::format(
            "Could not convert '{}' to csv format:\n{}", 
            xlsx_file_path.c_str(),
            std::string{std::istreambuf_iterator<char>(pipe_stream), std::istreambuf_iterator<char>()});

        // We have to remove just created csv file because it is useless now
        try
        {
            std::filesystem::remove(csv_file_path);
        }
        catch (const std::exception& ex)
        {
            LOG_ERROR << ex.what();
        }
        
        return false;
    }
    else
    {
        return true;
    }
}

bool file_types_conversion::convert_sql_to_csv(
    const std::filesystem::path& sql_file_path, 
    const std::filesystem::path& csv_file_path)
{
    // Define processing of failed conversion that removes csv file and returns false as the result of conversion
    // to invoke this function if we won't be able to convert sql file to csv
    auto process_failed_conversion = 
        [&csv_file_path]
        {
            try
            {
                std::filesystem::remove(csv_file_path);
            }
            catch (const std::exception& ex)
            {
                LOG_ERROR << ex.what();
            }

            return false;
        };

    std::ifstream sql_file{sql_file_path};
    std::ofstream csv_file{csv_file_path};

    if (!sql_file.is_open() || !csv_file.is_open())
    {
        return process_failed_conversion();
    }

    // Take buffer size of the number of bytes that is enough to process predefined number of rows to be examined
    const size_t buffer_size = config::ROWS_NUMBER_TO_EXAMINE * config::MAX_BYTES_NUMBER_IN_ROW;
    std::string buffer(buffer_size, char());
    size_t start_position, end_position;

    sql_file.read(buffer.data(), buffer_size);

    // Since we control reading in buffer manually, we have to resize buffer explicitly 
    // if there was the last reading and we couldn't read the full buffer_size bytes
    // so store value of if we still have to read file
    bool is_reading_pending = true;

    // If we read less bytes than it was required then exchange flag value
    // and shrink buffer to actual size of read bytes 
    if (static_cast<size_t>(sql_file.gcount()) < buffer_size)
    {
        is_reading_pending = false;
        buffer.resize(static_cast<size_t>(sql_file.gcount()));
    }
    
    // This string is used to store sql expressions like CREATE TABLE or INSERT INTO to look for in file
    std::string search_string = "CREATE TABLE";

    // Find CREATE TABLE statement to get header line in csv as field names of sql table
    if ((start_position = buffer.find(search_string)) == std::string::npos)
    {
        return process_failed_conversion();    
    }

    // Find the beginning bracket of CREATE statement
    if ((start_position = buffer.find('(', start_position + search_string.size())) == std::string::npos)
    {
        return process_failed_conversion();    
    }

    // Find the end of CREATE TABLE statement
    if ((end_position = buffer.find(';', start_position + 1)) == std::string::npos)
    {
        return process_failed_conversion();    
    }

    // Get the whole CREATE statement data to parse it
    std::string_view statement_view{buffer.begin() + start_position, buffer.begin() + end_position};

    size_t fields_number = 0;
    std::string row;
    // Start parsing from the beginning of CREATE statement 
    end_position = 0;
    // Format of beginning of the field name in MySQL dump
    search_string = "\n  `";
    
    // Parse field names in CREATE TABLE statement to fill header line in csv
    // Find the beginning of field name while there are
    while ((start_position = statement_view.find(search_string, end_position + 1)) != std::string::npos)
    {
        // Find the end of field name
        if ((end_position = statement_view.find('`', start_position + search_string.size())) == std::string::npos)
        {
            return process_failed_conversion();
        }

        ++fields_number;

        // Append field name
        row += statement_view.substr(
            start_position + search_string.size(), 
            end_position - start_position - search_string.size());
        row += ',';
    }

    // There has to be at least one field
    if (!fields_number)
    {
        return process_failed_conversion();    
    }

    // Replace the last comma with line feed as the end of row
    row.back() = '\n';

    // Write the header line into csv file
    csv_file.write(row.c_str(), row.size());

    // Clear the header to use row for the actual data
    row.clear();

    search_string = "INSERT INTO";

    // Find INSERT INTO statement right after the end of CREATE statement
    if ((start_position = buffer.find(search_string, statement_view.end() - buffer.data())) == std::string::npos)
    {
        return process_failed_conversion();    
    }

    start_position += search_string.size();
    search_string = "VALUES";

    // Find VALUES as part of the INSERT INTO statement
    if ((start_position = buffer.find(search_string, start_position)) == std::string::npos)
    {
        return process_failed_conversion();    
    }

    // Find the beginning bracket of the first data cortege of INSERT INTO statement
    if ((start_position = buffer.find('(', start_position + search_string.size())) == std::string::npos)
    {
        return process_failed_conversion();    
    }

    // Move to the actual INSERT data
    ++start_position;

    std::string field;
    std::string_view field_view;
    size_t backslashes_number;
    double numeric_field;
    bool has_to_be_field_quoted;

    // Parse sql INSERT values and retrieve field values into rows 
    while (true)
    {
        for (size_t i = 0; i < fields_number; ++i)
        {
            // In some dumps first symbol in non-first fields can be space
            if (buffer[start_position] == ' ')
            {
                ++start_position;
            }

            // If field value starts with single quote then it is string
            if (buffer[start_position] == '\'')
            {
                end_position = start_position;

                // In string field there can be single quote but it can be escaped with odd number of backslashes
                // because of escaped backslashes: ///' -> /' that is just quote within field,
                // but //' is a escaped backslash with quote that is the end of field
                // so search for single quote with even number of preceding backslashes to find the end of field
                do
                {
                    if ((end_position = buffer.find('\'', end_position + 1)) == std::string::npos) 
                    {
                        return process_failed_conversion();
                    }
                    
                    backslashes_number = 0;

                    while (buffer[end_position - backslashes_number - 1] == '\\')
                    {
                        ++backslashes_number;
                    }
                } 
                while (backslashes_number % 2 == 1);
                                
                // Get string field value between single quotes
                field = buffer.substr(start_position + 1, end_position - start_position - 1);

                // Move to the data after the field value to parse
                start_position = end_position + 2;

                // Not escaped first double quote is forbidden so the row is invalid 
                if (field.front() == '"')
                {
                    return process_failed_conversion();
                }

                // Parse special sql sequences starting with backslash
                // by replacing them with appropriate symbols in csv
                for (size_t j = field.size() - 1; j > 0 && j < field.size(); --j)
                {
                    // Double quote has to be doubled in csv so just replace backslash with double qoute
                    if (field[j] == '"')
                    {
                        if (field[j - 1] == '\\')
                        {
                            field[j - 1] = '"';
                            --j;
                        }
                        else
                        {
                            return process_failed_conversion();
                        }
                    }
                    // Single quote and backslash are reqular symbols is csv so just erase first backslash
                    else if (field[j] == '\'' || field[j] == '\\')
                    {
                        if (field[j - 1] == '\\')
                        {
                            field.erase(j - 1, 1);
                            --j;
                        }
                        else
                        {
                            return process_failed_conversion();
                        }
                    }
                    // Replace \n string with actual line feed character that is newline
                    else if (field[j] == 'n' && field[j - 1] == '\\')
                    {
                        field.replace(j - 1, 2, "\n");
                        --j;
                    }
                    // Erase \r as it is useless
                    else if (field[j] == 'r' && field[j - 1] == '\\')
                    {
                        field.erase(j - 1, 2);
                        --j;
                    }
                }

                has_to_be_field_quoted = false;

                // In csv format field has to be double quoted if it contains comma, double quote or line feed 
                for (auto symbol : field)
                {
                    if (symbol == ',' || symbol == '"' || symbol == '\n')
                    {
                        has_to_be_field_quoted = true;

                        break;
                    }
                }

                if (has_to_be_field_quoted)
                {
                    // Append double quotes to the beginning and to the end of the string
                    row += '"' + field + "\",";
                }
                else
                {
                    row += field + ',';
                }
            }
            // If field value doesn't start with single quote then either it is NULL 
            // or number(bool is represented by 0 or 1)
            else
            {
                // If field is not the last then we just find comma as the end of the field 
                if (i != fields_number - 1)
                {
                    if ((end_position = buffer.find(',', start_position)) == std::string::npos)
                    {
                        return process_failed_conversion();
                    }
                }
                // If field is the last then we find ) as the end of the INSERT cortege
                else
                {
                    if ((end_position = buffer.find(')', start_position)) == std::string::npos)
                    {
                        return process_failed_conversion();
                    }
                }

                // Get the field value
                field_view = std::string_view{buffer.begin() + start_position, buffer.begin() + end_position};

                // Check if the field is NULL
                if (field_view != "NULL")
                {
                    // If it is not NULL then try to convert it to the number
                    if (std::from_chars(field_view.begin(), field_view.end(), numeric_field).ptr != field_view.end())
                    {
                        return process_failed_conversion();
                    }
                }

                // Append field value and comma to the row
                row += field_view;
                row += ',';

                // Move to the data after the field value to parse
                start_position = end_position + 1;
            }
        }

        // Replace the last comma with line feed as the end of row
        row.back() = '\n';

        // Write the row into csv file
        csv_file.write(row.c_str(), row.size());

        // The semicolon after INSERT cortege means the end of INSERT INTO statement 
        if (buffer[start_position] == ';')
        {
            search_string = "INSERT INTO";

            // If there is no next INSERT INTO statement then it is the end of table data
            if (buffer.substr(start_position + 2, search_string.size()) != search_string)
            // if ((start_position = buffer.find(search_string, start_position + 1)) == std::string::npos)
            {
                break;
            }

            start_position += 2 + search_string.size();
            search_string = "VALUES";

            // Find VALUES as part of the INSERT INTO statement
            if ((start_position = buffer.find(search_string, start_position)) == 
                std::string::npos)
            {
                return process_failed_conversion();    
            }

            // Find the beginning bracket of the first data cortege of INSERT INTO statement
            if ((start_position = buffer.find('(', start_position + search_string.size())) == std::string::npos)
            {
                return process_failed_conversion();    
            }

            // Move to the actual INSERT data
            ++start_position;
        }
        // If there is no semicolon then process next INSERT data
        else
        {
            // INSERT corteges has to be separated with comma
            if (buffer[start_position] != ',')
            {
                return process_failed_conversion();
            }

            // Each INSERT cortege may start with newline 
            if (buffer[start_position + 1] == '\n')
            {
                start_position += 1;
            }

            // Move to the next INSERT cortege data
            start_position += 2;
        }

        // We assume that each row has to be less than config::MAX_BYTES_NUMBER_IN_ROW bytes 
        // so if it is left less in buffer and we didn't read all the file 
        // then move the remainder to the beginning and read up to buffer_size again 
        if (is_reading_pending && buffer_size - start_position < config::MAX_BYTES_NUMBER_IN_ROW)
        {
            std::move(buffer.begin() + start_position, buffer.end(), buffer.begin());

            sql_file.read(buffer.data() + buffer_size - start_position, start_position);

            // If we read less bytes than it was required then exchange flag value
            // and shrink buffer to actual size of moved remainder and read bytes 
            if (static_cast<size_t>(sql_file.gcount()) < start_position)
            {
                is_reading_pending = false;
                buffer.resize(buffer_size - start_position + static_cast<size_t>(sql_file.gcount()));
            }

            start_position = 0;
        }

        // Clear the row to use it for the next one
        row.clear();
    }

    return true;
}