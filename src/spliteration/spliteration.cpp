#include "spliteration/spliteration.hpp"


int8_t spliter::tokenize(std::vector <std::string>& result, std::string_view string_to_split, std::string_view delimiter)
{
	int8_t split_turn = 2;


	size_t  delimiter_position, 
			delimetr_length = delimiter.length(),
			quote_start_position,
			quote_end_position,
			quote_counter;

	char quote_type;

	if(!(result.empty()))
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
	
	while(true)
	{
		switch(split_turn)
		{

		case 0:
		
			if((quote_start_position = string_to_split.find('"',quote_start_position) != std::string_view::npos)
			|| (quote_start_position = string_to_split.find('\'',quote_start_position) != std::string_view::npos))
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

			while((++quote_start_position < string_to_split.length()) 
			   && (string_to_split[quote_start_position] == quote_type))
			   {
				quote_counter++;
			   }

			if(quote_counter % 2)
			{
				quote_end_position = quote_start_position;
				split_turn = 2;
			}
			else if(quote_start_position == delimiter_position)
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

			while(delimiter_position = _my_find(string_to_split, delimiter, delimiter_position, quote_start_position) != std::string_view::npos)
			{
				result.push_back(std::string(string_to_split.substr(quote_end_position, delimiter_position++)));
				quote_end_position = delimiter_position;
			}

			result.push_back(std::string(string_to_split.substr(quote_end_position, delimiter_position)));

			if(quote_start_position != std::string_view::npos)
			{
				delimiter_position = quote_start_position;
			}
			else
			{
				return 1;
			}
			split_turn = 3;
		
		case 3:

			if(quote_end_position = string_to_split.find(quote_type, quote_end_position) != std::string_view::npos)
			{
				quote_counter = 1;
			}
			else
			{
				result.push_back(std::to_string(quote_start_position));
				return 0;
			}
		
		case 4:

			while((++quote_end_position < string_to_split.length()) 
			   && (string_to_split[quote_end_position] == quote_type))
			   {
				quote_counter++;
			   }
			
			if(quote_counter % 2)
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
			if(quote_end_position == (string_to_split.length() - 1))
			{
				return 1;
			}
			split_turn = 0;
		}
	}
}

int8_t spliter::tokenize(std::vector <std::string>& result, std::string_view string_to_split, char delimiter)
{
	//zapicivaem v vector last quote_start_pos

	int8_t split_turn = 2;


	size_t  delimiter_position, 
			quote_start_position,
			quote_end_position,
			quote_counter;

	char quote_type;

	if(!(result.empty()))
	{
		//size_t i = (result.back()).length() - 1;

		
		// for(char c : result.back())
		// {
		// 	quote_start_position += atoi(&c) * pow(10, i--);
		// }
		std::cout<< "vse huevo\n";
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

	while(true)
	{
		switch(split_turn)
		{

		case 0:
		
			if((quote_start_position = string_to_split.find('"',quote_start_position) != std::string_view::npos)
			|| (quote_start_position = string_to_split.find('\'',quote_start_position) != std::string_view::npos))
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

			while((++quote_start_position < string_to_split.length()) 
			   && (string_to_split[quote_start_position] == quote_type))
			   {
				quote_counter++;
			   }
			
			if(quote_counter % 2)
			{
				quote_end_position = quote_start_position;
				split_turn = 2;
			}
			else if(quote_start_position == delimiter_position)
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
			std::cout<< "vse gud\n";
			std::cout<< delimiter_position <<"\n";
			while(delimiter_position = _my_find(string_to_split, delimiter, delimiter_position, quote_start_position) != std::string_view::npos)
			{
				std::cout<< delimiter_position <<"\n";
				result.push_back(std::string(string_to_split.substr(quote_end_position, delimiter_position++)));
				quote_end_position = delimiter_position;
			}

			result.push_back(std::string(string_to_split.substr(quote_end_position, delimiter_position)));

			if(quote_start_position != std::string_view::npos)
			{
				delimiter_position = quote_start_position;
			}
			else
			{
				return 1;
			}
			split_turn = 3;
		
		case 3:

			if(quote_end_position = string_to_split.find(quote_type, quote_end_position) != std::string_view::npos)
			{
				quote_counter = 1;
			}
			else
			{
				result.push_back(std::to_string(quote_start_position));
				return 0;
			}
		
		case 4:

			while((++quote_end_position < string_to_split.length()) 
			   && (string_to_split[quote_end_position] == quote_type))
			   {
				quote_counter++;
			   }
			
			if(quote_counter % 2)
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
			if(quote_end_position == (string_to_split.length() - 1))
			{
				return 1;
			}
			split_turn = 0;
		}
	}
}

size_t spliter::_my_find(std::string_view string_for_find, char char_to_find, size_t start_pos, size_t end_pos)
{
	return start_pos += ((string_for_find.substr(start_pos,end_pos)).find(char_to_find));
}

size_t spliter::_my_find(std::string_view string_for_find, std::string_view string_to_find, size_t start_pos, size_t end_pos)
{
	return start_pos += ((string_for_find.substr(start_pos,end_pos)).find(string_to_find) + 1);
}
/*
void spliter::split(std::vector <std::string>& result, std::string_view string_to_split, char delimiter)
 {
	int split_turn = 0;


	size_t delimiter_position = string_to_split.length() - 1, 
		   quote_start_position = std::string_view::npos,
		   quote_end_position = string_to_split.length() -1,
		   quote_counter = 0;

	while (split_turn != -1)
	{
		switch (split_turn)
		{
		case 0:

		//delim i vse bez viebonov

		break;		

		case 1:
			
			if((quote_end_position < string_to_split.length()) 
			&& (quote_end_position = string_to_split.rfind('"',quote_end_position) != std::string_view::npos))
			{
				if(quote_start_position < quote_end_position)
				delimiter_position = quote_start_position - 1;
				quote_start_position = quote_end_position;

				split_turn =2;
			}
			else
				split_turn = 0;

			break;

		case 2:

			while((quote_end_position < string_to_split.length())
			&& (string_to_split[--quote_end_position] == '"'))
				quote_counter++;
			
			if(quote_counter % 2)
				{
					if(delimiter_position == string_to_split.length() - 1 || string_to_split[delimiter_position-1] == delimiter)
					{
						split_turn = 1;
					}
					else
					{
						if((quote_end_position < string_to_split.length())  
						&& (string_to_split[quote_end_position] == delimiter))
						{
							split_turn  = -1;
						}
						
					}
				}
			else




			break;

		case 3:

			

			break;

		case 4:

			while(string_to_split[--quote_end_position] == '\'')
				quote_counter++;
			
			if(quote_counter % 2)
				{
					if(delimiter_position == string_to_split.length() - 1)
					{

					}
					else
					{

					}
				}
			else


			break;

		case 5:

			break;

		default:
			break;
		}
	}
	

 }

void spliter::split(std::vector <std::string>& result, std::string_view string_to_split, std::string_view delimiter)
{

}
*/

// int8_t spliter::split(std::vector<std::string>& result, std::string_view string_to_split, char delimiter)
// {
// 	int split_turn = 0;


// 	size_t 
// 			delimiter_position = 0, 
// 			quote_start_position = 0,
// 			quote_end_position = 0,
// 			quote_counter = 0;

	
// 	while(split_turn != -1)
// 	{
// 		switch(split_turn)
// 		{
// 		case 0:
// 		//quote_start eto nachalo polya to est delim v nachale 

// 			while((delimiter_position += (string_to_split.substr(delimiter_position,quote_start_position)).find(delimiter) + 1) != std::string_view::npos)
// 			{
// 				result.push_back(std::string(string_to_split.substr(quote_end_position, delimiter_position)));
// 				delimiter_position++;
// 			}
// 			split_turn = 2;

// 			break;

// 		case 1:

// 			result.push_back(std::string(string_to_split.substr(quote_start_position, quote_end_position - quote_counter)));
// 			split_turn = 2;

			
// 			break;

// 		case 2:
			

// 			if((quote_start_position = string_to_split.find('"', quote_start_position) != std::string_view::npos)
// 			|| (quote_start_position = string_to_split.find('\'', quote_start_position) != std::string_view::npos))
// 			{
// 				if((delimiter_position = string_to_split.find(delimiter, delimiter_position) != std::string_view::npos)
// 				&& (delimiter_position  < quote_start_position))
// 				{

// 					split_turn = 0;
// 				}

// 				//if(!quote_counter)
				
// 				// if(quote_start_position >= quote_end_position && quote_counter > 1)
// 				// {
// 				// 	quote_counter = 1;
// 				// 	//delimiter_position = quote
// 				// 	split_turn = 3;
// 				// }
// 				// else
// 				// {
// 				// 	quote_counter = 1;
// 				// 	split_turn = 4;
// 				// }
// 			}
// 			else
// 			{
// 				quote_start_position = string_to_split.length();
// 				split_turn = 0;
// 				break;
// 			}


// 		case 3:

// 			//while()

// 			break;

// 		case 4:

// 			if((quote_start_position == string_to_split.find('"', quote_start_position) != std::string_view::npos)
// 			|| (quote_start_position == string_to_split.find('\'', quote_start_position) != std::string_view::npos))
// 			{
// 				if(!quote_counter)
				
// 				if(quote_start_position >= quote_end_position && quote_counter)
// 				{
// 					quote_counter = 0;
// 					//delimiter_position = quote
// 					split_turn = 3;
// 				}
// 				else
// 				{
// 					quote_counter = 0;
// 					split_turn = 2;
// 				}
// 			}
// 			else
// 				split_turn = 0;


// 			break;


// 		case 5:

// 			break;

// 		case 6:

// 			break;




// 		}
// 	}



// }

// int8_t spliter::split(std::vector<std::string>& result, std::string_view string_to_split, std::string_view delimiter)
// {

// }

// void spliter::spliteration(std::vector<std::string>& result, std::string& string_to_split, std::string& delimiter)
// {
// 	int turn = 0;

// 	size_t field_number = 0, gluing_field_counter = 0;
// 	std::string field_string;

// 	boost::split(result, string_to_split, boost::is_any_of(delimiter), boost::token_compress_on);
	
// 	while (turn != -1)
// 	{
// 		switch (turn)
// 		{
// 		case 0:

// 			if (field_number < result.size() - 1)
// 			{
// 				field_string = result[field_number];

// 				if (field_string[0] != '"')
// 					field_number++;
// 				else
// 					turn = 1;
// 			}
// 			else
// 				turn = -1;

// 			break;

// 		case 1:

// 			if (field_string[field_string.length() - 1] == '"')
// 			{
// 				_formate(field_string);
// 				result[field_number++] = field_string;

// 				turn = 0;
// 			}
// 			else
// 				turn = 2;

// 			break;

// 		case 2:

// 			if (gluing_field_counter + field_number < result.size() - 1)
// 			{
// 				field_string += delimiter + result[++gluing_field_counter + field_number];

// 				if (field_string[field_string.length() - 1] == '"')
// 				{
// 					_formate(field_string);
// 					result[field_number++] = field_string;
// 					_vector_moving(result, field_number, gluing_field_counter);

// 					turn = 0;
// 				}
// 			}
// 			else
// 				turn = 0;

// 			break;
// 		}

// 	}
// }

// bool spliter::is_string_full(std::string& string_to_check, std::string& delimiter)
// {
// 	int turn = 0;

// 	size_t quote_counter, quote_position = string_to_check.length();
 
// 	bool result = true;

// 	while(1)
// 	{
// 		switch(turn)
// 		{
// 			case 0:

// 				if(quote_position = string_to_check.rfind('"'), quote_position - 1)
// 				{
// 					quote_counter = 1;

// 					turn = 1;
// 				}
// 				else
// 					return true;

// 				break;

// 			case 1:

// 				while(string_to_check[quote_position--] == '"')
// 					quote_counter++;
// 				if(!(quote_counter % 2))
// 					turn = 0;
// 				else
// 					if(delimiter == string_to_check.substr(quote_position - delimiter.length(), delimiter.length()))
// 						turn = 2;

// 				break;

// 			case 2:

// 				if(quote_position = string_to_check.find('"',(quote_position + quote_counter) + 1))
// 				{
// 					quote_counter = 1;

// 					turn = 3;
// 				}
// 				else
// 					return false;


// 				break;

// 			case 3:

// 				while(string_to_check[quote_position++] == '"')
// 					quote_counter++;
// 				if(!(quote_counter % 2))
// 					return false;
// 				else
// 				 	return true;
				
// 				break;

// 		}
// 	} 
// }



// void spliter::_formate(std::string& field_to_formate)
// {
// 	field_to_formate = field_to_formate.substr(1, field_to_formate.length() - 1);
// }

// void spliter::_vector_moving(std::vector <std::string>& result, size_t start_pos, size_t count)
// {
// 	size_t i;

// 	for (i = start_pos; i + count < result.size() - 1; i++)
// 	{
// 		result[i] = result[i + count];
// 	}
// 	for (i = 0; i < count; i++)
// 		result.pop_back();
// }
