#pragma once
#include <Windows.h>
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include "FunctionCallback.h"
#include "HwInput.h"

class ConsoleIO;

class ProgramInterfacer
{
public:
    ProgramInterfacer(ConsoleIO& Console, HwInput& Input);
    ~ProgramInterfacer();
    
    void OpenFileDialog(std::wstring Params);
    
    void AttachToProcess();

	void Pause();
	void Unpause();

    void TogglePause();
    
    void SaveMemory();
    void LoadMemory();
    
private:
	struct SearchableByte
	{
		uint8_t Byte; //Value to compare
		bool Searchable; //Flag to use or ignore the Byte
	};

    void _ProcessLoop(std::wstring& FileName);
	LPVOID _FindFunctionLocation(LPCVOID StartAddress);
	void _SetPauseBreakpoint(HANDLE Thread, DWORD BreakpointAddressOffset);
	void _RemovePauseBreakpoint(HANDLE Thread);
	bool _ByteMatch(const std::vector<SearchableByte>& SearchBytes, const std::vector<uint8_t>& ByteBuffer);

	volatile std::atomic<bool> _Paused = false;

	HMODULE _GetBaseAddress(HANDLE Handle);

	HANDLE _ProcessHandle = 0;
    DWORD _ProcessID = 0;
	HANDLE _ProcessThreadHandle = 0;
	DWORD _ProcessThreadID = 0;

	void _IterateThreads();
	std::atomic<bool> _LookingForPauseThread = false;
	std::atomic<HANDLE> _PauseThreadHandle = 0;
	std::vector<HANDLE> _ThreadHandles;
	std::unique_ptr<std::thread> _FindProcessThread;

	std::unique_ptr<std::thread> _ProcessThreadLoop;
    
    ConsoleIO& _Console;
	HwInput& _Input;

	LPVOID _DxPresentCallLocation;
};