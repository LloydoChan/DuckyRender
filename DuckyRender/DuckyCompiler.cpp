#include "DuckyCompiler.h"
#include "DuckyTools.h"
#include <vector>

#pragma comment(lib, "dxcompiler.lib")

bool DuckyCompiler::Init(std::wofstream* LogFilePtr)
{
	mLogFilePtr = LogFilePtr;

	// create compilers
	HRESULT hResult = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(mUtils.ReleaseAndGetAddressOf()));
	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating utils: ", *mLogFilePtr)) return false;

	hResult = mUtils->CreateDefaultIncludeHandler(mIncludeHandler.ReleaseAndGetAddressOf());
	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating include handler: ", *mLogFilePtr)) return false;

	hResult = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&mCompiler));
	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating compiler: ", *mLogFilePtr)) return false;

	return true;
}

bool DuckyCompiler::CompileShaderDXC(LPCWSTR ShaderFilePath, LPCWSTR entryPoint, LPCWSTR profile, ShaderCompilationOutput& newOutput)
{
	std::ifstream file(ShaderFilePath, std::ios::binary | std::ios::ate);

	if (!file) return false;

	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);
	std::vector<char> buffer(size);
	file.read(buffer.data(), size);

	// 4. Populate DxcBuffer
	DxcBuffer dxcBuffer;
	dxcBuffer.Ptr = buffer.data();
	dxcBuffer.Size = buffer.size();
	dxcBuffer.Encoding = DXC_CP_UTF8;

	std::vector<LPCWSTR> arguments;
	//entrypoint
	arguments.push_back(L"-E");
	arguments.push_back(entryPoint);

	//profile
	arguments.push_back(L"-T");
	arguments.push_back(profile);

	HRESULT hResult = mCompiler->Compile(&dxcBuffer, arguments.data(), (UINT32)arguments.size(), mIncludeHandler.Get(), IID_PPV_ARGS(&newOutput.result));
	if (hResult != S_OK && OutputErrorFromHResult(hResult, "couldn't compile shader", *mLogFilePtr)) return false;

	hResult = newOutput.result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&newOutput.reflectionBlob), nullptr);
	if (hResult != S_OK && OutputErrorFromHResult(hResult, "couldn't get reflection from result", *mLogFilePtr)) return false;

	hResult = newOutput.result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&newOutput.shaderBlob), nullptr);
	if (hResult != S_OK && OutputErrorFromHResult(hResult, "couldn't get shader info from result", *mLogFilePtr)) return false;

	return true;
}

bool DuckyCompiler::CreateReflectionData(DxcBuffer* ReflectionBlob, ID3D12ShaderReflection** ReflectionDataResult)
{
	HRESULT hResult = mUtils->CreateReflection(ReflectionBlob, IID_PPV_ARGS(ReflectionDataResult));
	if (hResult != S_OK && OutputErrorFromHResult(hResult, "couldn't get shader reflection info", *mLogFilePtr)) return false;
	return true;
}
