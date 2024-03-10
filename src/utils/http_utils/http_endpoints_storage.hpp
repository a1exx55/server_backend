#ifndef HTTP_ENDPOINTS_STORAGE_HPP
#define HTTP_ENDPOINTS_STORAGE_HPP

//local
#include <utils/http_utils/uri.hpp>

//internal
#include <initializer_list>

namespace http_utils
{
    // Structure for storing http endpoint: 
    // uri template an http method for identification of endpoint and metadata for storing necessary payload
    // Uri template is an usual uri with highlighted path parameters to distinct them from the constant path segments
    // Uri template example: /api/user/{userId}/login, where {userId} is a path parameter
    template <typename T>
    struct http_endpoint
    {
        std::string uri_template{};
        boost::beast::http::verb method{};
        T metadata{};
    };

    // Storage of http endpoints, represented as sort of tree to perform fast lookup 
    // of endpoints with possible path parameters in uri
    // NOTE: endpoint's uri template with path parameters has to follow rules, described in http_endpoint
    template <typename T>
    class http_endpoints_storage
    {
        public:
            http_endpoints_storage(std::initializer_list<http_endpoint<T>> endpoints)
            {
                if (endpoints.size())
                {
                    _initial_path_segment = new http_uri_path_segment<T>{};
                }

                for (auto& endpoint : endpoints)
                {
                    http_uri_path_segment<T>* current_path_segment = _initial_path_segment;

                    for (auto path_segment : uri::get_path_segments(endpoint.uri_template))
                    {
                        // Segment is path parameter
                        if (path_segment.front() == '{' && path_segment.back() == '}')
                        {
                            if (!current_path_segment->next_path_parameter)
                            {
                                current_path_segment->next_path_parameter = new http_uri_path_segment<T>{};
                            }

                            current_path_segment = current_path_segment->next_path_parameter;
                        }
                        // Segment is constant
                        else
                        {
                            auto found_segment = std::find_if(
                                current_path_segment->next_constant_path_segments.begin(),
                                current_path_segment->next_constant_path_segments.end(),
                                [&path_segment](std::pair<std::string, http_uri_path_segment<T>*>& segment)
                                {
                                    return segment.first == path_segment;
                                });

                            // Path segment was not found
                            if (found_segment == current_path_segment->next_constant_path_segments.end())
                            {
                                current_path_segment = 
                                    current_path_segment->next_constant_path_segments.emplace_back(
                                        std::string{path_segment}, 
                                        new http_uri_path_segment<T>{})
                                    .second;
                            }
                            else
                            {
                                current_path_segment = found_segment->second;
                            }
                        }
                    }

                    // Assign endpoint data to the last segment of uri
                    current_path_segment->endpoint = endpoint;
                }
            }

            ~http_endpoints_storage()
            {
                release_path_segment(_initial_path_segment);
            }

            // Find the specific endpoint among stored endpoints by uri and http method
            // Return corresponding endpoint of finding, otherwise return empty http_endpoint
            http_endpoint<T> find_endpoint(std::string_view uri, boost::beast::http::verb method) const
            {
                if (!_initial_path_segment)
                {
                    return {};
                }

                http_uri_path_segment<T>* current_path_segment = _initial_path_segment;

                for (auto path_segment : uri::get_path_segments(uri))
                {
                    // Firstly try to find path segment among constant segments
                    auto found_segment = std::find_if(
                        current_path_segment->next_constant_path_segments.begin(),
                        current_path_segment->next_constant_path_segments.end(),
                        [&path_segment](std::pair<std::string, http_uri_path_segment<T>*>& segment)
                        {
                            return segment.first == path_segment;
                        });

                    // If it is not constant segment then it can be only path parameter
                    if (found_segment == current_path_segment->next_constant_path_segments.end())
                    {
                        if (current_path_segment->next_path_parameter)
                        {
                            current_path_segment = current_path_segment->next_path_parameter;
                        }
                        else
                        {
                            return {};
                        }
                    }
                    else
                    {
                        current_path_segment = found_segment->second;
                    }
                }

                // After getting to the last path segment compare its method with the given one
                if (current_path_segment->endpoint.method == method)
                {
                    return current_path_segment->endpoint;
                }
                else
                {
                    return {};
                }
            }

        private:
            template <typename U>
            struct http_uri_path_segment
            {
                http_endpoint<U> endpoint{};

                std::vector<std::pair<std::string, http_uri_path_segment<U>*>> next_constant_path_segments{};
                http_uri_path_segment<U>* next_path_parameter{};
            };
        
            // Release path segment by recurseively invoke this function 
            // to its children and then delete pointer to this path_segment 
            void release_path_segment(http_uri_path_segment<T>* path_segment)
            {
                if (path_segment)
                {
                    for (auto& next_constant_path_segment : path_segment->next_constant_path_segments)
                    {
                        release_path_segment(next_constant_path_segment.second);
                    }

                    release_path_segment(path_segment->next_path_parameter);

                    delete path_segment;
                }
            }

            http_uri_path_segment<T>* _initial_path_segment{};
    };
}

#endif