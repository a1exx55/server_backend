#include <validation/normalization.hpp>

inline const std::unordered_map<validation, std::function<void(std::string&)>> normalizer::_validation_to_normalizer
{
    {validation::UNKNOWN, [](std::string&){return;}},
    {validation::NO_VALIDATION, [](std::string&){return;}},
    {validation::BLANK, [](std::string&){return;}},
    {validation::SMALLINT, [](std::string&){return;}},
    {validation::INT, [](std::string&){return;}},
    {validation::BIGINT, [](std::string&){return;}},
    {validation::BOOL, normalize_bool},
    {validation::PHONE, normalize_phone},
    {validation::EMAIL, normalize_email},
    {validation::FULL_NAME, normalize_full_name},
    {validation::DATE, normalize_date},
    {validation::DATETIME, [](std::string&){return;}},
    {validation::CAR_NUMBER, normalize_car_number}
};

void normalizer::normalize(std::string& string_to_normalize, validation validation)
{
    // Invoke the corresponding function by validation to normalize the given string
    _validation_to_normalizer.at(validation)(string_to_normalize);
}

void normalizer::normalize_bool(std::string& string_to_normalize)
{
    if (string_to_normalize[0] == 'F' || string_to_normalize[0] == 'f')
    {
        string_to_normalize.assign("0");
        return;
    }

    if (string_to_normalize[0] == 'T' || string_to_normalize[0] == 't')
    {
        string_to_normalize.assign("1");
    }
}

void normalizer::normalize_phone(std::string& string_to_normalize)
{
    // Remove all spaces, parantheses and dashes
    std::erase_if(string_to_normalize, [](unsigned char symbol)
    {
        return std::isspace(symbol) || symbol == '(' || symbol == ')' || symbol == '-';
    });

    // If the first symbol is + then the phone starts with +7... so we remove the first two symbols
    if (string_to_normalize[0] == '+')
    {
        string_to_normalize.erase(0, 2);
        return;
    }

    // If the length of the string is 11 then the phone starts with 8... so we remove the first symbol
    if (string_to_normalize.size() == 11)
    {
        string_to_normalize.erase(0, 1);
    }
}

void normalizer::normalize_email(std::string& string_to_normalize)
{
    for (char& symbol : string_to_normalize)
        symbol = tolower(symbol);
}

void normalizer::normalize_full_name(std::string &string_to_normalize)
{
    icu::UnicodeString string_to_format{string_to_normalize.c_str()};

    // Transform the string to the upper case
    string_to_format.toUpper();

    string_to_normalize.clear();

    // Move the transformed content to the original string
    string_to_format.toUTF8String(string_to_normalize);
}

void normalizer::normalize_date(std::string& string_to_normalize)
{
    // Swap day and month if the date format is "DD.MM.YYYY" 
    // Consider whether the day contains one digit or two
    if (string_to_normalize[2] == '.')
    {
        std::string month{string_to_normalize.substr(3, string_to_normalize.find('.', 3) - 3)};
        string_to_normalize.replace(3, month.size(), string_to_normalize.substr(0, 2));
        string_to_normalize.replace(0, 2, month);
        return;
    }

    if (string_to_normalize[1] == '.')
    {
        std::string month{string_to_normalize.substr(2, string_to_normalize.find('.', 2) - 2)};
        string_to_normalize.replace(2, month.size(), string_to_normalize.substr(0, 1));
        string_to_normalize.replace(0, 1, month);
        return;
    }
}

void normalizer::normalize_car_number(std::string& string_to_normalize)
{
    icu::UnicodeString string_to_format{string_to_normalize.c_str()};

    // Transform the string to the upper case
    string_to_format.toUpper();

    string_to_normalize.clear();

    // Move the transformed content to the original string
    string_to_format.toUTF8String(string_to_normalize);
}