#ifndef NORMALIZATION_HPP
#define NORMALIZATION_HPP

//local
#include <validation/validation.hpp>

//internal
#include <unordered_map>

//external
#include <unicode/unistr.h>

class normalizer
{
	public:
		// Transform the given string to the unified form depending on the specified validation
		static void normalize(std::string& string_to_normalize, validation validation);

	private:
		// Assign the given string with 0 on the false value and 1 on the true value
		static void normalize_bool(std::string& string_to_normalize);

		// Erase the following characters "() -+" and the leading 7 or 8
		// Normalized phone is 10 digits string like "9101534786"
		static void normalize_phone(std::string& string_to_normalize);

		// Transform the given string to the lower case
		static void normalize_email(std::string& string_to_normalize);

		// Transform the given string to the upper case
		static void normalize_full_name(std::string &string_to_normalize);

		static void normalize_date(std::string& string_to_normalize);

		static void normalize_datetime(std::string& string_to_normalize);
		
		// Transform the given string to the upper case
		static void normalize_car_number(std::string& string_to_normalize);

		static const std::unordered_map<validation, std::function<void(std::string&)>> _validation_to_normalizer;
};

#endif