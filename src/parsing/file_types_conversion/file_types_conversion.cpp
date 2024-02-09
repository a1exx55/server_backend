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
    return false;
    
    // Define removal of csv file that we will create to invoke it if we won't be able to convert sql file to csv
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

    const size_t BUFFER_SIZE = 1000;
    std::string buffer(BUFFER_SIZE, char());
    size_t position;
    std::string search_string = "CREATE TABLE";
    size_t remainder_string_size = search_string.size() - 1;
    sql_file.read(buffer.data(), BUFFER_SIZE);
    
    while ((position = buffer.find(search_string)) == std::string::npos)
    {
        std::move(buffer.end() - remainder_string_size, buffer.end(), buffer.begin());

        sql_file.read(buffer.data() + remainder_string_size, BUFFER_SIZE - remainder_string_size);
    }
}