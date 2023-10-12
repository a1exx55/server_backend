#include "spliteration.hpp"

// NE RABOTAET!
int8_t spliter::tokenize(std::vector <std::string>& result, std::string_view string_to_split, std::string_view delimiter)
{
	int8_t split_turn = 2;


	size_t  delimiter_position,
		delimetr_length = delimiter.length(),
		quote_start_position,
		quote_end_position,
		quote_counter;

	char quote_type;

	if (!(result.empty()))
	{
		//size_t i = (result.back()).length() - 1;

		// for(char c : result.back())
		// {
		// 	quote_start_position += atoi(&c) * pow(10, i--);
		// }

		quote_start_position = atol((result.back()).data());

		quote_end_position = quote_start_position;
		delimiter_position = quote_start_position;

	}
	else
	{
		delimiter_position = 0;
		quote_start_position = 0;
		quote_end_position = 0;
	}

	while (true)
	{
		switch (split_turn)
		{

		case 0:

			if ((quote_start_position = string_to_split.find('"', quote_start_position) != std::string_view::npos)
				|| (quote_start_position = string_to_split.find('\'', quote_start_position) != std::string_view::npos))
			{
				quote_type = string_to_split[quote_start_position];
				quote_counter = 1;
				split_turn = 1;
			}
			else
			{
				
				split_turn = 2;
			}

		case 1:

			while ((++quote_start_position < string_to_split.length())
				&& (string_to_split[quote_start_position] == quote_type))
			{
				quote_counter++;
			}

			if (quote_counter % 2)
			{
				quote_end_position = quote_start_position;
				split_turn = 2;
			}
			else if (quote_start_position == delimiter_position)
			{
				quote_end_position = quote_start_position;
				split_turn = 5;
			}
			else
			{
				return -1;
			}

		case 2:

			quote_end_position = delimiter_position;

			while (delimiter_position = _my_find(string_to_split, delimiter, delimiter_position, quote_start_position) != std::string_view::npos)
			{
				result.push_back(std::string(string_to_split.substr(quote_end_position, delimiter_position++)));
				quote_end_position = delimiter_position;
			}

			result.push_back(std::string(string_to_split.substr(quote_end_position, delimiter_position)));

			if (quote_start_position != std::string_view::npos)
			{
				delimiter_position = quote_start_position;
			}
			else
			{
				return 1;
			}
			split_turn = 3;

		case 3:

			if (quote_end_position = string_to_split.find(quote_type, quote_end_position) != std::string_view::npos)
			{
				quote_counter = 1;
			}
			else
			{
				result.push_back(std::to_string(quote_start_position));
				return 0;
			}

		case 4:

			while ((++quote_end_position < string_to_split.length())
				&& (string_to_split[quote_end_position] == quote_type))
			{
				quote_counter++;
			}

			if (quote_counter % 2)
			{
				split_turn = 5;
			}
			else
			{
				split_turn = 3;
				break;
			}

		case 5:

			result.push_back(std::string(string_to_split.substr(quote_start_position, quote_end_position - quote_counter)));
			quote_start_position = quote_end_position;
			if (quote_end_position == (string_to_split.length() - 1))
			{
				return 1;
			}
			split_turn = 0;
		}
	}
}

// RABOTAET?
int8_t spliter::tokenize(std::vector <std::string>& result, std::string_view string_to_split, char delimiter)
{
	int8_t split_turn = 0;


	size_t  delimiter_position,
		quote_start_position,
		quote_end_position,
		quote_counter;

	char quote_type;

	if (!(result.empty()))
	{
		quote_start_position = atol((result.back()).data());
		result.pop_back();
		quote_end_position = quote_start_position;
		//delimiter_position = quote_start_position;
		split_turn = 3;
		quote_type = string_to_split[quote_start_position - 1];


	}
	else
	{
		delimiter_position = 0;
		quote_start_position = 0;
		quote_end_position = 0;
	}

 	while (true)
	{
		switch (split_turn)
		{

		case 0:

			if (((quote_start_position = string_to_split.find('"', quote_start_position)) != std::string_view::npos))/*
				|| ((quote_start_position = string_to_split.find('\'', quote_end_position)) != std::string_view::npos))*/
			{
				quote_type = string_to_split[quote_start_position];
				quote_counter = 1;
			}
			else
			{
				split_turn = 2;
				break;
			}

		case 1:

			while ((++quote_start_position < string_to_split.length())
				&& (string_to_split[quote_start_position] == quote_type))
			{
				quote_counter++;
			}

			if (!(quote_counter % 2))
			{
				if ( (quote_start_position == string_to_split.length()) || (string_to_split[quote_start_position] == delimiter))
				{
					result.push_back("");
					split_turn = 0;
					break;
				}
				else
				{
					return -1;
				}
			} 
			//else
			//{
			//	quote_end_position = quote_start_position;
			//}

		case 2:

			quote_end_position = delimiter_position;
			while ((delimiter_position = _my_find(string_to_split, delimiter, delimiter_position, quote_start_position)) != std::string_view::npos)
			{
				result.push_back(std::string(string_to_split.substr(quote_end_position, delimiter_position - quote_end_position)));
				quote_end_position = ++delimiter_position;
			}

			

			if (quote_start_position != std::string_view::npos)
			{
				delimiter_position = quote_start_position;
				quote_end_position = quote_start_position;
			}
			else
			{
				result.push_back(std::string(string_to_split.substr(quote_end_position, delimiter_position - quote_end_position)));
				return 1;
			}

		case 3:

			if ((quote_end_position = string_to_split.find(quote_type, quote_end_position)) != std::string_view::npos)
			{
				quote_counter = 1;
			}
			else
			{
				result.push_back(std::to_string(quote_start_position));
				return 0;
			}

		case 4:

			while (((++quote_end_position) < string_to_split.length())
				&& (string_to_split[quote_end_position] == quote_type))
			{
				quote_counter++;
			}

			if (!(quote_counter % 2))
			{
				split_turn = 3;
				break;
			}

		case 5:

			result.push_back(std::string(string_to_split.substr(quote_start_position, quote_end_position - quote_counter - quote_start_position)));
			if (quote_end_position == (string_to_split.length() - 1))
			{
				return 1;
			}
			quote_start_position = quote_end_position;
			delimiter_position = quote_end_position + 1;
			split_turn = 0;
		}
	}
}

// RABOTAET?
size_t spliter::_my_find(std::string_view string_for_find, char char_to_find, size_t start_pos, size_t end_pos)
{
	if ((string_for_find.substr(start_pos, end_pos-start_pos)).find(char_to_find) != std::string::npos)
	{
		return start_pos += ((string_for_find.substr(start_pos, end_pos-start_pos)).find(char_to_find));
	}
	else
		return std::string::npos;
}

// NE RABOTAET!
size_t spliter::_my_find(std::string_view string_for_find, std::string_view string_to_find, size_t start_pos, size_t end_pos)
{
	return start_pos += ((string_for_find.substr(start_pos, end_pos)).find(string_to_find));
}
