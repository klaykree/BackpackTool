#include "ConsoleIO.h"
#include <iostream>
#include "conio.h"

ConsoleIO::ConsoleIO()
{
	_ConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	
	//Bind PrintHelp to 'help' command
	RegisterCallback( { L"help", { L"Displays a list of all possible functions", std::bind(&ConsoleIO::PrintHelp, this) } } );
	
	PrintPrompt();
	
	//Begin getting input, place input on its own thread to not block current thread
	_InputThread = std::make_unique<std::thread>(&ConsoleIO::InputLoop, this);
}

ConsoleIO::~ConsoleIO() {}

void ConsoleIO::Print(std::wstring Text)
{
	CONSOLE_SCREEN_BUFFER_INFO ConBufInfo;
	GetConsoleScreenBufferInfo(_ConsoleHandle, &ConBufInfo);
	//Move cursor to start of current line
	SetConsoleCursorPosition(_ConsoleHandle, { 0, ConBufInfo.dwCursorPosition.Y });
	for(SHORT i = 0 ; i < ConBufInfo.dwCursorPosition.X ; ++i)
	{
		std::wcout << ' '; //Overwrite all current characters with 'nothing'
	}
	//Move cursor back to start of current line
	SetConsoleCursorPosition(_ConsoleHandle, { 0, ConBufInfo.dwCursorPosition.Y });
	
	std::wcout << Text << std::endl;
	
	PrintPrompt(); //Reprint prompt, it was overwritten by Text
	
	std::wcout << _Input; //Reprint already inputted text
}

void ConsoleIO::RegisterCallback(FunctionCallbackWithName FunctionCB)
{
	_Callbacks[FunctionCB.Name] = FunctionCB.FunctionCB; //Set callback function by name
}

void ConsoleIO::PrintHelp()
{
	for(const auto & i : _Callbacks) //Display all callback commands and descriptions
	{
		std::wstring Text = i.first + L": " + i.second.FunctionDescription;
		Print(Text);
	}
}

void ConsoleIO::PrintPrompt()
{
	SetConsoleTextAttribute(_ConsoleHandle, 8); //Set text color to grey
	std::cout << "BT";
	SetConsoleTextAttribute(_ConsoleHandle, 7); //Set text color to light grey
	std::cout << ">";
}

void ConsoleIO::InputLoop()
{
	//Enters input state and does not return
	while(true)
	{
		const wchar_t Inputtedkey = _getwch(); //Wait for and retrieve keypress
		
		if(Inputtedkey == '\r') //Pressed enter
		{
			std::wcout << std::endl;
			
			auto CB = _Callbacks.find(_Input); //Get callback by name
			if(CB != _Callbacks.end()) //Function found
			{
				std::wstring Input(_Input);
				_Input.clear();
				CB->second.Callback(Input); //Call function callback
			}
			else //Function not found
			{
				std::wstring InputNotFound = _Input + L" not found (use 'help' for a list of functions)";
				_Input.clear();
				Print(InputNotFound);
			}
		}
		else if(Inputtedkey == '\b') //Pressed backspace
		{
			if(!_Input.empty())
			{
				_Input.pop_back(); //Remove latest input
				
				CONSOLE_SCREEN_BUFFER_INFO ConBufInfo;
				GetConsoleScreenBufferInfo(_ConsoleHandle, &ConBufInfo);
				//Move cursor back one character
				SetConsoleCursorPosition(_ConsoleHandle, { ConBufInfo.dwCursorPosition.X - 1, ConBufInfo.dwCursorPosition.Y });
				//Overwrite character
				std::wcout << L' ';
				//Move cursor back one character due to the overwrite
				SetConsoleCursorPosition(_ConsoleHandle, { ConBufInfo.dwCursorPosition.X - 1, ConBufInfo.dwCursorPosition.Y });
			}
		}
		else
		{
			_Input.push_back(Inputtedkey);
			std::wcout << Inputtedkey; //Print the inputted character
		}
	}
}