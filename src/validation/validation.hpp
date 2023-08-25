#ifndef VALIDATION_HPP
#define VALIDATION_HPP

//internal
#include <stdint.h>
#include <iostream>

//external
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
        // Determine if the given string has the specified validation
        static bool has_validation(
            const std::string& string_to_validate, 
            validation validation);

        // Determine if the given STRING OR ITS PART has the specified validation
        static bool has_partial_validation(
            const std::string& string_to_validate, 
            validation validation);

        // Determine the validation of the given string
        static validation determine_validation(const std::string& string_to_validate);

        // Determine the validation of the given STRING OR ITS PART
        static validation determine_partial_validation(const std::string& string_to_validate);

        // Determine the validation of the given string excepting the specified validation 
        static validation determine_validation_except(
            const std::string& string_to_validate,
            validation excepted_validation);

    private:
        static const std::map<validation, const boost::regex> _validation_regexes;
};

#endif