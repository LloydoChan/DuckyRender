#pragma once
#include <Windows.h>
#include <fstream>
#include <DirectXMath.h>

using namespace DirectX;

bool OutputErrorFromHResult(HRESULT hResult, const char* message, std::wofstream& logFile);
void DebugMatrix(XMMATRIX& nextTransform, std::wofstream& logFile);

constexpr UINT64 AlignConstantBufferSize(UINT64 size)
{
	return (size + 255) & ~255;
}