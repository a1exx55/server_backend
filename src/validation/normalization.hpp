#ifndef NORMALIZATION_HPP
#define NORMALIZATION_HPP

#include <validation/validation.hpp>

class normalizer
{
	public:
		static std::string& normalize(const std::string& string_to_normalize, validation validation);

	private:
		static std::string& _normalize_phone(const std::string& string_to_normalize);

		static std::string& normalize_datetime(const std::string& string_to_normalize);
};

#endif