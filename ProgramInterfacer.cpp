#include "ProgramInterfacer.h"
#include "ConsoleIO.h"
#include "SharedMem.h"
#include "TlHelp32.h"
#include <Aclapi.h>
#include <Psapi.h>
#include <unordered_map>

#pragma warning (disable: 4996)

ProgramInterfacer::ProgramInterfacer(ConsoleIO& Console, HwInput& Input)
	:_Console(Console), _Input(Input)
{
	FunctionCallback OpenCB;
	OpenCB.FunctionDescription = L"Opens game";
	OpenCB.Callback = std::bind(&ProgramInterfacer::StartFileDialog, this, std::placeholders::_1);
	Console.RegisterCallback({ L"start game", OpenCB });

	FunctionCallback PauseCB;
	PauseCB.FunctionDescription = L"Pauses selected process";
	PauseCB.Callback = std::bind(&ProgramInterfacer::TogglePause, this);
	Console.RegisterCallback({ L"pause", PauseCB });

	_Input.RegisterCallback(VK_OEM_3, std::bind(&ProgramInterfacer::TogglePause, this)); //VK_OEM_3 should be grave/tilde (`~)
}

ProgramInterfacer::~ProgramInterfacer()
{}

void ProgramInterfacer::StartFileDialog(std::wstring Params)
{
	_ProcessThreadLoop = std::make_unique<std::thread>(&ProgramInterfacer::_OpenFileDialog, this, Params);
}

void ProgramInterfacer::_OpenFileDialog(std::wstring Params)
{
	OPENFILENAMEW FileInfo = {}; //Init stuct to zero
    FileInfo.lStructSize = sizeof(FileInfo);
	FileInfo.hwndOwner = NULL;
	std::wstring FileName(260, 0);
	FileInfo.lpstrFile = &FileName[0];
	FileInfo.nMaxFile = static_cast<DWORD>(FileName.size());
	FileInfo.lpstrFilter = L"Program\0*.exe;*.bat;*.lnk\0"; //Can open executable, batch, and link files
	FileInfo.nFilterIndex = 1;
	FileInfo.lpstrFileTitle = NULL;
	FileInfo.nMaxFileTitle = 0;
	FileInfo.lpstrInitialDir = NULL;
	FileInfo.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
	
    BOOL SelectedOpened = GetOpenFileNameW(&FileInfo);
    if(SelectedOpened)
    {
		_Console.Print(L"Starting process...");
		size_t FileDirNameLength = FileName.find(L'\0') + 1; //Find first null to get actual directory length

		STARTUPINFOW StartInfo = {};
		PROCESS_INFORMATION ProcessInfo = {};
		StartInfo.cb = sizeof(StartInfo);

		BOOL ProcessCreated = CreateProcessW(FileName.c_str(), NULL, NULL, NULL, TRUE, DEBUG_ONLY_THIS_PROCESS, NULL, NULL, &StartInfo, &ProcessInfo);
		_ProcessID = ProcessInfo.dwProcessId;
		_ProcessThreadID = ProcessInfo.dwThreadId;

		AttachToProcess();
	}
	else
	{
		_Console.Print(L"No process started");
	}
}

HMODULE ProgramInterfacer::_GetBaseAddress(HANDLE Handle)
{
	HMODULE hMods[1024];
	DWORD cbNeeded;
	unsigned int i;

	std::wstring ExePath(MAX_PATH, 0);
	GetModuleFileNameExW(Handle, NULL, &ExePath[0], MAX_PATH);
	size_t ExeNameStart = ExePath.find_last_of(L"\\");
	ExePath = ExePath.substr(ExeNameStart + 1);

	if(EnumProcessModules(Handle, hMods, sizeof(hMods), &cbNeeded))
	{
		for(i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
		{
			std::wstring wstrModName(MAX_PATH, 0);
			if(GetModuleFileNameExW(Handle, hMods[i], &wstrModName[0], static_cast<DWORD>(wstrModName.capacity())))
			{
				size_t ModuleNameStart = wstrModName.find_last_of(L"\\");
				wstrModName = wstrModName.substr(ModuleNameStart + 1);
				if(wstrModName.find(ExePath) != std::wstring::npos)
				{
					return hMods[i];
				}
			}
		}
	}
	return nullptr;
}

void ProgramInterfacer::AttachToProcess()
{
	_ProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, _ProcessID);
	_ProcessThreadHandle = OpenThread(THREAD_ALL_ACCESS, FALSE, _ProcessThreadID);

	BOOL Success = DebugActiveProcess(_ProcessID);

	DEBUG_EVENT DebugEvent = {};
	while(WaitForDebugEvent(&DebugEvent, INFINITE)) //Debug loop
	{
		if(DebugEvent.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT)
		{
			_Console.Print(L"Create process event");
		}
		else if(DebugEvent.dwDebugEventCode == CREATE_THREAD_DEBUG_EVENT)
		{
			_Console.Print(L"Create thread event");

			_ThreadHandles.push_back(DebugEvent.u.CreateThread.hThread);
		}
		else if(DebugEvent.dwDebugEventCode == EXIT_THREAD_DEBUG_EVENT)
		{
			_Console.Print(L"Exit thread event");

			//_ThreadHandles.erase(DebugEvent.u.CreateThread.hThread);
		}
		else if(DebugEvent.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT)
		{
			auto DllInfo = DebugEvent.u.LoadDll;
			std::wstring DllPath(260, 0);
			auto PathLength = GetFinalPathNameByHandleW(DllInfo.hFile, &DllPath[0], 260, VOLUME_NAME_NONE);
			std::wstring DllName = DllPath.substr(0, PathLength);
			_Console.Print(L"DLL loaded: " + DllName);

			if(DllName.find(L"d3d9") != std::wstring::npos) //Directx 9 dll was found
			{
				_DxPresentCallLocation = _FindFunctionLocation(DebugEvent.u.LoadDll.lpBaseOfDll);
			}
		}
		else if(DebugEvent.dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
		{
			bool HitFirstBreakpoint = _DxPresentCallLocation == DebugEvent.u.Exception.ExceptionRecord.ExceptionAddress;
			bool HitSecondBreakpoint = (LPVOID)((DWORD)_DxPresentCallLocation + 2) == (LPVOID)((DWORD)DebugEvent.u.Exception.ExceptionRecord.ExceptionAddress);

			if(_LookingForPauseThread && HitFirstBreakpoint)
			{
				_Console.Print(L"------------Found pause thread-------------");
				_LookingForPauseThread = false;

				ContinueDebugEvent(DebugEvent.dwProcessId, DebugEvent.dwThreadId, DBG_CONTINUE);
				continue;
			}
			
			if(!_LookingForPauseThread)
			{
				if(_Paused && HitFirstBreakpoint)
				{
					_RemovePauseBreakpoint(_PauseThreadHandle); //Remove start breakpoint

					//Halt debugger to halt debuggee
					while(_Paused) {}

					_Console.Print(L"Unpausing");

					_SetPauseBreakpoint(_PauseThreadHandle, 2); //Set second breakpoint
				}
				else if(HitSecondBreakpoint)
				{
					_Console.Print(L"Swapping breakpoints");
					_RemovePauseBreakpoint(_PauseThreadHandle); //Remove second breakpoint
					_SetPauseBreakpoint(_PauseThreadHandle, 0); //Reset first breakpoint
					_Paused = true;
				}
			}
		}

		ContinueDebugEvent(DebugEvent.dwProcessId, DebugEvent.dwThreadId, DBG_CONTINUE);
	}
}

bool ProgramInterfacer::_ByteMatch(const std::vector<SearchableByte>& SearchBytes, const std::vector<uint8_t>& ByteBuffer)
{
	for(size_t i = 0 ; i < SearchBytes.size() ; ++i)
	{
		if(SearchBytes[i].Searchable)
		{
			if(SearchBytes[i].Byte != ByteBuffer[i])
			{
				return false;
			}
		}
	}

	return true;
}

LPVOID ProgramInterfacer::_FindFunctionLocation(LPCVOID StartAddress)
{
	SYSTEM_INFO si = {};
	GetSystemInfo(&si);

	MEMORY_BASIC_INFORMATION MemoryInfo = {};
	unsigned char* Position = (unsigned char*)StartAddress;
	LPVOID SignatureAddress = NULL;

#ifdef _DEBUG
	return (LPVOID)0x72259e00; //Searching for function signature is slow in debug so return early with 'known' address
#endif

	std::vector<SearchableByte> FunctionSignature =
	{
		{ 0x8B, true }, { 0xFF, true },																							//MOV EDI,EDI
		{ 0x55, true },																											//PUSH EBP
		{ 0x8B, true }, { 0xEC, true },																							//MOV EBP,ESP
		{ 0x83, true }, { 0xE4, true }, { 0xF8, true },																			//AND ESP,FFFFFFF8
		{ 0x83, true }, { 0xEC, true }, { 0x10, true },																			//SUB ESP,10
		{ 0xA1, false }, { 0xA4, false }, { 0x81, false }, { 0xF6, false }, { 0x71, false },									//MOV EAX,DWORD PTR DS:[71F681A4]
		{ 0x33, true }, { 0xC4, true },																							//XOR EAX,ESP
		{ 0x89, true }, { 0x44, false }, { 0x24, false }, { 0x0C, false },														//MOV DWORD PTR SS:[ESP+C],EAX
		{ 0x56, true },																											//PUSH ESI
		{ 0x8B, true }, { 0x75, false }, { 0x08, false },																		//MOV ESI,DWORD PTR SS:[EBP+8]
		{ 0x8B, true }, { 0xCE, true },																							//MOV ECX,ESI
		{ 0xF7, true }, { 0xD9, true },																							//NEG ECX
		{ 0x57, true },																											//PUSH EDI
		{ 0x1B, true }, { 0xC9, true },																							//SBB ECX,ECX
		{ 0x8D, true }, { 0x46, false }, { 0x04, false },																		//LEA EAX,DWORD PTR DS:[ESI+4]
		{ 0x23, true }, { 0xC8, true },																							//AND ECX,EAX
		{ 0x6A, true }, { 0x00, true },																							//PUSH 0
		{ 0x51, true },																											//PUSH ECX
		{ 0x8D, true }, { 0x4C, false }, { 0x24, false }, { 0x14, false },														//LEA ECX,DWORD PTR SS:[ESP+14]
		{ 0xE8, true }, { 0x2F, false }, { 0x47, false }, { 0xF8, false }, { 0xFF, false },										//CALL d3d9.71E7E561
		{ 0xF7, true }, { 0x46, false }, { 0x2C, false }, { 0x02, false }, { 0x00, false }, { 0x00, false }, { 0x00, false },	//TEST DWORD PTR DS:[ESI+2C],2
		{ 0x74, false }, { 0x07, false },																						//JE SHORT d3d9.71EF9E42
		{ 0xBF, false }, { 0x6C, false }, { 0x08, false }, { 0x76, false }, { 0x88, false },									//MOV EDI,8876086C
		{ 0xEB, true }, { 0x17, false },																						//JMP SHORT d3d9.71EF9E59
		{ 0x6A, true }, { 0x00, true },																							//PUSH 0
		{ 0xFF, false }, { 0x75, false }, { 0x18, false },																		//PUSH DWORD PTR SS:[EBP+18]
		{ 0x8B, true }, { 0xCE, true },																							//MOV ECX,ESI
		{ 0xFF, false }, { 0x75, false }, { 0x14, false },																		//PUSH DWORD PTR SS:[EBP+14]
		{ 0xFF, false }, { 0x75, false }, { 0x10, false },																		//PUSH DWORD PTR SS:[EBP+10]
		{ 0xFF, false }, { 0x75, false }, { 0x0C, false },																		//PUSH DWORD PTR SS:[EBP+C]
		{ 0xE8, true }, { 0x0D, false }, { 0x01, false }, { 0x00, false }, { 0x00, false },										//CALL d3d9.71EF9F64
		{ 0x8B, true }, { 0xF8, true },																							//MOV EDI,EAX
		{ 0x8B, true }, { 0x74, false }, { 0x24, false }, { 0x0C, false },														//MOV ESI,DWORD PTR SS:[ESP+C]
		{ 0x83, false }, { 0x7E, false }, { 0x18, false }, { 0x00, false },														//CMP DWORD PTR DS:[ESI+18],0
		{ 0x74, false }, { 0x07, false },																						//JE SHORT d3d9.71EF9E6A
		{ 0x56, true },																											//PUSH ESI
		{ 0xFF, false }, { 0x15, false }, { 0x88, false }, { 0xF3, false }, { 0xF6, false }, { 0x71, false },					//CALL DWORD PTR DS:[<&KERNEL32.LeaveCriti> ; ntdll.RtlLeaveCriticalSection
		{ 0x83, false }, { 0x7C, false }, { 0x24, false }, { 0x10, false }, { 0x00, false },									//CMP DWORD PTR SS:[ESP+10],0
		{ 0x74, false }, { 0x13, false },																						//JE SHORT d3d9.71EF9E84
		{ 0x8B, true }, { 0x46, false }, { 0x20, false },																		//MOV EAX,DWORD PTR DS:[ESI+20]
		{ 0x50, true },																											//PUSH EAX
		{ 0x8B, true }, { 0x08, false },																						//MOV ECX,DWORD PTR DS:[EAX]
		{ 0x8B, true }, { 0x71, false }, { 0x08, false },																		//MOV ESI,DWORD PTR DS:[ECX+8]
		{ 0x8B, true }, { 0xCE, false },																						//MOV ECX,ESI
		{ 0xFF, false }, { 0x15, false }, { 0x74, false }, { 0xF6, false }, { 0xF6, false }, { 0x71, false },					//CALL DWORD PTR DS:[71F6F674] ;d3d9.71E886C0
		{ 0xFF, false }, { 0xD6, false },																						//CALL ESI
		{ 0x8B, true }, { 0x4C, false }, { 0x24, false }, { 0x14, false },														//MOV ECX,DWORD PTR SS:[ESP+14]
		{ 0x8B, true }, { 0xC7, true },																							//MOV EAX,EDI
		{ 0x5F, true },																											//POP EDI
		{ 0x5E, true },																											//POP ESI
		{ 0x33, true }, { 0xCC, true },																							//XOR ECX,ESP
		{ 0xE8, true }, { 0x8D, false }, { 0x91, false }, { 0xF9, false }, { 0xFF, false },										//CALL d3d9.71E93020
		{ 0x8B, true }, { 0xE5, true },																							//MOV ESP,EBP
		{ 0x5D, true },																											//POP EBP
		{ 0xC2, true }, { 0x14, true }, { 0x00, true }																			//RETN 14
	};

	std::vector<uint8_t> BufferSliceCopy(FunctionSignature.size());

	uint32_t SignaturesFound = 0;

	while(Position < si.lpMaximumApplicationAddress)
	{
		SIZE_T MemSize = VirtualQueryEx(_ProcessHandle, (LPCVOID)Position, &MemoryInfo, sizeof(MemoryInfo));

		if(MemoryInfo.State & MEM_COMMIT &&
			(MemoryInfo.Protect == PAGE_EXECUTE_READWRITE ||
				MemoryInfo.Protect == PAGE_EXECUTE_READ ||
				MemoryInfo.Protect == PAGE_EXECUTE ||
				MemoryInfo.Protect == PAGE_EXECUTE_WRITECOPY))
		{
			std::vector<uint8_t> Buffer(MemoryInfo.RegionSize);
			SIZE_T BytesRead;
			BOOL Success = ReadProcessMemory(_ProcessHandle, (LPCVOID)MemoryInfo.BaseAddress, &Buffer[0], MemoryInfo.RegionSize, &BytesRead);
			if(Success != 0)
			{
				for(SIZE_T i = 0 ; i < MemoryInfo.RegionSize - FunctionSignature.size() ; ++i)
				{
					std::copy(Buffer.begin() + i,
						Buffer.begin() + i + FunctionSignature.size(),
						BufferSliceCopy.begin());

					if(_ByteMatch(FunctionSignature, BufferSliceCopy))
					{
						_Console.Print(L"Found function call");

						SignatureAddress = static_cast<LPVOID>(static_cast<char*>(MemoryInfo.BaseAddress) + i);
						
						++SignaturesFound;
					}
				}
			}
			else
			{
				_Console.Print(L"Failed to read");
			}

			_Console.Print(L"Found commited memory");
		}

		Position += MemoryInfo.RegionSize;
	}

	if(SignaturesFound == 0)
	{
		_Console.Print(L"---------No function signatures found---------");
	}

	if(SignaturesFound > 1)
	{
		_Console.Print(L"---------More than one function signature found---------");
	}

	return SignatureAddress;
}

void ProgramInterfacer::_IterateThreads()
{
	auto DebugTime = std::chrono::milliseconds(150);

	//Test the main thread of the process before testing the others
	_SetPauseBreakpoint(_ProcessThreadHandle, 0);
	std::this_thread::sleep_for(DebugTime); //Give the debug loop thread time to hit
	_RemovePauseBreakpoint(_ProcessThreadHandle);
	if(!_LookingForPauseThread)
	{
		_PauseThreadHandle = _ProcessThreadHandle; //Handle has been found
		return;
	}

	_Console.Print(L"Testing " + std::to_wstring(_ThreadHandles.size()) + L" threads");

	for(auto& ThreadHandle : _ThreadHandles)
	{
		_Console.Print(L"Testing thread handle: " + std::to_wstring((DWORD)ThreadHandle));

		_SetPauseBreakpoint(ThreadHandle, 0);
		std::this_thread::sleep_for(DebugTime); //Give the debug loop thread time to hit
		_RemovePauseBreakpoint(ThreadHandle);

		//_LookingForPauseThread will be set to false when the debug loop hits the correct breakpoint
		if(!_LookingForPauseThread)
		{
			_PauseThreadHandle = ThreadHandle; //Handle has been found
			return;
		}
	}
}

void ProgramInterfacer::Pause()
{
	if(_PauseThreadHandle == 0)
	{
		_Console.Print(L"Finding pause thread");
		_LookingForPauseThread = true;
		_FindProcessThread = std::make_unique<std::thread>(&ProgramInterfacer::_IterateThreads, this);
		_FindProcessThread->join();
	}
	else
	{
		_Console.Print(L"Pausing");
		_Paused = true;

		_SetPauseBreakpoint(_PauseThreadHandle, 0);
	}
}

inline void SetBits(unsigned long& dw, int lowBit, int bits, int newValue)
{
	int mask = (1 << bits) - 1; // e.g. 1 becomes 0001, 2 becomes 0011, 3 becomes 0111

	dw = (dw & ~(mask << lowBit)) | (newValue << lowBit);
}

void ProgramInterfacer::_SetPauseBreakpoint(HANDLE Thread, DWORD BreakpointAddressOffset)
{
	//Software breakpoint
	/*uint8_t Int3 = 0xCC; //Breakpoint
	SIZE_T Written;
	BOOL WriteSuccess = WriteProcessMemory(_ProcessHandle, _DxPresentCallLocation, &Int3, 1, &Written);
	BOOL FlushSuccess = FlushInstructionCache(_ProcessHandle, _DxPresentCallLocation, 1);*/

	DWORD Suspend = SuspendThread(Thread);

	//Hardware breakpoint
	CONTEXT lcContext;
	lcContext.ContextFlags = CONTEXT_DEBUG_REGISTERS;
	BOOL Success = GetThreadContext(Thread, &lcContext);

	lcContext.Dr0 = (DWORD)_DxPresentCallLocation + BreakpointAddressOffset;
	lcContext.Dr7 = 0;
	SetBits(lcContext.Dr7, 16, 2, 0); //Condition (on code execute)
	SetBits(lcContext.Dr7, 18, 2, 0); //Length (1 byte)
	SetBits(lcContext.Dr7, 0, 1, 1); //Activate local breakpoint

	Success = SetThreadContext(Thread, &lcContext);

	Suspend = ResumeThread(Thread);
}

void ProgramInterfacer::Unpause()
{
	_Paused = false;
}

void ProgramInterfacer::_RemovePauseBreakpoint(HANDLE Thread)
{
	//Software breakpoint
	/*uint8_t PrevousOpCode = 0x8B;
	SIZE_T Written;
	BOOL WriteSuccess = WriteProcessMemory(_ProcessHandle, _DxPresentCallLocation, &PrevousOpCode, 1, &Written);
	BOOL FushSuccess = FlushInstructionCache(_ProcessHandle, _DxPresentCallLocation, 1);*/

	//Hardware breakpoint
	DWORD Suspend = SuspendThread(Thread);

	CONTEXT lcContext;
	lcContext.ContextFlags = CONTEXT_DEBUG_REGISTERS;
	BOOL Success = GetThreadContext(Thread, &lcContext);
	lcContext.Dr0 = 0; //Unset breakpoint address
	lcContext.Dr6 = 0;
	lcContext.Dr7 = 0; //Deactivate breakpoint
	Success = SetThreadContext(Thread, &lcContext);

	Suspend = ResumeThread(Thread);
}

void ProgramInterfacer::TogglePause()
{
	if(_Paused)
	{
		Unpause();
	}
	else
	{
		Pause();
	}
}