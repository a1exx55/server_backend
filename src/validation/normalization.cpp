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
    {validation::DATETIME, normalize_datetime},
    {validation::CAR_NUMBER, normalize_car_number}
};

void normalizer::normalize(std::string& string_to_normalize, validation validation)
{
    // Invoke the corresponding function by valation to normalize the given string
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
    std::erase_if(string_to_normalize, [](unsigned char symbol)
    {
        return std::isspace(symbol) || symbol == '(' || symbol == ')' || symbol == '-';
    });

    if (string_to_normalize[0] == '+')
    {
        string_to_normalize.erase(0, 2);
        return;
    }

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

}

void normalizer::normalize_datetime(std::string& string_to_normalize)
{

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