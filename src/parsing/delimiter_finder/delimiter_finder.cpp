#include <parsing/delimiter_finder/delimiter_finder.hpp>

std::string delimiter_finder::determine_text_file_delimiter(const std::string& path_to_the_file)
{
	std::string result = "";

	std::ifstream file_to_analyze;
	file_to_analyze.open(path_to_the_file);

	if (file_to_analyze.is_open())
	{
		//
		// single delimiter
		//

		// start finding a single delimiter
		std::array<double, _delimiters_array_size > delimiters_total_rows_avg{0};
		size_t count_of_analyzed_rows = 0;

		// 1 MB string to read file
		char* string_to_analyze_file = new char[1048576];

		// start analyzing of file by 1 MB
		file_to_analyze.read(string_to_analyze_file, 1048576);
		while (file_to_analyze.gcount() && (count_of_analyzed_rows <= 1000))
		{
			std::string_view string_to_analyze(string_to_analyze_file, file_to_analyze.gcount());
			count_of_analyzed_rows += std::count(string_to_analyze.begin(), string_to_analyze.end(), '\n');
			analyze_part_single_delimiter(delimiters_total_rows_avg, string_to_analyze);
			file_to_analyze.read(string_to_analyze_file, 1048576);
		}

		//result of finding single delimiter
		char finded_single_delimiter = get_finded_single_delimiter(delimiters_total_rows_avg);

		//
		// multiple delimiter
		//

		// returning to start position in opened file
		file_to_analyze.seekg(0, std::ios_base::beg);
		count_of_analyzed_rows = 0;

		std::map <size_t, size_t> number_of_consecutive_single_delimiters;

		file_to_analyze.read(string_to_analyze_file, 1048576);

		std::vector<double> total_avg_multiple_delimiter_per_string(_max_multiple_delimiter_length);
		std::fill(total_avg_multiple_delimiter_per_string.begin(), total_avg_multiple_delimiter_per_string.end(), 0);

		while (file_to_analyze.gcount() && (count_of_analyzed_rows <= 1000))
		{
			std::string_view string_to_analyze(string_to_analyze_file, file_to_analyze.gcount());
			count_of_analyzed_rows += std::count(string_to_analyze.begin(), string_to_analyze.end(), '\n');
			analyze_part_multiple_delimiter(total_avg_multiple_delimiter_per_string, string_to_analyze, finded_single_delimiter, _max_multiple_delimiter_length);
			file_to_analyze.read(string_to_analyze_file, 1048576);
		}

		delete[] string_to_analyze_file;
		file_to_analyze.close();

		result = get_finded_multyple_delimiter(total_avg_multiple_delimiter_per_string,finded_single_delimiter);

	}

	return result;
}


//
// single delimiter methods
//

void delimiter_finder::analyze_part_single_delimiter(std::array<double, delimiter_finder::_delimiters_array_size>& result, std::string_view analysis_part)
{
	size_t string_end_pos = 0,
		string_start_pos = 0;

	while ((string_end_pos = analysis_part.find('\n', string_end_pos)) != std::string_view::npos)
	{
		analyze_row_to_find_single_delimiter(result, analysis_part.substr(string_start_pos, string_end_pos - string_start_pos));
		string_start_pos = ++string_end_pos;
	}
	analyze_row_to_find_single_delimiter(result, analysis_part.substr(string_start_pos));
}

void delimiter_finder::analyze_row_to_find_single_delimiter(std::array<double, delimiter_finder::_delimiters_array_size>& result, std::string_view analysis_row)
{
	std::array<size_t, _delimiters_array_size> delimiters_count_per_string{0};

	for (size_t i = 0, j = 0; i < analysis_row.length(); ++i)
	{
		if ((j = _delimiters_array.find(analysis_row[i])) != std::string_view::npos)
		{
			++delimiters_count_per_string[j];
		}
	}
	get_delimiters_pre_row_avg(result ,delimiters_count_per_string, analysis_row.length());
}

void delimiter_finder::get_delimiters_pre_row_avg(std::array<double, delimiter_finder::_delimiters_array_size>& result, const std::array<size_t, delimiter_finder::_delimiters_array_size>& delimiters_count_per_row, size_t row_length)
{
	for (size_t i = 0; i < _delimiters_array_size; ++i)
	{
		result[i] += double(delimiters_count_per_row[i]) / (row_length > 0 ? row_length : 1);
	}
}

char delimiter_finder::get_finded_single_delimiter(const std::array<double, _delimiters_array_size>& delimiters_total_rows_avg)
{
	std::array<double, _delimiters_array_size> temp = delimiters_total_rows_avg;
	std::sort(temp.begin(),temp.end());
	if (temp[_delimiters_array_size - 1] > (2 * temp[_delimiters_array_size - 2]))
	{
		return _delimiters_array[
			std::distance(delimiters_total_rows_avg.begin(),
				std::max_element(delimiters_total_rows_avg.begin(), delimiters_total_rows_avg.end())
			)
		];
	}
	
	return 0;
}

//
// multiply delimiter methods
//


void delimiter_finder::analyze_part_multiple_delimiter(std::vector<double>& result, std::string_view analysis_part, const char single_delimiter, const size_t max_multiple_delimiter_length)
{
	size_t string_end_pos = 0,
		string_start_pos = 0;

	while ((string_end_pos = analysis_part.find('\n', string_end_pos)) != std::string_view::npos)
	{
		analyze_row_to_find_multiple_delimiter(result, analysis_part.substr(string_start_pos, string_end_pos - string_start_pos), single_delimiter, max_multiple_delimiter_length);
		string_start_pos = ++string_end_pos;
	}
	analyze_row_to_find_multiple_delimiter(result, analysis_part.substr(string_start_pos), single_delimiter, max_multiple_delimiter_length);
}

void delimiter_finder::analyze_row_to_find_multiple_delimiter(std::vector<double>& result, std::string_view analysis_row, const char single_delimiter, const size_t max_multiple_delimiter_length)
{
	std::map<size_t, size_t> _count_of_multi_delimiters_sets_repetition;
	for (size_t i = 0; i < analysis_row.length(); ++i)
	{
		if (analysis_row[i] == single_delimiter)
		{
			for (size_t j = 0; i < analysis_row.length(); ++j, ++i)
			{
				if (analysis_row[i] != single_delimiter)
				{
					++_count_of_multi_delimiters_sets_repetition[j];
					break;
				}
			}
		}
	}

	if (analysis_row.length())
	{
		for (auto i : _count_of_multi_delimiters_sets_repetition)
		{
			result[0] += double(i.second * i.first) / analysis_row.length();

			for (size_t j = 1; j < max_multiple_delimiter_length; ++j)
			{
				if (!(i.first % (j+1)))
				{
					result[j] += (_multiple_delimiter_main_calibration_constant + double(j+1) / _multiple_delimiter_additional_calibration_constant) * (i.second * i.first) / analysis_row.length();
				}
			}
		}
	}
}

std::string delimiter_finder::get_finded_multyple_delimiter(const std::vector<double>& total_avg_multiple_delimiter_per_string, const char single_delimiter)
{
	std::vector<double> temp = total_avg_multiple_delimiter_per_string;
	std::string result = "";
	size_t multiple_delimiter_length = std::distance(total_avg_multiple_delimiter_per_string.begin(), std::max_element(total_avg_multiple_delimiter_per_string.begin(), total_avg_multiple_delimiter_per_string.end())) + 1;
	for (size_t i = 0; i < multiple_delimiter_length; ++i)
	{
		result += single_delimiter;
	}
	return result;
}
