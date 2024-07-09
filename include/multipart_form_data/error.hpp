#ifndef MULTIPART_FORM_DATA_ERROR_HPP
#define MULTIPART_FORM_DATA_ERROR_HPP

#include <boost/beast/core/error.hpp>

namespace multipart_form_data
{
    using error_code = boost::beast::error_code;
    using error_category = boost::beast::error_category;
    using error_condition = boost::beast::error_condition;

    // Error codes for specific multipart/form-data errors.
    enum class error
    {
        // Request's Content-Type field doesn't contain multipart/form-data.
        not_multipart_form_data_request = 1,
        // Request has invalid structure for multipart/form-data protocol.
        invalid_structure,
        // File with path that was provided in on_read_file_header_handler can't be opened.
        invalid_file_path,
        // Either on_read_file_header_handler or on_read_file_body_handler threw an exception
        // so the whole operation is aborted.
        operation_aborted
    };

    // Error conditions corresponding to present error codes. 
    enum class condition
    {
        // Request's Content-Type field doesn't contain multipart/form-data.
        not_multipart_form_data_request = 1,
        // Request has invalid structure for multipart/form-data protocol.
        invalid_structure,
        // File with path that was provided in on_read_file_header_handler can't be opened.
        invalid_file_path,
        // Either on_read_file_header_handler or on_read_file_body_handler threw an exception
        // so the whole operation is aborted.
        operation_aborted
    };
}

// Enable present error codes and conditions to be used as boost errors.
namespace boost 
{
    namespace system 
    {
        template<>
        struct is_error_code_enum<multipart_form_data::error>
        {
            static bool const value = true;
        };
        template<>
        struct is_error_condition_enum<multipart_form_data::condition>
        {
            static bool const value = true;
        };
    }
}

namespace multipart_form_data
{
    namespace detail
    {
        class error_codes : public error_category
        {
            public:
                const char* name() const noexcept override
                {
                    return "multipart_form_data";
                }

                // Assign category id with random 64 bit value to avoid collisions and provide valid comparison. 
                error_codes() 
                : error_category(0x06abccf272b9e104u)  
                {}

                BOOST_BEAST_DECL std::string message(int ev) const override
                {
                    switch(static_cast<error>(ev))
                    {
                        case error::not_multipart_form_data_request:
                        {
                            return "Content-Type is not multipart/form-data";
                        }
                        case error::invalid_structure:
                        {
                            return "Invalid multipart/form-data structure";
                        }
                        case error::invalid_file_path:
                        {
                            return "Invalid file path was provided";
                        }
                        case error::operation_aborted:
                        {
                            return "Multipart/form-data operation is aborted due to caugth exception";
                        }
                        default:
                        {
                            return "Unknown error";
                        }
                    }
                }

                BOOST_BEAST_DECL error_condition default_error_condition(int ev) const noexcept override
                {
                    switch(static_cast<error>(ev))
                    {
                        case error::not_multipart_form_data_request:
                        {
                            return condition::not_multipart_form_data_request;
                        }
                        case error::invalid_structure:
                        {
                            return condition::invalid_structure;
                        }
                        case error::invalid_file_path:
                        {
                            return condition::invalid_file_path;
                        }
                        case error::operation_aborted:
                        {
                            return condition::operation_aborted;
                        }
                        default:
                        {
                            return {ev, *this};
                        }
                    }
                }
        };

        class error_conditions : public error_category
        {
            public:
                BOOST_BEAST_DECL const char* name() const noexcept override
                {
                    return "multipart_form_data";
                }

                // Assign condition id with random 64 bit value to avoid collisions and provide valid comparison. 
                error_conditions() 
                : error_category(0xe0f11f933d47c6d4u)  
                {}

                BOOST_BEAST_DECL std::string message(int cv) const override
                {
                    switch(static_cast<condition>(cv))
                    {
                        case condition::not_multipart_form_data_request:
                        {
                            return "Content-Type is not multipart/form-data";
                        }
                        case condition::invalid_structure:
                        {
                            return "Invalid multipart/form-data structure";
                        }
                        case condition::invalid_file_path:
                        {
                            return "Invalid file path was provided";
                        }
                        case condition::operation_aborted:
                        {
                            return "Multipart/form-data operation is aborted due to caugth exception";
                        }
                        default:
                        {
                            return "Unknown error";
                        }
                    }
                }
        };
    }

    BOOST_BEAST_DECL error_code make_error_code(error e)
    {
        static detail::error_codes const cat{};

        return error_code{static_cast<
            std::underlying_type<error>::type>(e), cat};
    }

    BOOST_BEAST_DECL error_condition make_error_condition(condition c)
    {
        static detail::error_conditions const cat{};

        return error_condition{static_cast<std::underlying_type<condition>::type>(c), cat};
    }
}

#endif