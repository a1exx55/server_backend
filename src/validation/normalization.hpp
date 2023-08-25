#ifndef NORMALIZATION_HPP
#define NORMALIZATION_HPP

#include <validation/validation.hpp>

class normalizer
{
	public:
		// Transform the given string to the unified form depending on the specified validation
		static void normalize(std::string& string_to_normalize, validation validation);

	private:
		// Erase the following characters "() -+" and the leading 7 or 8
		// Normalized phone is 10 digits string like "9101534786"
		static void normalize_phone(std::string& string_to_normalize);

		// Convert English letters to the lower case(don't do anything with other symbols)
		static void normalize_email(std::string& string_to_normalize);

		static void normalize_datetime(std::string& string_to_normalize);
};

#endif