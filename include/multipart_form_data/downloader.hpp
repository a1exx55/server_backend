#ifndef MULTIPART_FORM_DATA_DOWNLOADER_HPP
#define MULTIPART_FORM_DATA_DOWNLOADER_HPP

#include <boost/asio/read_until.hpp>
#include <filesystem>
#include <fstream>

#include <multipart_form_data/error.hpp>

namespace multipart_form_data
{
    // Class for downloading files using multipart/form-data protocol.
    template<typename read_stream, typename dynamic_buffer>
    class downloader
    {
        public:
            // Custom settings that can be applied to the downloading process to change its behavior or properties. 
            template<typename ...additional_parameters_t>
            struct settings
            {
                // The size of packets that will be used to read files by chunks.
                //
                // Default packet size is 10 MB.
                size_t packets_size{10 * 1024 * 1024};
                // The waiting time of asynchronous read operations' execution. After expiry of this time 
                // the operation will be canceled and request will be aborted with corresponding error code.
                // Does nothing if used in sync_download
                //
                // Default timeout is 30 seconds.
                std::chrono::steady_clock::duration operations_timeout{std::chrono::seconds(30)};    
                // The directory where the downloaded files will be placed 
                // if on_read_file_header_handler is not defined or it returns empty path.
                //       
                // Default output directory is the current execution one.
                std::filesystem::path output_directory{"."};
                // The function that will be invoked when each file header, containing file metadata, is read.
                // File name is provided as the first function argument. Other arguments are optional and can be provided in download function.
                // Function return value can be used to provide file path to write into.
                // If this handler is not defined or it returns empty path then file will be written into output_directory with unique name.
                // If this handler throw exception then the whole downloading operation is aborted 
                // and multipart_form_data::error::operation_aborted is set in callback.
                std::function<std::filesystem::path(std::string_view, additional_parameters_t&...)> on_read_file_header_handler{};
                // The function that will be invoked when each file body is entirely read and written to filesystem.
                // Path of the output file is provided as the first function argument. Other arguments are optional and can be provided in download function.
                // If this handler throw exception then the whole downloading operation is aborted 
                // and multipart_form_data::error::operation_aborted is set in callback.
                std::function<void(const std::filesystem::path&, additional_parameters_t&...)> on_read_file_body_handler{};
            };
            
            /**
             * @param stream stream that will be used to perform all read operations. 
             * It is stored only reference to the stream, not the actual data.
             * @param buffer dynamic buffer that is used to read request's headers. It is necessary because 
             * boost::beast::http::(async_)read_header obtains some part of body that contains 
             * multipart/form-data details. It is stored only reference to the buffer, not the actual data.
             */
            downloader(
                read_stream& stream, 
                const dynamic_buffer& buffer)
                : 
                _stream{stream},
                _input_buffer{buffer}
            {}

            /**
             * @brief Asynchronously download files using multipart/form-data protocol.
             * 
             * @param content_type value of Content-Type field in request header. It contains important protocol details.
             * @param settings custom settings that can be applied to the downloading process to change its behavior or properties.
             * @param handler callback function that will be invoked in the end of downloading process. It has to
             * respond to the `asio` `completion token` concept with the signature of 
             * `void(boost::beast::error_code, std::vector<std::filesystem::path>&&)` with `additional_parameters`
             * in the end if there are. Error code in `handler` can be set with either `asio` and `beast` 
             * errors or specialized `multipart_form_data::error` errors. Vector of paths in `handler` reflects
             * downloaded files' paths.
             * @param self_ptr pointer to the instance of the caller class to extend its lifetime while the downloading is in progress. 
             * @tparam additional_parameters optional parameters that can be used to transmit necessary data to the settings
             * handlers and to `handler` callback. Although it is used perfect forwarding to pass `additional_parameters`,
             * reference variables have to be wrapped in `std::ref` or `std::cref` to work correctly. `additional_parameters`
             * will be passed to the `handler` as they are, but to the `settings`'s hanlders they will be passed by
             * reference because accepting rvalues would leave them in the first invoked handler.
             */
            template<
                boost::asio::completion_token_for<void(
                    boost::beast::error_code, 
                    std::vector<std::filesystem::path>&&)> handler_t, 
                typename session_t,
                typename ...additional_parameters_t>
            void async_download(
                std::string_view content_type, 
                settings<additional_parameters_t...>&& settings,
                handler_t&& handler, 
                std::shared_ptr<session_t> self_ptr,
                additional_parameters_t&&... additional_parameters)
            {
                // Check if the request is actually multipart/form-data
                if (content_type.find("multipart/form-data") == std::string::npos)
                {
                    return handler(
                        error::not_multipart_form_data_request, 
                        std::vector<std::filesystem::path>{}, 
                        std::forward<additional_parameters_t>(additional_parameters)...);
                }

                async_prepare_files_processing(
                    content_type, 
                    std::move(settings),
                    std::forward<handler_t>(handler), 
                    std::move(self_ptr), 
                    std::forward<additional_parameters_t>(additional_parameters)...);
            }

            /**
             * @brief Synchronously download files using multipart/form-data protocol.
             * 
             * @param content_type value of Content-Type field in request header. It contains important protocol details.
             * @param settings custom settings that can be applied to the downloading process to change its behavior or properties.
             * @param error_code operation status in terms of error codes. It can be set with either `asio` and `beast` 
             * errors or specialized `multipart_form_data::error` errors.
             * @tparam additional_parameters optional parameters that can be used to transmit necessary data to the settings
             * handlers and to `handler` callback. Although it is used perfect forwarding to pass `additional_parameters`,
             * reference variables have to be wrapped in `std::ref` or `std::cref` to work correctly. 
             * `additional_parameters` will be passed to the `settings`'s hanlders by reference 
             * because accepting rvalues would leave them in the first invoked handler.
             * 
             * @return Vector of the downloaded files' paths. 
             */
            template<typename ...additional_parameters_t>
            std::vector<std::filesystem::path> sync_download(
                std::string_view content_type, 
                settings<additional_parameters_t...>&& settings,
                boost::beast::error_code& error_code,
                additional_parameters_t&&... additional_parameters)
            {
                // Check if the request is actually multipart/form-data
                if (content_type.find("multipart/form-data") == std::string::npos)
                {
                    error_code = error::not_multipart_form_data_request;
                    
                    return std::vector<std::filesystem::path>{};
                }

                sync_prepare_files_processing(
                    content_type, 
                    std::move(settings),
                    error_code, 
                    std::forward<additional_parameters_t>(additional_parameters)...);

                return _output_file_paths;
            }
            
        private:
            template<
                boost::asio::completion_token_for<void(
                    boost::beast::error_code, 
                    std::vector<std::filesystem::path>&&)> handler_t, 
                typename session_t,
                typename ...additional_parameters_t>
            void async_prepare_files_processing(
                std::string_view content_type, 
                settings<additional_parameters_t...>&& settings, 
                handler_t&& handler, 
                std::shared_ptr<session_t>&& self_ptr,
                additional_parameters_t&&... additional_parameters)
            {
                // Clear the previous output file paths
                _output_file_paths.clear();

                // Assign buffer storage with input buffer data because it can store some part of the request body
                _buffer_storage.assign(
                    boost::asio::buffers_begin(_input_buffer.data()),
                    boost::asio::buffers_end(_input_buffer.data()));

                // Reinitialize buffer with specified packets size limit
                _buffer.emplace(_buffer_storage, settings.packets_size);

                size_t boundary_position = content_type.find("boundary=");

                // Boundary was not found in the content type
                if (boundary_position == std::string::npos)
                {
                    return handler(
                        error::invalid_structure, 
                        std::vector<std::filesystem::path>{}, 
                        std::forward<additional_parameters_t>(additional_parameters)...);
                }

                // Determine the boundary for multipart/form-data content type
                _boundary = content_type.substr(boundary_position + 9);

                // Set the timeout
                boost::beast::get_lowest_layer(_stream).expires_after(settings.operations_timeout);

                // Read the boundary before the header of the first file 
                boost::asio::async_read_until(_stream, *_buffer, _boundary, 
                    boost::beast::bind_front_handler(
                        [this, self_ptr](
                            downloader::settings<additional_parameters_t...>&& settings,
                            handler_t&& handler,
                            additional_parameters_t&&... additional_parameters,
                            boost::beast::error_code error_code, 
                            std::size_t bytes_transferred) mutable
                        {
                            if (error_code)
                            {
                                return handler(
                                    error_code, 
                                    std::vector<std::filesystem::path>{}, 
                                    std::forward<additional_parameters_t>(additional_parameters)...);
                            }

                            // Consume read bytes as it is just the boundary
                            _buffer->consume(bytes_transferred);

                            // Set the timeout
                            boost::beast::get_lowest_layer(_stream).expires_after(settings.operations_timeout);

                            // Read the first file header obtaining bytes until the empty string 
                            // that represents the delimiter between file header and data itself
                            boost::asio::async_read_until(_stream, *_buffer, "\r\n\r\n", 
                                boost::beast::bind_front_handler(
                                    [this, self_ptr](
                                        downloader::settings<additional_parameters_t...>&& settings,
                                        handler_t&& handler,
                                        additional_parameters_t&&... additional_parameters,
                                        boost::beast::error_code error_code, 
                                        std::size_t bytes_transferred) mutable
                                    {
                                        async_process_file_header(
                                            std::move(settings),
                                            std::forward<handler_t>(handler), 
                                            std::move(self_ptr), 
                                            error_code, 
                                            bytes_transferred,
                                            std::forward<additional_parameters_t>(additional_parameters)...);
                                    },
                                    std::move(settings),
                                    std::forward<handler_t>(handler),
                                    std::forward<additional_parameters_t>(additional_parameters)...));
                        },
                        std::move(settings),
                        std::forward<handler_t>(handler),
                        std::forward<additional_parameters_t>(additional_parameters)...));
            }

            template<
                boost::asio::completion_token_for<void(
                    boost::beast::error_code, 
                    std::vector<std::filesystem::path>&&)> handler_t, 
                typename session_t,
                typename ...additional_parameters_t>
            void async_process_file_header(
                settings<additional_parameters_t...>&& settings,
                handler_t&& handler, 
                std::shared_ptr<session_t>&& self_ptr,
                boost::beast::error_code error_code, 
                std::size_t bytes_transferred,
                additional_parameters_t&&... additional_parameters)
            {
                if (error_code)
                {
                    // Reset the timeout
                    boost::beast::get_lowest_layer(_stream).expires_never();
                    
                    return handler(
                        error_code, 
                        std::move(_output_file_paths), 
                        std::forward<additional_parameters_t>(additional_parameters)...);
                }

                // Construct the string representation of obtained file header
                std::string_view file_header_data{
                    _buffer_storage.data(),
                    bytes_transferred};

                // Position of the filename field in the file header
                size_t file_name_position = file_header_data.find("filename=\"");

                // filename field is absent
                if (file_name_position == std::string::npos)
                {
                    // Reset the timeout
                    boost::beast::get_lowest_layer(_stream).expires_never();
                    
                    return handler(
                        error::invalid_structure, 
                        std::move(_output_file_paths), 
                        std::forward<additional_parameters_t>(additional_parameters)...);
                }

                // Remove the data before the actual file name
                file_header_data.remove_prefix(file_name_position + 10);

                // Look for the end of the file name
                // Find from the end because actual file name can contain double quotes
                file_name_position = file_header_data.rfind('"');

                // No closing double quote in filename
                if (file_name_position == std::string::npos)
                {
                    // Reset the timeout
                    boost::beast::get_lowest_layer(_stream).expires_never();
                    
                    return handler(
                        error::invalid_structure, 
                        std::move(_output_file_paths), 
                        std::forward<additional_parameters_t>(additional_parameters)...);
                }

                // Get the actual file name
                file_header_data.remove_suffix(file_header_data.size() - file_name_position);

                if (settings.on_read_file_header_handler)
                {
                    try
                    {
                        _file_path = settings.on_read_file_header_handler(file_header_data, additional_parameters...);
                    }
                    catch (...)
                    {
                        return handler(
                            error::operation_aborted, 
                            std::move(_output_file_paths), 
                            std::forward<additional_parameters_t>(additional_parameters)...);
                    }
                    
                    if (_file_path.empty())
                    {
                        if (!generate_file_path(settings.output_directory, file_header_data, error_code))
                        {
                            return handler(
                                error_code, 
                                std::move(_output_file_paths), 
                                std::forward<additional_parameters_t>(additional_parameters)...);
                        }
                    }
                }
                else
                {
                    if (!generate_file_path(settings.output_directory, file_header_data, error_code))
                    {
                        return handler(
                            error_code, 
                            std::move(_output_file_paths), 
                            std::forward<additional_parameters_t>(additional_parameters)...);
                    }
                }

                // Open the file to write the obtaining data
                _file.open(_file_path, std::ios::binary);

                // Invalid file path was provided
                if (!_file.is_open())
                {
                    // Reset the timeout
                    boost::beast::get_lowest_layer(_stream).expires_never();
                     
                    return handler(
                        error::invalid_file_path, 
                        std::move(_output_file_paths), 
                        std::forward<additional_parameters_t>(additional_parameters)...);
                }
                
                // Store provided file path
                _output_file_paths.emplace_back(_file_path);

                // Consume the file header bytes 
                _buffer->consume(bytes_transferred);

                // Set the timeout
                boost::beast::get_lowest_layer(_stream).expires_after(settings.operations_timeout);

                // Read the file body obtaining bytes until the boundary that represents the end of file
                boost::asio::async_read_until(_stream, *_buffer, _boundary,
                    boost::beast::bind_front_handler(
                        [this, self_ptr](
                            downloader::settings<additional_parameters_t...>&& settings,
                            handler_t&& handler,
                            additional_parameters_t&&... additional_parameters,
                            boost::beast::error_code error_code, 
                            std::size_t bytes_transferred) mutable
                        {
                            async_process_file_body(
                                std::move(settings),
                                std::forward<handler_t>(handler), 
                                std::move(self_ptr), 
                                error_code, 
                                bytes_transferred,
                                std::forward<additional_parameters_t>(additional_parameters)...);
                        },
                        std::move(settings),
                        std::forward<handler_t>(handler),
                        std::forward<additional_parameters_t>(additional_parameters)...));
            }

            template<
                boost::asio::completion_token_for<void(
                    boost::beast::error_code, 
                    std::vector<std::filesystem::path>&&)> handler_t, 
                typename session_t,
                typename ...additional_parameters_t>
            void async_process_file_body(
                settings<additional_parameters_t...>&& settings,
                handler_t&& handler, 
                std::shared_ptr<session_t>&& self_ptr,
                boost::beast::error_code error_code, 
                std::size_t bytes_transferred,
                additional_parameters_t&&... additional_parameters)
            {
                // File can't be read at once as it is too big(more than settings.packets_size bytes)
                // Process obtained packet and go on reading
                if (error_code == boost::asio::error::not_found)
                {
                    // Write obtained packet to the file
                    // Don't touch last symbols with boundary length as we could stop in the middle of boundary
                    // so we would write the part of boundary to the file
                    _file.write(
                        _buffer_storage.data(),
                        _buffer_storage.size() - _boundary.size());

                    // Consume written bytes
                    _buffer->consume(_buffer_storage.size() - _boundary.size());

                    // Set the timeout
                    boost::beast::get_lowest_layer(_stream).expires_after(settings.operations_timeout);

                    // Read the next data until either we find a boundary or read the packet of maximum size again 
                    return boost::asio::async_read_until(_stream, *_buffer, _boundary, 
                        boost::beast::bind_front_handler(
                            [this, self_ptr](
                                downloader::settings<additional_parameters_t...>&& settings,
                                handler_t&& handler,
                                additional_parameters_t&&... additional_parameters,
                                boost::beast::error_code error_code, 
                                std::size_t bytes_transferred) mutable
                            {
                                async_process_file_body(
                                    std::move(settings),
                                    std::forward<handler_t>(handler), 
                                    std::move(self_ptr), 
                                    error_code, 
                                    bytes_transferred,
                                    std::forward<additional_parameters_t>(additional_parameters)...);
                            },
                            std::move(settings),
                            std::forward<handler_t>(handler),
                            std::forward<additional_parameters_t>(additional_parameters)...));
                }

                // Unexpected error occured so clean up everything about not uploaded file
                if (error_code)
                {
                    // Reset the timeout
                    boost::beast::get_lowest_layer(_stream).expires_never();
                    
                    _file.close();

                    // Remove the file from the file system
                    try
                    {
                        std::filesystem::remove(_output_file_paths.back());
                    }
                    catch (const std::exception& ex)
                    {}

                    // Remove the file from the list of uploaded files 
                    _output_file_paths.pop_back();

                    return handler(
                        error_code, 
                        std::move(_output_file_paths), 
                        std::forward<additional_parameters_t>(additional_parameters)...);
                }

                // Write obtained bytes to the file excluding CRLF after the file data and -- followed by boundary
                // -- is the part of the boundary, used only in body, so we have to consider this -- length because
                // _boudary variable doesn't contain it
                _file.write(_buffer_storage.data(), bytes_transferred - _boundary.size() - 4);

                // Close the file as its uploading is over
                _file.close();

                // Invoke handler after reading the whole file body if it is defined
                if (settings.on_read_file_body_handler)
                {
                    try
                    {
                        settings.on_read_file_body_handler(_output_file_paths.back(), additional_parameters...);
                    }
                    catch (...)
                    {
                        return handler(
                            error::operation_aborted, 
                            std::move(_output_file_paths), 
                            std::forward<additional_parameters_t>(additional_parameters)...);
                    }
                }

                // Consume obtained bytes
                _buffer->consume(bytes_transferred); 

                // If there is "--" after the boundary then there are no more files and request body is over
                if (std::string_view{_buffer_storage.data(), _buffer_storage.size()} == "--\r\n")
                {
                    // Reset the timeout
                    boost::beast::get_lowest_layer(_stream).expires_never();
                   
                    return handler(
                        error_code, 
                        std::move(_output_file_paths), 
                        std::forward<additional_parameters_t>(additional_parameters)...);
                }
                
                // Set the timeout
                boost::beast::get_lowest_layer(_stream).expires_after(settings.operations_timeout);
                
                // Read the next file header
                boost::asio::async_read_until(_stream, *_buffer, "\r\n\r\n", 
                    boost::beast::bind_front_handler(
                        [this, self_ptr](
                            downloader::settings<additional_parameters_t...>&& settings,
                            handler_t&& handler,
                            additional_parameters_t&&... additional_parameters,
                            boost::beast::error_code error_code, 
                            std::size_t bytes_transferred) mutable
                        {
                            async_process_file_header(
                                std::move(settings),
                                std::forward<handler_t>(handler), 
                                std::move(self_ptr), 
                                error_code, 
                                bytes_transferred,
                                std::forward<additional_parameters_t>(additional_parameters)...);
                        },
                        std::move(settings),
                        std::forward<handler_t>(handler),
                        std::forward<additional_parameters_t>(additional_parameters)...));
            }

            template<typename ...additional_parameters_t>
            void sync_prepare_files_processing(
                std::string_view content_type, 
                settings<additional_parameters_t...>&& settings, 
                boost::beast::error_code& error_code,
                additional_parameters_t&&... additional_parameters)
            {
                // Clear the previous output file paths
                _output_file_paths.clear();

                // Assign buffer storage with input buffer data because it can store some part of the request body
                _buffer_storage.assign(
                    boost::asio::buffers_begin(_input_buffer.data()),
                    boost::asio::buffers_end(_input_buffer.data()));

                // Reinitialize buffer with specified packets size limit
                _buffer.emplace(_buffer_storage, settings.packets_size);

                size_t boundary_position = content_type.find("boundary=");

                // Boundary was not found in the content type
                if (boundary_position == std::string::npos)
                {
                    error_code = error::invalid_structure;
                    _output_file_paths = {};

                    return;
                }

                // Determine the boundary for multipart/form-data content type
                _boundary = content_type.substr(boundary_position + 9);

                // Read the boundary before the header of the first file 
                std::size_t bytes_transferred =  boost::asio::read_until(_stream, *_buffer, _boundary, error_code);

                if (error_code)
                {
                    _output_file_paths = std::vector<std::filesystem::path>{};

                    return;
                }

                // Consume read bytes as it is just the boundary
                _buffer->consume(bytes_transferred);

                // Read the first file header obtaining bytes until the empty string 
                // that represents the delimiter between file header and data itself
                bytes_transferred = boost::asio::read_until(_stream, *_buffer, "\r\n\r\n", error_code);

                sync_process_file_header(
                    std::move(settings), 
                    error_code, 
                    bytes_transferred, 
                    std::forward<additional_parameters_t>(additional_parameters)...);
            }

            template<typename ...additional_parameters_t>
            void sync_process_file_header(
                settings<additional_parameters_t...>&& settings,
                boost::beast::error_code& error_code, 
                std::size_t bytes_transferred,
                additional_parameters_t&&... additional_parameters)
            {
                if (error_code)
                {
                    return;
                }

                // Construct the string representation of obtained file header
                std::string_view file_header_data{
                    _buffer_storage.data(),
                    bytes_transferred};

                // Position of the filename field in the file header
                size_t file_name_position = file_header_data.find("filename=\"");

                // filename field is absent
                if (file_name_position == std::string::npos)
                {
                    error_code = boost::asio::error::not_found;

                    return;
                }

                // Remove the data before the actual file name
                file_header_data.remove_prefix(file_name_position + 10);

                // Look for the end of the file name
                // Find from the end because actual file name can contain double quotes
                file_name_position = file_header_data.rfind('"');

                // No closing double quote in filename
                if (file_name_position == std::string::npos)
                {
                    error_code = error::invalid_structure;

                    return;
                }

                // Get the actual file name
                file_header_data.remove_suffix(file_header_data.size() - file_name_position);

                if (settings.on_read_file_header_handler)
                {
                    try
                    {
                        _file_path = settings.on_read_file_header_handler(file_header_data, additional_parameters...);
                    }
                    catch (...)
                    {
                        error_code = error::operation_aborted;

                        return;
                    }

                    if (_file_path.empty())
                    {
                        if (!generate_file_path(settings.output_directory, file_header_data, error_code))
                        {
                            return;
                        }
                    }
                }
                else
                {
                    if (!generate_file_path(settings.output_directory, file_header_data, error_code))
                    {
                        return;
                    }
                }

                // Open the file to write the obtaining data
                _file.open(_file_path, std::ios::binary);

                // Invalid file path was provided
                if (!_file.is_open())
                {
                    error_code = error::invalid_file_path;

                    return;
                }
                
                // Store provided file path
                _output_file_paths.emplace_back(_file_path);

                // Consume the file header bytes 
                _buffer->consume(bytes_transferred);

                // Read the file body obtaining bytes until the boundary that represents the end of file
                bytes_transferred = boost::asio::read_until(_stream, *_buffer, _boundary, error_code);

                sync_process_file_body(
                    std::move(settings), 
                    error_code, 
                    bytes_transferred, 
                    std::forward<additional_parameters_t>(additional_parameters)...);
            }

            template<typename ...additional_parameters_t>
            void sync_process_file_body(
                settings<additional_parameters_t...>&& settings,
                boost::beast::error_code& error_code,
                std::size_t bytes_transferred,
                additional_parameters_t&&... additional_parameters)
            {
                // File can't be read at once as it is too big(more than settings.packets_size bytes)
                // Process obtained packet and go on reading
                if (error_code == boost::asio::error::not_found)
                {
                    // Write obtained packet to the file
                    // Don't touch last symbols with boundary length as we could stop in the middle of boundary
                    // so we would write the part of boundary to the file
                    _file.write(
                        _buffer_storage.data(),
                        _buffer_storage.size() - _boundary.size());

                    // Consume written bytes
                    _buffer->consume(_buffer_storage.size() - _boundary.size());

                    // Read the next data until either we find a boundary or read the packet of maximum size again 
                    bytes_transferred = boost::asio::read_until(_stream, *_buffer, _boundary, error_code);

                    return sync_process_file_body(
                        std::move(settings), 
                        error_code, 
                        bytes_transferred, 
                        std::forward<additional_parameters_t>(additional_parameters)...);
                }

                // Unexpected error occured so clean up everything about not uploaded file
                if (error_code)
                {
                    _file.close();

                    // Remove the file from the file system
                    try
                    {
                        std::filesystem::remove(_output_file_paths.back());
                    }
                    catch (const std::exception& ex)
                    {}

                    // Remove the file from the list of uploaded files 
                    _output_file_paths.pop_back();

                    return;
                }

                // Write obtained bytes to the file excluding CRLF after the file data and -- followed by boundary
                // -- is the part of the boundary, used only in body, so we have to consider this -- length because
                // _boudary variable doesn't contain it
                _file.write(_buffer_storage.data(), bytes_transferred - _boundary.size() - 4);

                // Close the file as its uploading is over
                _file.close();

                // Invoke handler after reading the whole file body if it is defined
                if (settings.on_read_file_body_handler)
                {
                    try
                    {
                        settings.on_read_file_body_handler(_output_file_paths.back(), additional_parameters...);
                    }
                    catch (...)
                    {
                        error_code = error::operation_aborted;
                        
                        return;
                    }
                }

                // Consume obtained bytes
                _buffer->consume(bytes_transferred); 

                // If there is "--" after the boundary then there are no more files and request body is over
                if (std::string_view{_buffer_storage.data(), _buffer_storage.size()} == "--\r\n")
                {
                    return;
                }
                
                // Read the next file header
                bytes_transferred = boost::asio::read_until(_stream, *_buffer, "\r\n\r\n", error_code);

                sync_process_file_header(
                    std::move(settings), 
                    error_code, 
                    bytes_transferred,
                    std::forward<additional_parameters_t>(additional_parameters)...);
            }

            inline bool generate_file_path(
                const std::filesystem::path& output_directory,
                std::string_view file_name, 
                std::error_code& error_code)
            {
                _file_path = output_directory / file_name;

                if (std::filesystem::exists(_file_path, error_code))
                {
                    std::string new_file_name = _file_path.stem().string() + "(1)";

                    size_t copy_number = 1, copy_number_start_position = new_file_name.size() - 2;

                    _file_path.replace_filename(new_file_name + _file_path.extension().c_str());
                    
                    while (std::filesystem::exists(_file_path, error_code))
                    {
                        new_file_name.replace(
                            copy_number_start_position, 
                            new_file_name.size() - copy_number_start_position - 1,
                            std::to_string(++copy_number));

                        _file_path.replace_filename(new_file_name + _file_path.extension().c_str());
                    }
                }

                return !error_code;
            }

            read_stream& _stream;
            // Buffer that is used to read requests outside this class
            // It is necessary because it can already store some part of the request body
            const dynamic_buffer& _input_buffer;
            // String buffer storage that actually contains read data
            std::string _buffer_storage{};
            // Main buffer that is wrapper around the string to use it in asio operations
            std::optional<boost::asio::dynamic_string_buffer<char, std::char_traits<char>, std::allocator<char>>> _buffer{};
            std::string_view _boundary{};
            std::filesystem::path _file_path{};
            std::ofstream _file{};
            std::vector<std::filesystem::path> _output_file_paths{};
    };
};

#endif