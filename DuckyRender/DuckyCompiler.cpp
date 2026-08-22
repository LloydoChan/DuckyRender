#include "pch.h"
#include "DuckyCompiler.h"
#include "DuckyTools.h"

#pragma comment(lib, "dxcompiler.lib")

bool DuckyCompiler::Init()
{
	// create compilers
	HRESULT hResult = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(mUtils.ReleaseAndGetAddressOf()));
	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"problem creating utils"))) return false;

	hResult = mUtils->CreateDefaultIncludeHandler(mIncludeHandler.ReleaseAndGetAddressOf());
	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"problem creating include Handler"))) return false;

	hResult = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&mCompiler));
	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"problem creating compiler"))) return false;

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
	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"problem compiling shader"))) return false;

	HRESULT compileStatus;
	hResult = newOutput.result->GetStatus(&compileStatus);
	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"couldn't obtain  shader compiling status"))) return false;

	if (FAILED(compileStatus))
	{
		ComPtr<IDxcBlobUtf8> errors;
		newOutput.result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errors.ReleaseAndGetAddressOf()), nullptr);

		if (errors && errors->GetStringLength() > 0)
		{
			const wchar_t* wide_ptr = reinterpret_cast<const wchar_t*>(errors->GetStringPointer());
			size_t wide_count = errors->GetStringLength() / sizeof(wchar_t);
			DuckyLog::Error(std::wstring_view(wide_ptr,wide_count));
		}

		return false;
	}
		
	hResult = newOutput.result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&newOutput.reflectionBlob), nullptr);
	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"couldn't get reflection from result"))) return false;

	hResult = newOutput.result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&newOutput.shaderBlob), nullptr);
	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"couldn't get shader reflection from result"))) return false;

	return true;
}

bool DuckyCompiler::CreateReflectionData(DxcBuffer* ReflectionBlob, ID3D12ShaderReflection** ReflectionDataResult)
{
	HRESULT hResult = mUtils->CreateReflection(ReflectionBlob, IID_PPV_ARGS(ReflectionDataResult));
	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"couldn't get shader reflection info"))) return false;
	return true;
}
