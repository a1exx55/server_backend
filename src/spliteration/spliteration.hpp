#ifndef SPLITERATION_HPP
#define SPLITERATION_HPP

#include <iostream>
#include <vector>
#include <math.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>


//#include "delimiter_identification.h"



class spliter
{
public:

    //static void spliteration(std::vector<std::string>& result, std::string& string_to_split, std::string& delimiter);

    //static void split(std::vector <std::string>& result, std::string_view string_to_split, char delimiter);

    //static void split(std::vector <std::string>& result, std::string_view string_to_split, std::string_view delimiter);

    //static int8_t split(std::vector <std::string>& result, std::string_view string_to_split, char delimiter);

    //static int8_t split(std::vector <std::string>& result, std::string_view string_to_split, std::string_view delimiter);

    static int8_t tokenize(std::vector <std::string>& result, std::string_view string_to_split, std::string_view delimter);

    static int8_t tokenize(std::vector <std::string>& result, std::string_view string_to_split, char delimter);

    //static bool is_string_full(std::string& string_to_check,std::string& delimiter);
    /*
        //static std::vector <std::string>& splite(std::string& string_to_split, std::string delimiter);

        //static std::vector <std::string>& splite(std::string& string_to_split, char delimiter);

        static std::string* spliting(std::string string_to_split, char delimiter);

        static std::string* spliting(std::string string_to_split, std::string delimiter, size_t number_of_fields);

        static std::string* spliting(std::string string_to_split, char delimiter, size_t number_of_fields);

    */
private:

    //static void _formate(std::string &field_to_formate);

    //static void _vector_moving(std::vector <std::string>& result,size_t start_pos, size_t count);

    static size_t _my_find(std::string_view string_for_find, char char_to_find, size_t start_pos, size_t end_pos);

    static size_t _my_find(std::string_view string_for_find, std::string_view string_to_find, size_t start_pos, size_t end_pos);

   



    //static std::vector <std::string> _result;

};

#endif
