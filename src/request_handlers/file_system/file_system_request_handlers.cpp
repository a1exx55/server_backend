#include <request_handlers/file_system/file_system_request_handlers.hpp>

void request_handlers::file_system::get_folders_info(
    [[maybe_unused]] const request_params& request, 
    response_params& response)
{
    auto db_conn = database_connections_pool::get<file_system_database_connection>();

    // No available connections
    if (!db_conn)
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "No available database connections");
    }
    
    // Get all folders info in json format
    std::optional<json::array> folders_info_array_opt = db_conn->get_folders_info(); 

    // An error occured with database connection
    if (!folders_info_array_opt.has_value())
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "Internal server error occured");
    }
    
    // Initialize body with string representation of json
    response.body = json::serialize(folders_info_array_opt.value());
}

void request_handlers::file_system::get_file_rows_number(const request_params& request, response_params& response)
{
    size_t file_id;

    if (!http_utils::uri::get_path_parameter(request.uri, request.uri_template, "fileId", file_id))
    {
        return prepare_error_response(
            response,
            http::status::unprocessable_entity, 
            "Invalid file id");
    }

    auto db_conn = database_connections_pool::get<file_system_database_connection>();

    // No available connections
    if (!db_conn)
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "No available database connections");
    }
    
    // Get file path by id to interact with file itself
    std::optional<std::string> file_path_opt = db_conn->get_file_path(file_id); 

    // An error occured with database connection
    if (!file_path_opt.has_value())
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "Internal server error occured");
    }

    // File with given id doesn't exist
    if (file_path_opt.value() == "")
    {
        return prepare_error_response(
            response,
            http::status::not_found, 
            "File was not found");
    }

    size_t file_rows_number = file_preview::get_file_rows_number(file_path_opt.value());

    // File was just deleted
    if (file_rows_number == static_cast<size_t>(-1))
    {
        return prepare_error_response(
            response,
            http::status::not_found, 
            "File was not found");
    }

    response.body = json::serialize(
        json::object
        {
            {"rowsNumber", file_rows_number}
        });
}

void request_handlers::file_system::get_file_raw_rows(const request_params& request, response_params& response)
{
    size_t file_id;

    if (!http_utils::uri::get_path_parameter(request.uri, request.uri_template, "fileId", file_id))
    {
        return prepare_error_response(
            response,
            http::status::unprocessable_entity, 
            "Invalid file id");
    }

    size_t from_row_number, rows_number;

    if (!http_utils::uri::get_query_parameter(request.uri, "fromRowNumber", from_row_number))
    {
        return prepare_error_response(
            response,
            http::status::unprocessable_entity, 
            "Invalid row parameters");
    }

    if (!http_utils::uri::get_query_parameter(request.uri, "rowsNumber", rows_number))
    {
        return prepare_error_response(
            response,
            http::status::unprocessable_entity, 
            "Invalid row parameters");
    }

    auto db_conn = database_connections_pool::get<file_system_database_connection>();

    // No available connections
    if (!db_conn)
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "No available database connections");
    }
    
    // Get file path by id to interact with file itself
    std::optional<std::string> file_path_opt = db_conn->get_file_path(file_id); 

    // An error occured with database connection
    if (!file_path_opt.has_value())
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "Internal server error occured");
    }

    // File with given id doesn't exist
    if (file_path_opt.value() == "")
    {
        return prepare_error_response(
            response,
            http::status::not_found, 
            "File was not found");
    }

    std::pair<uint8_t, json::array> file_rows = 
        file_preview::get_file_raw_rows(file_path_opt.value(), from_row_number, rows_number);

    // An error occured while processing the raw rows
    if (file_rows.first)
    {
        // Desired file contains too large data to preview 
        if (file_rows.first == 3)
        {
            return prepare_error_response(
                response,
                http::status::unprocessable_entity, 
                "Too large data to preview");
        }

        // Invalid row parameters were provided
        if (file_rows.first == 2)
        {
            return prepare_error_response(
                response,
                http::status::unprocessable_entity, 
                "Invalid row parameters");
        }

        // File was just deleted
        if (file_rows.first == 1)
        {
            return prepare_error_response(
                response,
                http::status::not_found, 
                "File was not found");
        }
    }

    response.body = json::serialize(file_rows.second);
}

void request_handlers::file_system::create_folder(const request_params& request, response_params& response)
{
    try
    {
        std::string folder_name = json::parse(request.body).as_object().at("folderName").as_string().c_str();

        // Trim whitespaces at the beggining and at the end of the folder name
        boost::algorithm::trim(folder_name);

        // Transform the string to UnicodeString to check the actual length of unicode symbols
        icu::UnicodeString folder_name_unicode{folder_name.c_str()}; 

        // Folder name can't be less than 3 and longer than 64 symbols
        if (folder_name_unicode.length() < 3 || folder_name_unicode.length() > 64) 
        {
            return prepare_error_response(
                response, 
                http::status::unprocessable_entity, 
                "Invalid folder name format");
        }

        auto db_conn = database_connections_pool::get<file_system_database_connection>();
 
        // No available connections
        if (!db_conn)
        {
            return prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "No available database connections");
        }

        std::optional<bool> does_folder_already_exist = db_conn->check_folder_existence_by_name(folder_name);

        // An error occured with database connection
        if (!does_folder_already_exist.has_value())
        {
            return prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "Internal server error occured");
        }

        // Folder with given name already exists
        if (does_folder_already_exist.value())
        {
            return prepare_error_response(
                response, 
                http::status::conflict, 
                "Folder with this name already exists");
        }

        size_t user_id;
        std::string user_name;

        // Get user's id and name claims from the access token
        jwt_utils::get_token_claim(std::string{request.access_token}, "userId", user_id);
        jwt_utils::get_token_claim(std::string{request.access_token}, "nickname", user_name);

        // Insert folder with given name and get a pair of json with data of this folder and 
        // the folder path to create directory in filesystem
        std::optional<std::pair<json::object, std::string>> folder_data_opt = db_conn->insert_folder(
            user_id,
            user_name,
            folder_name);

        // An error occured with database connection
        if (!folder_data_opt.has_value())
        {
            return prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "Internal server error occured");
        }

        try
        {
            // Create folder in file system
            std::filesystem::create_directory(folder_data_opt->second);
        }
        catch (const std::exception& ex)
        {
            LOG_ERROR<< ex.what();

            db_conn->delete_folders({folder_data_opt->first.at("id").to_number<size_t>()});

            return prepare_error_response(
                response,
                http::status::internal_server_error,
                "Internal server error occured");
        }

        // Initialize response
        response.body = json::serialize(folder_data_opt->first);
    }
    catch (const std::exception&)
    {
        return prepare_error_response(
            response, 
            http::status::unprocessable_entity, 
            "Invalid body format");
    }
}

void request_handlers::file_system::delete_folders(const request_params& request, response_params& response)
{
    std::vector<size_t> folder_ids;

    if (!http_utils::uri::get_query_parameter(request.uri, "folderIds", folder_ids))
    {
        return prepare_error_response(
            response,
            http::status::unprocessable_entity, 
            "Invalid folder ids");
    }

    auto db_conn = database_connections_pool::get<file_system_database_connection>();

    // No available connections
    if (!db_conn)
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "No available database connections");
    }
    
    // Try to delete given folders and get a pair of ids and paths of successfully deleted folders
    std::optional<std::pair<std::vector<size_t>, std::vector<std::string>>> deleted_folders_data_opt = 
        db_conn->delete_folders(folder_ids); 

    // An error occured with database connection
    if (!deleted_folders_data_opt.has_value())
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "Internal server error occured");
    }

    try
    {
        // After folders deletion from the database we delele them from the filesystem
        for (std::string& deleted_folder_path : deleted_folders_data_opt->second)
        {
            std::filesystem::remove_all(deleted_folder_path);
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();

        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "Internal server error occured");
    }

    // Not all folders were deleted
    if (deleted_folders_data_opt->first.size() != folder_ids.size())
    {
        response.status = http::status::unprocessable_entity;
        response.body = json::serialize(
            json::object
            {
                {"error", "Some folders haven't been deleted"},
                {"deletedFolderIds", json::array(
                    deleted_folders_data_opt->first.begin(), 
                    deleted_folders_data_opt->first.end())}
            });
    }
}

void request_handlers::file_system::rename_folder(const request_params& request, response_params& response)
{
    try
    {
        size_t folder_id;

        if (!http_utils::uri::get_path_parameter(request.uri, request.uri_template, "folderId", folder_id))
        {
            return prepare_error_response(
                response, 
                http::status::unprocessable_entity, 
                "Invalid folder id");
        }
        
        json::object body_json = json::parse(request.body).as_object();

        std::string new_folder_name = json::parse(request.body).as_object().at("newFolderName").as_string().c_str();

        // Trim whitespaces at the beggining and at the end of the folder name
        boost::algorithm::trim(new_folder_name);

        // Transform the string to UnicodeString to check the actual length of unicode symbols
        icu::UnicodeString new_folder_name_unicode{new_folder_name.c_str()}; 

        // Folder name can't be less than 3 and longer than 64 symbols
        if (new_folder_name_unicode.length() < 3 || new_folder_name_unicode.length() > 64) 
        {
            return prepare_error_response(
                response, 
                http::status::unprocessable_entity, 
                "Invalid new folder name format");
        }

        auto db_conn = database_connections_pool::get<file_system_database_connection>();

        // No available connections
        if (!db_conn)
        {
            return prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "No available database connections");
        }

        std::optional<bool> does_folder_already_exist = db_conn->check_folder_existence_by_name(new_folder_name);

        // An error occured with database connection
        if (!does_folder_already_exist.has_value())
        {
            return prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "Internal server error occured");
        }

        // Folder with given name already exists
        if (does_folder_already_exist.value())
        {
            return prepare_error_response(
                response, 
                http::status::conflict, 
                "Folder with this name already exists");
        }

        // Rename folder by its id
        std::optional<bool> is_folder_id_valid_opt = db_conn->rename_folder(folder_id, new_folder_name);

        // An error occured with database connection
        if (!is_folder_id_valid_opt.has_value())
        {
            return prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "Internal server error occured");
        }

        // Folder with given id doesn't exist
        if (!is_folder_id_valid_opt.value())
        {
            return prepare_error_response(
                response, 
                http::status::not_found, 
                "Folder was not found");
        }
    }
    catch (const std::exception&)
    {
        return prepare_error_response(
            response, 
            http::status::unprocessable_entity, 
            "Invalid body format");
    }  
}

void request_handlers::file_system::get_files_info(const request_params& request, response_params& response)
{
    size_t folder_id;

    if (!http_utils::uri::get_query_parameter(request.uri, "folderId", folder_id))
    {
        return prepare_error_response(
            response,
            http::status::unprocessable_entity, 
            "Invalid folder id");
    }

    auto db_conn = database_connections_pool::get<file_system_database_connection>();

    // No available connections
    if (!db_conn)
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "No available database connections");
    }
    
    // Get all files info in json format
    std::optional<json::object> files_info_json_opt = db_conn->get_files_info(folder_id); 

    // An error occured with database connection
    if (!files_info_json_opt.has_value())
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "Internal server error occured");
    }

    // Folder with given id doesn't exist
    if (files_info_json_opt->empty())
    {
        return prepare_error_response(
            response, 
            http::status::not_found, 
            "Folder was not found");
    }
    
    // Initialize body with string representation of json
    response.body = json::serialize(files_info_json_opt.value());
}

void request_handlers::file_system::unzip_archives(
    std::list<std::tuple<size_t, std::filesystem::path, std::string>>& files_data,
    size_t user_id,
    size_t folder_id,
    database_connection_wrapper<file_system_database_connection>& db_conn)
{
    for (auto file_data_it = files_data.cbegin(); file_data_it != files_data.cend();)
    {
        // Check if the file has one of the allowed archive extensions
        if (config::ALLOWED_ARCHIVE_EXTENSIONS.contains(std::get<2>(*file_data_it)))
        {
            if (!db_conn->change_file_status(std::get<0>(*file_data_it), file_status::unzipping))
            {
                continue;
            }
            
            // Create temporary folder with unique name to extract the archive files into it
            std::filesystem::path temp_archive_folder_path = 
                std::get<1>(*file_data_it).parent_path() /
                std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());

            try
            {
                std::filesystem::create_directory(temp_archive_folder_path);
            }
            catch (const std::exception& ex)
            {
                LOG_ERROR << ex.what();
                continue;
            }

            try
            {
                static bit7z::Bit7zLibrary lib_7zip{config::PATH_TO_7ZIP_LIB};
                bit7z::BitArchiveReader archive_reader{lib_7zip, std::get<1>(*file_data_it)};
                
                // Don't retain archive structure to extract only files without nested folders
                archive_reader.setRetainDirectories(false);

                // Process all the archive files separately
                for (const auto& archive_item : archive_reader.items())
                {
                    // Don't do anything with folders and files with invalid extensions 
                    if (archive_item.isDir() || 
                        !config::ALLOWED_PARSING_FILE_EXTENSIONS.contains(archive_item.extension()))
                    {
                        continue;
                    }

                    // Unzip file to the prepared folder
                    archive_reader.extractTo(temp_archive_folder_path, {archive_item.index()});

                    // Get the file name without extension
                    std::string file_name = archive_item.name().erase(archive_item.name().find_last_of('.'));
                    std::optional<bool> does_file_exist_opt;
                    size_t opening_bracket, copy_number, non_digit_pos;

                    // Check if the file with specified name exists in database
                    // If so we create names with copies by adding number after it: 
                    // test.txt -> test(1).txt -> test(2).txt until the modified name is available in database
                    while (true)    
                    {
                        does_file_exist_opt = db_conn->check_file_existence_by_name(folder_id, file_name);

                        if (!does_file_exist_opt.has_value())
                        {
                            throw bit7z::BitException{"", std::error_code{}};
                        }

                        // We found the available file name so stop creating copies
                        if (!does_file_exist_opt.value())
                        {
                            break;
                        }

                        // If couldn't find opening and closing brackets then there is no copy yet
                        // so just append '(1)' to the end of the file name
                        if (file_name.back() == ')' && 
                            (opening_bracket = file_name.rfind('(', file_name.size() - 2)) != std::string::npos)
                        {
                            try
                            {
                                // Try to consider data between opening and closing brackets as copy number
                                copy_number = std::stoull(
                                    file_name.substr(opening_bracket + 1, file_name.size() - opening_bracket - 2), 
                                    &non_digit_pos);

                                // Copy number string contains non digits after valid number
                                if (non_digit_pos != 
                                    file_name.substr(
                                        opening_bracket + 1, file_name.size() - opening_bracket - 2).size())
                                {
                                    throw std::exception{};
                                }

                                // If copy number is valid then just increment it by one in the file name
                                file_name.replace(
                                    opening_bracket + 1, 
                                    file_name.size() - opening_bracket - 2, 
                                    std::to_string(copy_number + 1));
                            }
                            // If we got here then the copy number is not valid(e.g. the brackets don't represent 
                            // the copy number but just are the part of the file name data - 'test(modified).txt') 
                            // so just append the '(1)' to the end of the file name
                            catch (const std::exception&)
                            {
                                file_name.append("(1)");
                            }
                        }
                        else
                        {
                            file_name.append("(1)");
                        }       
                    }

                    std::optional<std::tuple<size_t, std::filesystem::path, std::string>> unzipped_file_data_opt = 
                        db_conn->insert_unzipped_file(
                            user_id,
                            folder_id,
                            file_name,
                            archive_item.extension(),
                            archive_item.size());

                    if (!unzipped_file_data_opt.has_value())
                    {
                        break;
                    }

                    // Rename unzipped file to the specific name got from the database
                    // and move it out from the temporary folder
                    try
                    {
                        std::filesystem::rename(
                            temp_archive_folder_path / archive_item.name(), 
                            std::get<1>(unzipped_file_data_opt.value()));
                    }
                    catch (const std::exception& ex)
                    {
                        LOG_ERROR << ex.what();

                        db_conn->delete_file(std::get<0>(unzipped_file_data_opt.value()));

                        break;
                    }

                    // Add unzipped file to the list to recode it with others afterward
                    files_data.insert(file_data_it, std::move(unzipped_file_data_opt.value()));
                }
            }
            catch (const bit7z::BitException&)
            {}
            
            // Remove the temporary archive folder from the file system
            try
            {
                std::filesystem::remove_all(temp_archive_folder_path);
            }
            catch (const std::exception& ex)
            {
                LOG_ERROR << ex.what();
            }

            // After processing an archive it has to be deleted because we don't need it anymore
            // as we added unzipped files instead of it

            // Remove the archive from the file system
            try
            {
                std::filesystem::remove(std::get<1>(*file_data_it));
            }
            catch (const std::exception& ex)
            {
                LOG_ERROR << ex.what();
            }
            
            // Delete the archive from the database
            db_conn->delete_file(std::get<0>(*file_data_it));

            ++file_data_it;
            // Delete the archive from the list of files to be recoded afterward
            files_data.erase(std::prev(file_data_it));
        }
        else
        {
            ++file_data_it;
        }
    }
}

void request_handlers::file_system::convert_files_to_csv(
    std::list<std::tuple<size_t, std::filesystem::path, std::string>>& files_data,
    database_connection_wrapper<file_system_database_connection>& db_conn)
{
    for (auto& file_data : files_data)
    {
        // Convert files with all extensions except csv
        if (std::get<2>(file_data) == "csv")
        {
            db_conn->change_file_status(std::get<0>(file_data), file_status::ready_for_parsing);
            
            continue;
        }
    
        db_conn->change_file_status(std::get<0>(file_data), file_status::converting);
        
        // Converted csv file will have the same path but csv extension
        std::filesystem::path new_file_path = std::get<1>(file_data);
        new_file_path.replace_extension("csv");

        // Try to convert file to csv
        // If successful then change file extension and status 
        // otherwise remove file from the database and filesystem 
        if (file_types_conversion::convert_file_to_csv(
            std::get<2>(file_data), 
            std::get<1>(file_data), 
            new_file_path))
        {
            size_t new_file_size;

            try
            {
                new_file_size = std::filesystem::file_size(new_file_path);
            }
            catch (const std::exception& ex)
            {
                LOG_ERROR << ex.what();
            }
            
            db_conn->update_converted_file(std::get<0>(file_data), "csv", new_file_path.c_str(), new_file_size);

            db_conn->change_file_status(std::get<0>(file_data), file_status::ready_for_parsing);
        }
        else
        {
            try
            {
                std::filesystem::remove(std::get<1>(file_data));
            }
            catch (const std::exception& ex)
            {
                LOG_ERROR << ex.what();
            }
            
            db_conn->delete_file(std::get<0>(file_data));
        }
    }
}

void request_handlers::file_system::process_uploaded_files(
    std::list<std::tuple<size_t, std::filesystem::path, std::string>>&& files_data,
    size_t user_id,
    size_t folder_id,
    database_connection_wrapper<file_system_database_connection>&& db_conn)
{
    // Unzip all the archives to get actual files
    unzip_archives(files_data, user_id, folder_id, db_conn);

    // Convert xlsx, sql and txt files to csv
    convert_files_to_csv(files_data, db_conn);

    // for (auto& file_data : files_data)
    // {
    //     db_conn->change_file_status(std::get<0>(file_data), file_status::ready_for_parsing);
    // }
}

void request_handlers::file_system::delete_files(const request_params& request, response_params& response)
{
    std::vector<size_t> file_ids;

    if (!http_utils::uri::get_query_parameter(request.uri, "fileIds", file_ids))
    {
        return prepare_error_response(
            response,
            http::status::unprocessable_entity, 
            "Invalid file ids");
    }

    auto db_conn = database_connections_pool::get<file_system_database_connection>();

    // No available connections
    if (!db_conn)
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "No available database connections");
    }
    
    // Try to delete given files and get a pair of ids and paths of successfully deleted files
    std::optional<std::pair<std::vector<size_t>, std::vector<std::string>>> deleted_files_data_opt = 
        db_conn->delete_files(file_ids); 

    // An error occured with database connection
    if (!deleted_files_data_opt.has_value())
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "Internal server error occured");
    }

    try
    {
        // After files deletion from the database we delele them from the filesystem
        for (std::string &deleted_file_path : deleted_files_data_opt->second)
        {
            std::filesystem::remove(deleted_file_path);
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();

        return prepare_error_response(
            response,
            http::status::internal_server_error,
            "Internal server error occured");
    }

    // Not all files were deleted
    if (deleted_files_data_opt->first.size() != file_ids.size())
    {
        response.status = http::status::unprocessable_entity;
        response.body = json::serialize(
            json::object
            {
                {"error", "Some files haven't been deleted"},
                {"deletedFileIds", json::array(
                    deleted_files_data_opt->first.begin(), 
                    deleted_files_data_opt->first.end())}
            });
    }
}

void request_handlers::file_system::rename_file(const request_params& request, response_params& response)
{
    try
    {
        size_t file_id;

        if (!http_utils::uri::get_path_parameter(request.uri, request.uri_template, "fileId", file_id))
        {
            return prepare_error_response(
                response, 
                http::status::unprocessable_entity, 
                "Invalid file id");
        }
        
        json::object body_json = json::parse(request.body).as_object();

        std::string new_file_name = json::parse(request.body).as_object().at("newFileName").as_string().c_str();

        // Trim whitespaces at the beggining and at the end of the file name
        boost::algorithm::trim(new_file_name);

        // Transform the string to UnicodeString to check the actual length of unicode symbols
        icu::UnicodeString new_file_name_unicode{new_file_name.c_str()}; 

        // File name can't be less than 3 and longer than 64 symbols
        if (new_file_name_unicode.length() < 3 || new_file_name_unicode.length() > 64) 
        {
            return prepare_error_response(
                response, 
                http::status::unprocessable_entity, 
                "Invalid new file name format");
        }

        auto db_conn = database_connections_pool::get<file_system_database_connection>();

        // No available connections
        if (!db_conn)
        {
            return prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "No available database connections");
        }

        std::optional<size_t> folder_id_opt = db_conn->get_folder_id_by_file_id(file_id);

        // An error occured with database connection
        if (!folder_id_opt.has_value())
        {
            return prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "Internal server error occured");
        }

        // File with given id doesn't exist
        if (folder_id_opt.value() == 0)
        {
            return prepare_error_response(
                response, 
                http::status::not_found, 
                "File was not found");
        }

        std::optional<bool> does_file_already_exist = db_conn->check_file_existence_by_name(
            folder_id_opt.value(), 
            new_file_name);

        // An error occured with database connection
        if (!does_file_already_exist.has_value())
        {
            return prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "Internal server error occured");
        }

        // File with given name already exists in its folder
        if (does_file_already_exist.value())
        {
            return prepare_error_response(
                response,
                http::status::conflict, 
                "File with this name already exists in its folder");
        }

        // An error occured with database connection
        if (!db_conn->rename_file(file_id, new_file_name).has_value())
        {
            return prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "Internal server error occured");
        }
    }
    catch (const std::exception&)
    {
        return prepare_error_response(
            response, 
            http::status::unprocessable_entity, 
            "Invalid body format");
    }  
}