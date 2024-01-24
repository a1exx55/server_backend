#include <parsing/validation/validation.hpp>

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
		// Cover the range [-32767, 32767]
		validation::SMALLINT,
		boost::regex{R"(0|-?(?:[1-9]\d{0,3}|[1-3][0-2][0-7][0-6][0-7]))"}
	},
	{
		// Cover the range [-2147483647, 2147483647] 
		// But we ignore the last digit as the 10 digits' numbers cover the PHONE validation
		validation::INT,
		boost::regex{R"(0|-?[1-9]\d{0,8})"}
	},
	{
		// Cover the range [-9223372036854775807, 9223372036854775807] 
		// But we ignore the last digit to avoid long digit by digit comparison
		validation::BIGINT,
		boost::regex{R"(0|-?[1-9]\d{0,17})"}
	},
	{
		// Simplified pattern of email just to check the correct structure
		validation::EMAIL, 
		boost::regex{R"([A-Za-z0-9._+\-\']+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,})"}
	},
	{
		// Alternate phone regex by region number or operator number(first 3-5 digits) to process 
		// different dashes or spaces positions: +7(912)-345-67-89, +7(9123)-45-67-89, +7(91234)-567-89 etc.
		// This region number can be surrounded by parentheses and even (8960) or (+7960)
		// The leading +7 or 8 is optional
		validation::PHONE, 
		boost::regex{R"(\(?(?:\+?7|8)?\s*-?\s*\(?(?:\d{3}\)?\s*-?\s*(?:\d{3}\s*-?\s*\d{2}\s*-?\s*\d{2}|\d{2}\s*-?\s*\d{3}\s*-?\s*\d{2}|\d{2}\s*-?\s*\d{2}\s*-?\s*\d{3})|\d{4}\)?\s*-?\s*(?:\d{2}\s*-?\s*\d{2}\s*-?\s*\d{2}|\d{3}\s*-?\s*\d{3})|\d{5}\)?\s*-?\s*(?:\d{3}\s*-?\s*\d{2}|\d{2}\s*-?\s*\d{3})))"}
	},
	{
		// Two or three words on Russian or English with the length at least 2 letters with possible double surname(Ivanov-Belyaev), ' or ` instead of ь(Kuz'Mina), Turkic middle names like оглы
		validation::FULL_NAME,
		boost::make_u32regex(R"([а-яёa-z]{1,}['`]?[а-яёa-z]{1,}(?:-[а-яёa-z]{1,}['`]?[а-яёa-z]{1,})?\s+[а-яёa-z]{1,}['`]?[а-яёa-z]{1,}(?:-[а-яёa-z]{1,}['`]?[а-яёa-z]{1,})?(?:\s+[а-яёa-z]{1,}['`]?[а-яёa-z]{1,}(?:-[а-яёa-z]{1,}['`]?[а-яёa-z]{1,})?)?(?:(?:\s+|-)(?:[оу]гл[ыуи]|[кг]ызы|уулу|улы|o['`]?g['`]?l[iy]))?)", 
			boost::regex::perl | boost::regex::icase)
	},
	{
		// Validate three date formats: "YYYY-MM-DD[T00:00:00.000+05:13]", "MM/DD/YYYY", "DD.MM.YYYY"
		// In each format it is validated leap years(28 or 29 days in February) and 30 or 31 days depending on month
		// In the first format(ISO) it is permitted to have time part with all zeros except time zone offset
		// that can be any as some files use full iso datetime format for dates only
		validation::DATE,
		boost::regex{R"([1-2][90](?:(?:[02468][1235679]|[13579][01345789])-(?:(?:0[13578]|10|12)-(?:0[1-9]|[1-2]\d|3[0-1])|(?:0[469]|11)-(?:0[1-9]|[1-2]\d|30)|02-(?:0[1-9]|1\d|2[0-8]))|(?:[02468][048]|[13579][26])-(?:(?:0[13578]|10|12)-(?:0[1-9]|[1-2]\d|3[0-1])|(?:0[469]|11)-(?:0[1-9]|[1-2]\d|30)|02-(?:0[1-9]|1\d|2[0-9])))(?:[\sT]00:00:00(?:.0{0,6})?(?:Z|[+-](?:0\d|1[0-5])(?::[0-5]\d)?)?)?|(?:0?[1-9]|[12]\d|3[01])\.(?:0?[13578]|10|12)\.[1-2][90]\d{2}|(?:0?[1-9]|[12]\d|30)\.(?:0?[469]|11)\.[1-2][90]\d{2}|(?:0?[1-9]|1[0-9]|2[0-8])\.0?2\.[1-2][90]\d{2}|29\.0?2\.[1-2][90](?:[02468][048]|[13579][26])|(?:0?[13578]|10|12)\/(?:0?[1-9]|[12]\d|3[01])\/[1-2][90]\d{2}|(?:0?[469]|11)\/(?:0?[1-9]|[12]\d|30)\/[1-2][90]\d{2}|0?2\/(?:(?:0?[1-9]|1[0-9]|2[0-8])\/[1-2][90]\d{2}|29\/[1-2][90](?:[02468][048]|[13579][26])))"}
	},
	{
		// Validate ISO datetime format, e.g. "YYYY-MM-DD[T12:34:56.789+12:34]"
		// It is validated leap years(28 or 29 days in February) and 30 or 31 days depending on month
		// It is permitted to omit time precision(.789) and time zone offset(+12:34)
		validation::DATETIME,
		boost::regex{R"([1-2][90](?:(?:[02468][1235679]|[13579][01345789])-(?:(?:0[13578]|10|12)-(?:0[1-9]|[1-2]\d|3[0-1])|(?:0[469]|11)-(?:0[1-9]|[1-2]\d|30)|02-(?:0[1-9]|1\d|2[0-8]))|(?:[02468][048]|[13579][26])-(?:(?:0[13578]|10|12)-(?:0[1-9]|[1-2]\d|3[0-1])|(?:0[469]|11)-(?:0[1-9]|[1-2]\d|30)|02-(?:0[1-9]|1\d|2[0-9])))[\sT](?:[0-1]\d|2[0-3]):[0-5]\d:[0-5]\d(?:.\d{0,6})?(?:Z|[+-](?:0\d|1[0-5])(?::[0-5]\d)?)?)"}
	},
	{
		// Include different Russian and Soviet car numbers in the following order:
		// usual_number|taxi_number|moto_number|soviet_number(1)|soviet_number(2)|soviet_number(3)|police_number|transit_number|diplomatic_number and foreign_citizens_number(russian and soviet)
		validation::CAR_NUMBER,
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