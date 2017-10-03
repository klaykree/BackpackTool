#include "WindowSelect.h"
#include "ProgramInterfacer.h"
#include "ConsoleIO.h"

WindowSelect* Self; //Pointer to self for WinEventProc to access

WindowSelect::WindowSelect(ConsoleIO& Console, ProgramInterfacer& Interfacer)
	:_Interfacer(Interfacer), _Console(Console)
{
	Self = this;
	
	//Get the HWND for this program
	_ThisProcessHandle = GetForegroundWindow();

	FunctionCallback SelectCB;
	SelectCB.FunctionDescription = L"Begins selecting game window";
	SelectCB.Callback = std::bind(&WindowSelect::BeginSelecting, this, std::placeholders::_1);
	Console.RegisterCallback({ L"s", SelectCB });
}

WindowSelect::~WindowSelect()
{}

VOID CALLBACK WinEventProcCallback(HWINEVENTHOOK hWinEventHook, DWORD dwEvent, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime)
{
	if(dwEvent == EVENT_SYSTEM_FOREGROUND) //Window focus changed
	{
		if(Self->SelectingWindow())
		{
			//Keep handle and set processID for the interfacer
			Self->SetSelectedHandle(hwnd);
			DWORD ProcessID;
			DWORD ThreadID = GetWindowThreadProcessId(hwnd, &ProcessID);
			//new std::thread(&ProgramInterfacer::AttachToProcess, &Self->GetInterfacer(), ProcessID, ThreadID);
			//Self->GetInterfacer()->AttachToProcess(ProcessID);
			
			//Done selecting
			Self->StopSelecting();
			
			//Console output feedback
			int TextLength = GetWindowTextLength(hwnd) + 1;
			std::wstring WindowText(TextLength, 0);
			GetWindowTextW(hwnd, &WindowText[0], TextLength);
			Self->GetConsole().Print(L"Selected - " + WindowText);
		}
	}
}

void WindowSelect::BeginSelecting(std::wstring Params)
{
	_SelectingWindow = true;
	
	_Console.Print(L"Selecting window...");
}

void WindowSelect::StopSelecting()
{
	_SelectingWindow = false;
}

void WindowSelect::RunEventProc()
{
	//Set event hook
	HWINEVENTHOOK hEvent = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, //Foreground window changed
		EVENT_SYSTEM_FOREGROUND, NULL,
		WinEventProcCallback, 0, 0,
		WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

	//Message loop
	MSG Msg;
	while(GetMessage(&Msg, NULL, 0, 0))
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}
}

HWND WindowSelect::GetSelectedHandle()
{
	return _SelectedProcessHandle;
}

void WindowSelect::SetSelectedHandle(HWND NewHandle)
{
	_SelectedProcessHandle = NewHandle;
}

ProgramInterfacer& WindowSelect::GetInterfacer() const
{
	return _Interfacer;
}

ConsoleIO& WindowSelect::GetConsole() const
{
	return _Console;
}

bool WindowSelect::SelectingWindow()
{
	return _SelectingWindow;
}