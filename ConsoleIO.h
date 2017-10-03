#pragma once
#include <Windows.h>
#include <string>
#include <thread>
#include <map>
#include "FunctionCallback.h"

class ConsoleIO
{
public:
	ConsoleIO();
	~ConsoleIO();
	
	void Print(std::wstring Text);
	
	void RegisterCallback(FunctionCallbackWithName FunctionCB);

private:
	//Displays a list of available commands and their descriptions
	void PrintHelp();
	//Prints the input prompt
	void PrintPrompt();
	//Infinite loop for reading input
	void InputLoop();
	
	//Current inputted text
	std::wstring _Input;
	//Thread for InputLoop, to avoid blocking other threads
	std::unique_ptr<std::thread> _InputThread;
	
	//Function callbacks for available commands
	std::map<std::wstring, FunctionCallback> _Callbacks;

	//Handle to the console for Windows specific console functions
	HANDLE _ConsoleHandle;
};