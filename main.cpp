#include <Windows.h>
#include <iostream>
#include <string>
#include <conio.h>
#include <memory>
#include "ConsoleIO.h"
#include "HwInput.h"
#include "WindowSelect.h"
#include "ProgramInterfacer.h"

int main()
{
	//Mouse/keyboard input
	HwInput Input;
	Input.BeginInputThread();

	//Console for text input and output
	ConsoleIO Console;
	
	//Object to pause/unpause/save/load/etc a program
	ProgramInterfacer GameProgramInterfactor(Console, Input);
	
	//Handles win event proc to get an active window
	WindowSelect WindowSelector(Console, GameProgramInterfactor);
	
	//Begin win event proc and message pump
	WindowSelector.RunEventProc();
}