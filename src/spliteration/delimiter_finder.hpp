#pragma once

#include <iostream>
#include <array>
#include <map>
#include <fstream>
#include <filesystem>

/// <summary>
/// /// </summary>  
	/**
	* @brief Класс для определения одиночного или множественного разделителя
	* 
	*	Для поиска одиночного разделителя начните добавлять строки для анализа через метод:  analysis_row_single_delimiter()
	* На любом этапе можно получить результат поиска на основе добавленных строк методом: get_finded_single_delimiter()
	* Для обнуления работы необходимо вызвать метод: reset_finding_single_delimiter()
	*
	*	Для поиска множественного разделителя  
	*/

class delimiter_finder
{
public:

	static std::string get_finded_delimiter_from_current_file(const std::string& path_to_the_file);



	 
private:
	// String of all possible single delimiters
	static constexpr std::string_view _delimiters_array = ",;:/|\\=\t~";
	static constexpr size_t _delimiters_array_size = _delimiters_array.length();

	// The main constant which help align values of single and multiple delimiters
	static constexpr double _multiple_delimiter_main_calibration_constant = 1.5;
	// The additional constant which help align the values of even multiple delimiters 
	static constexpr int _multiple_delimiter_additional_calibration_constant = 1000;
	// Max length of multiple delimiter
	static constexpr size_t _max_multiple_delimiter_length = 5;

	//
	// single delimiter
	// 

	/**
	* @brief analyzing a part on a single delimiter on the transferred analysis_part
	*/
	static void analyze_part_single_delimiter(std::array<double, delimiter_finder::_delimiters_array_size>& result, std::string_view analysis_part);

	/** 
	* @brief analyzing a string on a single delimiter on the transferred analysis_row
	*/
	static void analyze_row_to_find_single_delimiter(std::array<double, delimiter_finder::_delimiters_array_size>& result, std::string_view analysis_row);

	/**
	* @brief counting pre std::array of delimiters avg per string on the transferred delimiters_count_per_row and row_length
	*/
	static void get_delimiters_pre_row_avg(std::array<double, delimiter_finder::_delimiters_array_size>& result, const std::array<size_t, delimiter_finder::_delimiters_array_size>& delimiters_count_per_row, size_t row_length);

	/**
	* @brief get max of delimiters_total_rows_avg
	* @return char from _delimiters_array by max of total std::array<> of delimiters avg per string
	*/
	static char get_finded_single_delimiter(const std::array<double, _delimiters_array_size>& delimiters_total_rows_avg);

	//
	// multiple delimiter
	//  

	/**
	* @brief analyzing a part on a multiple delimiter on the transferred analysis_part
	*/
	static void analyze_part_multiple_delimiter(std::vector<double>& result, std::string_view analysis_part, const char single_delimiter, const size_t max_multiple_delimiter_length);

	/**
	* @brief analyzing a string on a multiple delimiter on the transferred analysis_row
	*/
	static void analyze_row_to_find_multiple_delimiter(std::vector<double>& result, std::string_view analysis_row, const char single_delimiter, const size_t max_multiple_delimiter_length);

	/**
	* @brief get max of delimiters_total_rows_avg
	* @return std::string of multiple delimiter
	*/
	static std::string get_finded_multyple_delimiter(const std::vector<double>& total_avg_multiple_delimiter_per_string, const char single_delimiter);

};

