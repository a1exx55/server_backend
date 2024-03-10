#include <utils/http_utils/uri.hpp>

std::vector<std::string_view> http_utils::uri::get_path_segments(std::string_view uri)
{
    // Get the "path" part of the uri without query parameters it there are any
    std::string_view path_segments_uri = uri.substr(0, uri.find('?'));

    std::vector<std::string_view> path_segments;
    size_t path_segment_start_position = 0, slash_position;

    do
    {
        slash_position = path_segments_uri.find('/', path_segment_start_position);

        // Add only not empty path segments
        if (slash_position != path_segment_start_position)
        {
            path_segments.emplace_back(
            path_segments_uri.substr(
                path_segment_start_position, 
                slash_position - path_segment_start_position));        
        }

        path_segment_start_position = slash_position + 1;
    } 
    while (slash_position != std::string::npos);
    
    return path_segments;
}