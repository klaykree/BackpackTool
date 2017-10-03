#pragma once
#include <Windows.h>
#include <string>
#include "FunctionCallback.h"

class ProgramInterfacer;
class ConsoleIO;

class WindowSelect
{
public:
	WindowSelect(ConsoleIO& Console, ProgramInterfacer& Interfacer);
	~WindowSelect();
	
	void BeginSelecting(std::wstring Params);
	void StopSelecting();
	
	//Enters windows message pump; does not return
	void RunEventProc();
	
	HWND GetSelectedHandle();
	void SetSelectedHandle(HWND NewHandle);
	
	ProgramInterfacer& GetInterfacer() const;
	ConsoleIO& GetConsole() const;
	
	bool SelectingWindow();
	
private:
	HWND _ThisProcessHandle;
	HWND _SelectedProcessHandle = NULL;
	
	bool _SelectingWindow = false;

	ProgramInterfacer& _Interfacer;
	ConsoleIO& _Console;
};