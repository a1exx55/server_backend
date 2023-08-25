#include <validation/validation.hpp>

inline const std::map<validation, const boost::regex> validator::_validation_regexes
{
	{
		validation::EMAIL, 
		boost::regex{R"(^[A-Za-z0-9._+\-\']+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,}$)"}
	},
	{
		validation::PHONE, 
		// boost::regex{R"(^(?:\+?7|8)?(?: |-)?\(?\d{3}\)?(?: |-)?\d\)?(?: |-)?\d\)?(?: |-)?\d(?: |-)?\d(?: |-)?\d(?: |-)?\d(?: |-)?\d$)"}
		boost::regex{R"(^(?:\+?7|8)?(?:\s+|-)?\(?(?:\d{3}\)?(?:\s+|-)?\d{3}(?:\s+|-)?\d{2}(?:\s+|-)?\d{2}|\d{4}\)?(?:\s+|-)?(?:\d{2}(?:\s+|-)?\d{2}(?:\s+|-)?\d{2}|\d{3}(?:\s+|-)?\d{3})|\d{5}\)?(?:\s+|-)?(?:\d{3}(?:\s+|-)?\d{2}|\d{2}(?:\s+|-)?\d{3}))$)"}
	}
};

bool validator::has_validation(
	const std::string& string_to_validate, 
	validation validation)
{
	if (boost::regex_match(string_to_validate, _validation_regexes.at(validation)))
	{
		return true;
	}
	
	return false;
}

bool validator::has_partial_validation(
	const std::string& string_to_validate, 
	validation validation)
{
	if (boost::regex_search(string_to_validate, boost::regex{_validation_regexes.at(validation)}))
	{
		return true;
	}

	return false;
}

validation validator::determine_validation(const std::string& string_to_validate)
{
	for (uint_fast8_t i = 1; i < magic_enum::enum_count<validation>(); ++i)
	{
		if (has_validation(string_to_validate, static_cast<validation>(i)))
		{
			return static_cast<validation>(i);
		}
	}

	return validation::UNKNOWN;
}

validation validator::determine_partial_validation(const std::string& string_to_validate)
{
	for (uint_fast8_t i = 1; i < magic_enum::enum_count<validation>(); ++i)
	{
		if (has_partial_validation(string_to_validate, static_cast<validation>(i)))
		{
			return static_cast<validation>(i);
		}
	}

	return validation::UNKNOWN;
}

validation validator::determine_validation_except(
	const std::string& string_to_validate,
	validation excepted_validation)
{
	// Store the number of excepted validation to avoid extra static_cast's when comparing
	uint_fast8_t excepted_validation_number{static_cast<uint_fast8_t>(excepted_validation)};

	for (uint_fast8_t i = 1; i < magic_enum::enum_count<validation>(); ++i)
	{
		if (i != excepted_validation_number && 
			has_validation(string_to_validate, static_cast<validation>(i)))
		{
			return static_cast<validation>(i);
		}
	}

	return validation::UNKNOWN;
}