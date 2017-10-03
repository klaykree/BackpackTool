#pragma once
#include <Windows.h>
#include <memory>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>

class HwInput
{
public:
	typedef DWORD VirtualKey;
	typedef std::unordered_map<VirtualKey, std::vector<std::function<void()>>> VKCallbackMap;

    HwInput();
    ~HwInput();
    
	void BeginInputThread();

	void RegisterCallback(DWORD VirtualKey, std::function<void()> Callback);

	const VKCallbackMap& GetCallbackMap();
	const HHOOK& GetKeyboardHook();
	std::unordered_set<DWORD>& GetPressedKeys();

private:
	void _InputLoop();
	void _PressedKeyLoop();

	std::unique_ptr<std::thread> _InputThread;
	//DWORD = virtual key, vector = function callbacks (no return value, no params)
	VKCallbackMap _VKCallbacks;
	std::unordered_set<DWORD> _PressedKeys;

    HHOOK _Hook;
};