#include <validation/normalization.hpp>

std::string& normalize(const std::string& string_to_normalize, validation validation)
{
    switch(validation)
    {
        case validation::UNKNOWN:
        {
            return string_to_normalize;
        }
        case validation::PHONE:
        {
            return normalize_phone(string_to_normalize);
        }
    }
}