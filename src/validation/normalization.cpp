#include <validation/normalization.hpp>

void normalizer::normalize(std::string& string_to_normalize, validation validation)
{
    switch(validation)
    {
        case validation::UNKNOWN:
        {
            return;
        }
        case validation::PHONE:
        {
            return normalize_phone(string_to_normalize);
        }
        case validation::EMAIL:
        {
            return normalize_email(string_to_normalize);
        }
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