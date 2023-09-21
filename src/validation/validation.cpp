#include <validation/validation.hpp>

inline const std::map<validation, std::variant<const boost::regex, const boost::u32regex>> validator::_validation_regexes
{
	{
		validation::BLANK,
		boost::regex{R"((?:\s*|null|<blank>))",
			boost::regex::perl | boost::regex::icase}
	},
	{
		validation::BOOL,
		boost::regex{R"(0|1|false|true)",
			boost::regex::perl | boost::regex::icase}
	},
	{
		validation::SMALLINT,
		// Cover the range [-32767, 32767]
		boost::regex{R"(0|-?(?:[1-9]\d{0,3}|[1-3][0-2][0-7][0-6][0-7]))"}
	},
	{
		validation::INT,
		// Cover the range [-2147483647, 2147483647] 
		// But we ignore the last digit as the 10 digits' numbers cover the PHONE validation
		boost::regex{R"(0|-?[1-9]\d{0,8})"}
	},
	{
		validation::BIGINT,
		// Cover the range [-9223372036854775807, 9223372036854775807] 
		// But we ignore the last digit to avoid long digit by digit comparison
		boost::regex{R"(0|-?[1-9]\d{0,17})"}
	},
	{
		validation::EMAIL, 
		boost::regex{R"([A-Za-z0-9._+\-\']+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,})"}
	},
	{
		validation::PHONE, 
		boost::regex{R"((?:\+?7|8)?(?:\s+|-)?\(?(?:\d{3}\)?(?:\s+|-)?(?:\d{3}(?:\s+|-)?\d{2}(?:\s+|-)?\d{2}|\d{2}(?:\s+|-)?\d{3}(?:\s+|-)?\d{2}|\d{2}(?:\s+|-)?\d{2}(?:\s+|-)?\d{3})|\d{4}\)?(?:\s+|-)?(?:\d{2}(?:\s+|-)?\d{2}(?:\s+|-)?\d{2}|\d{3}(?:\s+|-)?\d{3})|\d{5}\)?(?:\s+|-)?(?:\d{3}(?:\s+|-)?\d{2}|\d{2}(?:\s+|-)?\d{3})))"}
	},
	{
		validation::FULL_NAME,
		// Two or three words on Russian or English with the length at least 2 letters with possible double surname(Ivanov-Belyaev), ' or ` instead of ь(Kuz'Mina), Turkic middle names like оглы
		boost::make_u32regex(R"([а-яёa-z]{1,}['`]?[а-яёa-z]{1,}(?:-[а-яёa-z]{1,}['`]?[а-яёa-z]{1,})?\s+[а-яёa-z]{1,}['`]?[а-яёa-z]{1,}(?:-[а-яёa-z]{1,}['`]?[а-яёa-z]{1,})?(?:\s+[а-яёa-z]{1,}['`]?[а-яёa-z]{1,}(?:-[а-яёa-z]{1,}['`]?[а-яёa-z]{1,})?)?(?:(?:\s+|-)(?:[оу]гл[ыуи]|[кг]ызы|уулу|улы|o['`]?g['`]?l[iy]))?)", 
			boost::regex::perl | boost::regex::icase)
	},
	{
		validation::CAR_NUMBER,
		// usual_number|taxi_number|moto_number|soviet_number(1)|soviet_number(2)|soviet_number(3)|police_number|transit_number|diplomatic_number and foreign_citizens_number(russian and soviet)
		boost::make_u32regex(R"([а-я]\d{3}[а-я]{2}\d{2,3}|[а-я]{2}\d{5,6}|\d{4}[а-я]{2}\d{2,3}|[а-я]\d{4}[а-я]{2}|\d{4}[а-я]{3}|[а-я]{3}\d{4}|[а-я]\d{6,7}|[а-я]{2}\d{3}[а-я]\d{2,3}|\d{3}[dтдвекмн]\d{3}(?:\d{2,3})?)", 
			boost::regex::perl | boost::regex::icase)
	}
};

bool validator::has_validation(
	const std::string& string_to_validate, 
	validation validation)
{
	if (std::holds_alternative<const boost::regex>(_validation_regexes.at(validation)))
	{
		if (boost::regex_match(
				string_to_validate, 
				std::get<const boost::regex>(_validation_regexes.at(validation))))
		{
			return true;
		}
	}
	else
	{
		if (boost::u32regex_match(
				string_to_validate, 
				std::get<const boost::u32regex>(_validation_regexes.at(validation))))
		{
			return true;
		}
	}
	
	return false;
}

bool validator::has_partial_validation(
	const std::string& string_to_validate, 
	validation validation)
{
	// if (boost::regex_search(string_to_validate, boost::regex{_validation_regexes.at(validation)}))
	// {
	// 	return true;
	// }

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