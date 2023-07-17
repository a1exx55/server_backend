#include <validation/validation.hpp>

boost::regex validator::_regex{};

const std::map<validation, std::string> validator::_REGEX_STRINGS_BY_VALIDATION
{
	{validation::EMAIL, ""},
	{validation::PHONE, R"(^(?:7|\+7|8)?(?:\s*|-)\(?\d{3}\)?(?:\s*|-)\d{3}(?:\s*|-)\d{2}(?:\s*|-)\d{2}$)"}
};

bool validator::has_validation(const std::string& string_to_validate, validation validation, bool is_partial_match)
{
	_regex = _REGEX_STRINGS_BY_VALIDATION.at(validation);
	if (is_partial_match)
	{
		if (boost::regex_search(string_to_validate, _regex))
		{
			return true;
		}
		return false;
	}
	else
	{ 
		if (boost::regex_match(string_to_validate, _regex))
		{
			return true;
		}
		return false;
	}
}

validation validator::determine_validation(const std::string& string_to_validate, bool is_partial_match)
{
	for (uint_fast8_t i = 1; i < magic_enum::enum_count<validation>(); ++i)
	{
		if (has_validation(string_to_validate, static_cast<validation>(i), is_partial_match))
		{
			return static_cast<validation>(i);
		}
	}
	return validation::UNKNOWN;
}

// std::string validator::normalize_by_validation(const std::string &string_to_normalize, validation validation)
// {
//     //std::string as = normalizer::normalize();
// }
