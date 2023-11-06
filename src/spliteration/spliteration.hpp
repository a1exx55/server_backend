#ifndef SPLITERATION_HPP
#define SPLITERATION_HPP

#include <iostream>
#include <vector>
#include <math.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>



class spliter
{
public:

    static int8_t tokenize(std::vector <std::string>& result, std::string_view string_to_split, std::string_view delimter);

    static int8_t tokenize(std::vector <std::string>& result, std::string_view string_to_split, char delimter);


private:

    static size_t _my_find(std::string_view string_for_find, char char_to_find, size_t start_pos, size_t end_pos);

    static size_t _my_find(std::string_view string_for_find, std::string_view string_to_find, size_t start_pos, size_t end_pos);

};

#endif
