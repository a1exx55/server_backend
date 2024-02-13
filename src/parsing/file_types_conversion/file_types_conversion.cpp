#include <parsing/file_types_conversion/file_types_conversion.hpp>

bool file_types_conversion::convert_file_to_csv(
    std::string_view file_type,
    const std::filesystem::path& file_path, 
    const std::filesystem::path& output_file_path)
{
    if (file_type == "txt")
    {
        return convert_txt_to_csv(file_path, output_file_path);
    }
    else if (file_type == "xlsx")
    {
        return convert_xlsx_to_csv(file_path, output_file_path);
    }
    else if (file_type == "sql")
    {
        return convert_sql_to_csv(file_path, output_file_path);
    }
    else
    {
        return false;
    }
}

bool file_types_conversion::convert_txt_to_csv(
    const std::filesystem::path& txt_file_path, 
    const std::filesystem::path& csv_file_path)
{
    // To convert txt to csv we just need to change file extension because both text formats are similar
    try
    {
        std::filesystem::rename(
            txt_file_path,
            csv_file_path);
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();

        return false;
    }

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
        // Remove xlsx file because it is useless now
        try
        {
            std::filesystem::remove(xlsx_file_path);
        }
        catch (const std::exception& ex)
        {
            LOG_ERROR << ex.what();
        }

        return true;
    }
}

bool file_types_conversion::convert_sql_to_csv(
    const std::filesystem::path& sql_file_path, 
    const std::filesystem::path& csv_file_path)
{
    // Define removal of csv file to invoke it if we won't be able to convert sql file to csv
    auto remove_csv_file = 
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
        };

    std::ifstream sql_file{sql_file_path};
    std::ofstream csv_file{csv_file_path};

    if (!sql_file.is_open() || !csv_file.is_open())
    {
        remove_csv_file();

        return false;
    }

    const size_t BUFFER_SIZE = 1024 * 1024;
    std::string buffer(BUFFER_SIZE, char());
    size_t start_position, end_position;
    std::string search_string = "CREATE TABLE";

    sql_file.read(buffer.data(), BUFFER_SIZE);
    
    // Find CREATE TABLE statement to get header line in csv as field names of sql table
    if ((start_position = buffer.find(search_string)) == std::string::npos)
    {
        remove_csv_file();

        return false;
    }

    // Find the beginning bracket of CREATE statement
    if ((start_position = buffer.find('(', start_position + search_string.size())) == std::string::npos)
    {
        remove_csv_file();

        return false;
    }

    // Find the end of CREATE TABLE statement
    if ((end_position = buffer.find(';', start_position + 1)) == std::string::npos)
    {
        remove_csv_file();

        return false;
    }

    size_t fields_number = 0;
    std::string row;
    end_position = start_position;
    // Format of beginning of the field name in MySQL dump
    search_string = "\n  `";
    
    // Parse field names in CREATE TABLE statement to fill header line in csv
    // Find the beginning of field name while there are
    while ((start_position = buffer.find(search_string, end_position + 1)) != std::string::npos)
    {
        // Find the end of field name
        if ((end_position = buffer.find('`', start_position + search_string.size())) == std::string::npos)
        {
            remove_csv_file();

            return false;
        }

        ++fields_number;

        // Append field name
        row += buffer.substr(
            start_position + search_string.size(), 
            end_position - start_position - search_string.size()) + ',';
    }

    // There has to be at least one field
    if (!fields_number)
    {
        remove_csv_file();

        return false;
    }

    // Replace the last comma with line feed as the end of row
    row[row.size() - 1] = '\n';

    // Write the header line into csv file
    csv_file.write(row.c_str(), row.size());

    search_string = "INSERT INTO";

    // Find INSERT INTO statement to get actual data
    if ((start_position = buffer.find(search_string)) == std::string::npos)
    {
        remove_csv_file();

        return false;
    }

    // Find the beginning bracket of INSERT INTO statement
    if ((start_position = buffer.find('(', start_position + search_string.size())) == std::string::npos)
    {
        remove_csv_file();

        return false;
    }

    std::string field_value;
    bool has_to_be_string_quoted;

    // Parse sql insert values and retrieve field values into rows 
    while (true)
    {
        row.clear();

        for (size_t i = 0; i < fields_number; ++i)
        {
            // If field value starts with single quote then it is string
            if (buffer[start_position + 1] == '\'')
            {
                // In string field there can be single quote but it has to be escaped with backslash
                // so search for single quote until it is the end of the field value
                if ((end_position = buffer.find('\'', start_position + 2)) == std::string::npos) 
                {
                    remove_csv_file();

                    return false;
                }
                
                while (buffer[end_position - 1] == '\\')
                {
                    if ((end_position = buffer.find('\'', end_position + 1)) == std::string::npos) 
                    {
                        remove_csv_file();

                        return false;
                    }
                } 

                // Get string field value between single quotes
                field_value = buffer.substr(start_position + 2, end_position - start_position - 2);

                has_to_be_string_quoted = false;

                // In csv format field has to be double quoted if it contains comma, double quote or line feed 
                for (auto symbol : field_value)
                {
                    if (symbol == ',' || symbol == '"' || symbol == '\n')
                    {
                        has_to_be_string_quoted = true;

                        break;
                    }
                }

                // If field has to be quoted then \" in sql is replaced with "" in csv as ecsaping
                // and \' is replaced with ' because it is not required to be escaped 
                if (has_to_be_string_quoted)
                {
                    for (size_t j = 0; j < field_value.size(); ++j)
                    {
                        if (field_value[j] == '"')
                        {
                            field_value[j - 1] = '"';
                        }
                        else if (field_value[j] == '\'')
                        {
                            field_value.erase(j - 1, 1);
                            --j;
                        }
                    }

                    // Append double quotes to the beginning and to the end of the string
                    field_value.insert(0, "\"");
                    field_value.append("\"");
                }
                // If field has not to be quoted then in \" and \' we just remove backslash
                else
                {
                    for (size_t j = 0; j < field_value.size(); ++j)
                    {
                        if (field_value[j] == '"' || field_value[j] == '\'')
                        {
                            field_value.erase(j - 1, 1);
                            --j;
                        }
                    }
                }

                // Append field value with delimeter int the row
                row += field_value + ',';

                ++end_position;
            }
            // If field value doesn't start with single quote then there can be number, bool or NULL
            // so there is no any special format to apply
            else
            {
                // If field is not the last then we just find comma as the end of the field 
                if (i != fields_number - 1)
                {
                    if ((end_position = buffer.find(',', start_position + 1)) == std::string::npos)
                    {
                        remove_csv_file();

                        return false;
                    }
                }
                // If field is the last then we find ) as the end of the INSERT cortege
                else
                {
                    if ((end_position = buffer.find(')', start_position + 1)) == std::string::npos)
                    {
                        remove_csv_file();

                        return false;
                    }
                }

                // Append field value with comma
                row += buffer.substr(start_position + 1, end_position - start_position - 1) + ',';
            }

            start_position = end_position;
        }

        // Replace the last comma with line feed as the end of row
        row[row.size() - 1] = '\n';

        // Write the line into csv file
        csv_file.write(row.c_str(), row.size());

        // The semicolon after INSERT cortege means the end of INSERT INTO statement 
        if (buffer[start_position + 1] == ';')
        {
            // If there is no next INSERT INTO statement then it is the end of table data
            if (buffer.substr(start_position + 3, search_string.size()) != search_string)
            {
                break;
            }

            // Find the beginning bracket of INSERT INTO statement
            if ((start_position = buffer.find('(', start_position + 3 + search_string.size())) == std::string::npos)
            {
                remove_csv_file();

                return false;
            }
        }
        // If there is no semicolon then just shift to the next INSERT cortege
        else
        {
            start_position += 2;
        }

        // We assume that each row has to be less than 2000 bytes 
        // so if it is left less in buffer then move the remainder to the beginning and read 1 MB again 
        if (BUFFER_SIZE - start_position < 2000)
        {
            std::move(buffer.begin() + start_position, buffer.end(), buffer.begin());

            sql_file.read(buffer.data() + BUFFER_SIZE - start_position, start_position);

            start_position = 0;
        }
    }

    // Remove sql file because it is useless now
    try
    {
        std::filesystem::remove(sql_file_path);
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
    }

    return true;
}