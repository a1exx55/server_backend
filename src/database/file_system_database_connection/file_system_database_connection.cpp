#include <database/file_system_database_connection/file_system_database_connection.hpp>

bool file_system_database_connection::check_folder_existence_by_name_impl(
    pqxx::work& transaction, 
    std::string_view folder_name)
{
    // Check if the folder with given name already exists
    return transaction.query_value<bool>(
        std::format(    
            "SELECT EXISTS"
                "(SELECT 1 FROM folders "
                "WHERE name={})",
            transaction.quote(folder_name)));
}

bool file_system_database_connection::check_file_existence_by_name_impl(
    pqxx::work& transaction, 
    size_t folder_id, 
    std::string_view file_name)
{
    // Check if the file with given name already exists
    return transaction.query_value<bool>(
        std::format(
            "SELECT EXISTS"
                "(SELECT 1 FROM files "
                "WHERE folder_id={} AND name={})",
            folder_id,
            transaction.quote(file_name)));
}

std::optional<json::array> file_system_database_connection::get_folders_info()
{
    pqxx::work transaction{*_conn};
    
    try
    {
        json::array folders_array;
        json::object folder_json;

        for (auto [id, name, last_upload_date, created_by, files_number] : 
            transaction.query<size_t, std::string, std::optional<std::string>, std::string, size_t>(
                "SELECT folders.id,folders.name,folders.last_upload_date,users.nickname,folders.files_number "
                "FROM folders "
                "JOIN users ON folders.created_by_user_id=users.id"))
        {
            folder_json = 
                json::object
                {
                    {"id", id},
                    {"name", name},
                    {"createdBy", created_by},
                    {"filesNumber", files_number}
                };

            // Last upload date may be null so process it separately
            if (last_upload_date.has_value())
            {
                folder_json.emplace("lastUploadDate", last_upload_date.value());
            }
            else
            {
                folder_json.emplace("lastUploadDate", nullptr);
            }

            folders_array.emplace_back(folder_json);
        }   
        
        transaction.commit();

        return folders_array;
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return get_folders_info();
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<bool> file_system_database_connection::check_folder_existence_by_name(std::string_view folder_name)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        return check_folder_existence_by_name_impl(
            transaction,
            folder_name);
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return check_folder_existence_by_name(folder_name);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    }
}

std::optional<std::pair<json::object, std::string>> file_system_database_connection::insert_folder(
    size_t user_id, 
    std::string_view user_name, 
    std::string_view folder_name)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        // Insert folder with given data
        auto [folder_id, folder_path] = transaction.query1<size_t, std::string>(
            std::format(
                "WITH current_id AS (SELECT nextval('folders_id_seq')) "
                    "INSERT INTO folders (id,name,path,created_by_user_id) "
                    "VALUES ((SELECT * FROM current_id),{},{} || (SELECT * FROM current_id)::text || '/',{}) "
                    "RETURNING id,path",
                transaction.quote(folder_name),
                transaction.quote(config::FOLDERS_PATH),
                user_id));

        json::object folder_data_json;

        // Construct json with data of newly inserted folder
        folder_data_json.emplace("id", folder_id);
        folder_data_json.emplace("name", folder_name);
        folder_data_json.emplace("lastUploadDate", nullptr);
        folder_data_json.emplace("createdBy", user_name);
        folder_data_json.emplace("filesNumber", 0);

        transaction.commit();

        return std::pair{folder_data_json, folder_path};
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return insert_folder(user_id, user_name, folder_name);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<std::pair<std::vector<size_t>, std::vector<std::string>>> 
file_system_database_connection::delete_folders(
    const std::vector<size_t>& folder_ids)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        std::vector<size_t> deleted_folder_ids;
        std::vector<std::string> deleted_folder_paths;

        std::string folder_ids_list;
        
        for (size_t folder_id : folder_ids)
        {
            folder_ids_list.append(std::to_string(folder_id) + ",");
        }

        folder_ids_list.erase(folder_ids_list.size() - 1);

        for (auto [deleted_folder_id, deleted_folder_path] : transaction.query<size_t, std::string>(
            std::format(
                "DELETE FROM folders "
                "WHERE id IN ({}) AND NOT "
                    "(SELECT EXISTS"
                        "(SELECT 1 FROM files "
                        "WHERE folder_id=folders.id AND status<>'ready_for_parsing')) "
                "RETURNING id,path",
                folder_ids_list)))
        {
            deleted_folder_ids.emplace_back(deleted_folder_id);
            deleted_folder_paths.emplace_back(std::move(deleted_folder_path));
        }

        transaction.commit();

        return std::pair{deleted_folder_ids, deleted_folder_paths};
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return delete_folders(folder_ids);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<bool> file_system_database_connection::rename_folder(size_t folder_id, std::string_view new_folder_name)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        _result = transaction.exec0(
            std::format(
                "UPDATE folders SET name={} "
                "WHERE id={}",
                transaction.quote(new_folder_name),
                folder_id));

        // Check if the update has occured i.e. folder with given id actually exists
        if (_result.affected_rows())
        {
            transaction.commit();
            return true;
        }
        else
        {
            return false;
        }
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return rename_folder(folder_id, new_folder_name);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<json::object> file_system_database_connection::get_files_info(size_t folder_id)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        // Get the folder name by its id or throw if folder with this id doesn't exist
        std::string folder_name = transaction.query_value<std::string>(
            std::format(
                "SELECT name FROM folders "
                "WHERE id={}",
                folder_id));

        // Create json for files data and add corresponding folder name
        json::object files_data_json
        {
            {"folderName", folder_name}
        };

        files_data_json.emplace("files", json::array{});
        json::array& files_data_array = files_data_json.at("files").as_array();

        json::object file_data_json;

        for (auto [id, name, size, upload_date, uploaded_by, status] : 
            transaction.query<size_t, std::string, std::optional<size_t>, std::optional<std::string>, std::string, std::string>(
                std::format(
                    "SELECT files.id,files.name ||'.'|| files.extension,files.size,files.upload_date,"
                        "users.nickname,files.status FROM files "
                    "JOIN users ON files.uploaded_by_user_id=users.id " 
                    "WHERE files.folder_id={}",
                    folder_id)))
        {
            file_data_json = 
                json::object
                {
                    {"id", id},
                    {"name", name},
                    {"uploadedBy", uploaded_by},
                    {"status", status}
                };

            // Size and upload date may be null so process them separately
            if (size.has_value())
            {
                file_data_json.emplace("size", size.value());
            }
            else
            {
                file_data_json.emplace("size", nullptr);
            }

            if (upload_date.has_value())
            {
                file_data_json.emplace("uploadDate", upload_date.value());
            }
            else
            {
                file_data_json.emplace("uploadDate", nullptr);
            }

            files_data_array.emplace_back(file_data_json);
        }   
        
        transaction.commit();

        return files_data_json;
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return get_files_info(folder_id);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    // Folder with given folder_id doesn't exist
    catch (const pqxx::unexpected_rows&)
    {
        return json::object{};
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<std::string> file_system_database_connection::get_file_path(size_t file_id)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        return transaction.query_value<std::string>(
            std::format(
                "SELECT path FROM files "
                "WHERE id={}",
                file_id));
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return get_file_path(file_id);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    // File with given id doesn't exist
    catch (const pqxx::unexpected_rows&)
    {
        return "";
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<bool> file_system_database_connection::check_folder_existence_by_id(size_t folder_id)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        return transaction.query_value<bool>(
            std::format(
                "SELECT EXISTS"
                    "(SELECT 1 FROM folders "
                    "WHERE id={})",
                folder_id));
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return check_folder_existence_by_id(folder_id);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<bool> file_system_database_connection::check_file_existence_by_name(
    size_t folder_id, 
    std::string_view file_name)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        return check_file_existence_by_name_impl(
            transaction,
            folder_id,
            file_name);
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return check_file_existence_by_name(folder_id, file_name);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    }
}

std::optional<std::tuple<size_t, std::filesystem::path, std::string>> 
file_system_database_connection::insert_uploading_file(
    size_t user_id, 
    size_t folder_id,
    std::string_view file_name,
    std::string_view file_extension)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        auto [file_id, file_path] = transaction.query1<size_t, std::string>(
            std::format(
                "WITH current_id AS (SELECT nextval('files_id_seq')) "
                    "INSERT INTO files (id,name,extension,path,folder_id,uploaded_by_user_id) "
                    "VALUES ((SELECT * FROM current_id),{0},{1},{2}||{3}||'/'||(SELECT * FROM current_id)::text||'.'||{1},{3},{4}) "
                    "RETURNING id,path",
                transaction.quote(file_name),
                transaction.quote(file_extension),
                transaction.quote(config::FOLDERS_PATH),
                folder_id,
                user_id));

        transaction.commit();

        return std::tuple<size_t, std::filesystem::path, std::string>{file_id, file_path, file_extension};
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return insert_uploading_file(user_id, folder_id, file_name, file_extension);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    }
}

std::optional<std::monostate> file_system_database_connection::delete_file(size_t file_id)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        transaction.exec0(
            std::format(
                "DELETE FROM files "
                "WHERE id={}",
                file_id));
        
        transaction.commit();

        return std::monostate{};
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return delete_file(file_id);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    }
}

std::optional<std::monostate> file_system_database_connection::update_uploaded_file(size_t file_id, size_t file_size)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        transaction.exec0(
            std::format(
                "UPDATE files SET size={},upload_date=LOCALTIMESTAMP,status='uploaded' "
                "WHERE id={}",
                file_size,
                file_id));
        
        transaction.commit();
    
        return std::monostate{};
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return update_uploaded_file(file_id, file_size);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    }
}

std::optional<std::tuple<size_t, std::filesystem::path, std::string>> 
file_system_database_connection::insert_unzipped_file(
    size_t user_id,
    size_t folder_id,
    std::string_view file_name,
    std::string_view file_extension,
    size_t file_size)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        auto [file_id, file_path] = transaction.query1<size_t, std::string>(
            std::format(
                "WITH current_id AS (SELECT nextval('files_id_seq')) "
                    "INSERT INTO files (id,name,extension,path,folder_id,size,upload_date,uploaded_by_user_id,status) "
                    "VALUES ((SELECT * FROM current_id),{0},{1},{2}||{3}||'/'||(SELECT * FROM current_id)::text||'.'||"
                        "{1},{3},{4},LOCALTIMESTAMP,{5},'uploaded') "
                    "RETURNING id,path",
                transaction.quote(file_name),
                transaction.quote(file_extension),
                transaction.quote(config::FOLDERS_PATH),
                folder_id,
                file_size,
                user_id));

        transaction.commit();

        return std::tuple<size_t, std::filesystem::path, std::string>{file_id, file_path, file_extension};
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return insert_unzipped_file(user_id, folder_id, file_name, file_extension, file_size);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    }
}

std::optional<std::monostate> file_system_database_connection::change_file_status(
    size_t file_id, 
    file_status new_status)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        transaction.exec0(
            std::format(
                "UPDATE files SET status={} " 
                "WHERE id={}",
                transaction.quote(magic_enum::enum_name(new_status)),
                file_id));
        
        transaction.commit();
    
        return std::monostate{};
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return change_file_status(file_id, new_status);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    }
}

std::optional<std::monostate> file_system_database_connection::update_converted_file(
    size_t file_id, 
    std::string_view new_file_extension, 
    std::string_view new_file_path, 
    size_t new_file_size)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        // Update extension and path replacing the old extension to the new one
        transaction.exec0(
            std::format(
                "UPDATE files SET extension={},path={},size={} " 
                "WHERE id={}",
                transaction.quote(new_file_extension),
                transaction.quote(new_file_path),
                new_file_size,
                file_id));
        
        transaction.commit();
    
        return std::monostate{};
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return update_converted_file(file_id, new_file_extension, new_file_path, new_file_size);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    }
}

std::optional<std::pair<std::vector<size_t>, std::vector<std::string>>> 
file_system_database_connection::delete_files(const std::vector<size_t>& file_ids)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        std::vector<size_t> deleted_file_ids;
        std::vector<std::string> deleted_file_paths;

        std::string file_ids_list;
        
        for (size_t file_id : file_ids)
        {
            file_ids_list.append(std::to_string(file_id) + ",");
        }

        file_ids_list.erase(file_ids_list.size() - 1);

        for (auto [deleted_file_id, deleted_file_path] : transaction.query<size_t, std::string>(
            std::format(
                "DELETE FROM files "
                "WHERE id IN ({}) AND status='ready_for_parsing' "
                "RETURNING id,path",
                file_ids_list)))
        {
            deleted_file_ids.emplace_back(deleted_file_id);
            deleted_file_paths.emplace_back(std::move(deleted_file_path));
        }

        transaction.commit();

        return std::pair{deleted_file_ids, deleted_file_paths};
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return delete_files(file_ids);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<size_t> file_system_database_connection::get_folder_id_by_file_id(size_t file_id)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        return transaction.query_value<size_t>(
            std::format(
                "SELECT folder_id FROM files "
                "WHERE id={}",
                file_id));
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return get_folder_id_by_file_id(file_id);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    // File with given id doesn't exist
    catch (const pqxx::unexpected_rows&)
    {
        return 0;
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<bool> file_system_database_connection::rename_file(size_t file_id, std::string_view new_file_name)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        _result = transaction.exec0(
            std::format(
                "UPDATE files SET name={} "
                "WHERE id={}",
                transaction.quote(new_file_name),
                file_id));

        // Check if the update has occured i.e. file with given id actually exists
        if (_result.affected_rows())
        {
            transaction.commit();
            return true;
        }
        else
        {
            return false;
        }
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return rename_file(file_id, new_file_name);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}