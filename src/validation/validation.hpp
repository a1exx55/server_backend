#ifndef VALIDATION_HPP
#define VALIDATION_HPP

#include <stdint.h>
#include <iostream>

#include <boost/regex.hpp>
#include <magic_enum.hpp>

enum validation : uint_fast8_t
{
    UNKNOWN = 0,
    EMAIL,
    PHONE, 
    FULL_NAME,
    FULL_NAME_NICK,
    PASSPORT,
    ADDRESS,
    VK,
    INN,
    SNILS,
    CAR_NUMBER,
    DATETIME,
    IP,
    CARD_NUMBER
};

class validator
{
    public:
        static bool has_validation(const std::string& string_to_validate, validation validation, bool is_partial_match = false);

        static validation determine_validation(const std::string& string_to_validate, bool is_partial_match = false);

        //static std::string normalize_by_validation(const std::string& string_to_normalize, validation validation);

    private:
        static boost::regex _regex;
        
        static const std::map<validation, std::string> _REGEX_STRINGS_BY_VALIDATION;
};


// std::map<validation, std::string> validator::_regex_strings_by_validation =
// {
//             {validation::EMAIL, ""},
//             {validation::PHONE, ""}
// };

#endif