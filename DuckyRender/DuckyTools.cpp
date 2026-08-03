#include "pch.h"
#include "DuckyTools.h"

bool OutputErrorFromHResult(HRESULT hResult, const char* message, std::wofstream& logFile)
{

	if (FAILED(hResult))
	{
		const winrt::hstring hResultMessage = winrt::hresult_error(hResult).message().c_str();
		logFile << message << hResultMessage.c_str() << std::endl;
		return false;
	}

	return true;
}

void DebugMatrix(XMMATRIX& nextTransform, std::wofstream& logFile)
{
	XMFLOAT4X4 debugMatrix;
	XMStoreFloat4x4(
		&debugMatrix,
		nextTransform);

	logFile
		<< std::setw(10) << debugMatrix._41 << ", "
		<< std::setw(10) << debugMatrix._42 << ", "
		<< std::setw(10) << debugMatrix._43 << ", "
		<< std::setw(10) << debugMatrix._44 << std::endl;
}