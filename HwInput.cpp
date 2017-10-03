#include "HwInput.h"

HwInput* Self; //Pointer to self for LowLevelKeyboardProc to access

LRESULT CALLBACK LowLevelKeyboardProc(
    _In_ int    nCode,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam)
{
    if(nCode >= 0)
    {
        if(wParam == WM_KEYDOWN)
        {
			PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT)lParam;

			//Self->GetPressedKeys().insert(p->vkCode);

			//Call all registed callbacks associated to the pressed key
			HwInput::VKCallbackMap Callbackmap = Self->GetCallbackMap();
			auto& KeyCallbacks = Callbackmap.find(p->vkCode);
			if(KeyCallbacks != Callbackmap.end()) //Callback exists
			{
				for(const auto& KeyCallback : KeyCallbacks->second) //Call all callbacks for desired key
				{
					KeyCallback();
				}
			}
        }
		if(wParam == WM_KEYUP)
		{
			PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT)lParam;

			//Self->GetPressedKeys().erase(p->vkCode);
		}
    }
    
    return CallNextHookEx(Self->GetKeyboardHook(), nCode, wParam, lParam);
}
  
HwInput::HwInput()
{
	Self = this;
}

HwInput::~HwInput()
{
    UnhookWindowsHookEx(_Hook);
}

void HwInput::RegisterCallback(DWORD VirtualKey, std::function<void()> Callback)
{
	_VKCallbacks[VirtualKey].push_back(Callback);
}

const HwInput::VKCallbackMap& HwInput::GetCallbackMap()
{
	return _VKCallbacks;
}

const HHOOK& HwInput::GetKeyboardHook()
{
	return _Hook;
}

std::unordered_set<DWORD>& HwInput::GetPressedKeys()
{
	return _PressedKeys;
}

void HwInput::BeginInputThread()
{
	_Hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);

	_InputThread = std::make_unique<std::thread>(&HwInput::_InputLoop, this);
}

void HwInput::_PressedKeyLoop()
{
	while(true)
	{
		for(const DWORD& VK : _PressedKeys)
		{
			//Call all registed callbacks associated to the pressed key
			auto& KeyCallbacks = _VKCallbacks.find(VK);
			if(KeyCallbacks != _VKCallbacks.end()) //Callback exists
			{
				for(const auto& KeyCallback : KeyCallbacks->second) //Call all callbacks for desired key
				{
					KeyCallback();
				}
			}
		}
	}
}

void HwInput::_InputLoop()
{
	std::thread PressedKeyThread(&HwInput::_PressedKeyLoop, this);

	MSG msg;
	while(GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}