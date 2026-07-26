#pragma once
#include <wrl/client.h>
#include <dxcapi.h>
#include <fstream>
#include <d3d12shader.h>

using Microsoft::WRL::ComPtr;

// class for handling shader compiler requests
struct ShaderCompilationOutput
{
	ComPtr<IDxcBlob> reflectionBlob;
	ComPtr<IDxcBlob> shaderBlob;
	ComPtr<IDxcResult> result;
};

class DuckyCompiler
{
public:
	bool Init(std::wofstream* LogFilePtr);
	bool CompileShaderDXC(LPCWSTR ShaderFilePath, LPCWSTR entryPoint, LPCWSTR profile, ShaderCompilationOutput& newOutput);
	bool CreateReflectionData(DxcBuffer* ReflectionBlob, ID3D12ShaderReflection** ReflectionDataResult);
private:
	ComPtr<IDxcCompiler3> mCompiler;
	ComPtr<IDxcIncludeHandler> mIncludeHandler;
	ComPtr<IDxcUtils> mUtils;

	std::wofstream* mLogFilePtr = nullptr;
};
