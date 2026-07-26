#pragma once
#include <Windows.h>
#include <fstream>

bool OutputErrorFromHResult(HRESULT hResult, const char* message, std::wofstream& logFile);