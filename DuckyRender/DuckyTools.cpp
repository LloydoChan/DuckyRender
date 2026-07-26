#include "DuckyTools.h"
#include <winrt/base.h>
#include <fstream>

bool OutputErrorFromHResult(HRESULT hResult, const char* message, std::wofstream& logFile)
{

	if (hResult != S_OK)
	{
		const winrt::hstring hResultMessage = winrt::hresult_error(hResult).message().c_str();
		logFile << message << hResultMessage.c_str() << std::endl;
		return false;
	}

	return true;
}