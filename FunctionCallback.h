#pragma once
#include <string>
#include <functional>

struct FunctionCallback
{
	std::wstring FunctionDescription;
	std::function<void(std::wstring)> Callback;
};

struct FunctionCallbackWithName
{
	std::wstring Name;
	FunctionCallback FunctionCB;
};