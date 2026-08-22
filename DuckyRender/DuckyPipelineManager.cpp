#include "pch.h"
#include "DuckyPipelineManager.h"
#include "DuckyTools.h"
#include "DuckyRenderTypes.h"
#include "DuckyPipelineStates.h"

bool DuckyPipelineManager::Init(ID3D12Device* DevicePtr)
{
	mCompiler = std::make_unique<DuckyCompiler>();
	if (!mCompiler->Init()) return false;

	mDevicePtr = DevicePtr;

	mPipelineStateFactory[PipelineType::OPAQUE]		= MakeOpaquePipelineState;
	mPipelineStateFactory[PipelineType::ALPHA]		= MakeTransparentPipelineState;
	mPipelineStateFactory[PipelineType::MASKED]		= MakeMaskedPipelineState;
	mPipelineStateFactory[PipelineType::OPAQUE_DBL] = MakeDoubleSidedOpaquePipelineState;
	mPipelineStateFactory[PipelineType::ALPHA_DBL]  = MakeDoubleSidedTransparentPipelineState;
	mPipelineStateFactory[PipelineType::MASKED_DBL] = MakeDoubleSidedMaskedPipelineState;
	mPipelineStateFactory[PipelineType::DEBUG]      = MakeDebugDrawPipelineState;

	mShaderPath = GetExecutableDirectory();

	return true;
}

std::vector<D3D12_INPUT_ELEMENT_DESC> DuckyPipelineManager::CreateInputLayout(const void* ShaderData, size_t ShaderSize)
{
	std::vector<D3D12_INPUT_ELEMENT_DESC> Elems;

	ComPtr<ID3D12ShaderReflection> vertReflectionData;

	DxcBuffer reflectionData = { ShaderData,
								 ShaderSize,
								 0U };

	if (!mCompiler->CreateReflectionData(&reflectionData, vertReflectionData.ReleaseAndGetAddressOf())) return Elems;

	D3D12_SHADER_DESC shaderDesc;
	vertReflectionData->GetDesc(&shaderDesc);

	for (UINT i = 0; i < shaderDesc.InputParameters; i++)
	{
		D3D12_SIGNATURE_PARAMETER_DESC paramDesc;
		vertReflectionData->GetInputParameterDesc(i, &paramDesc);

		if (paramDesc.SystemValueType != D3D_NAME_UNDEFINED)
		{
			continue;
		}

		// assume is a position and just change where needed
		D3D12_INPUT_ELEMENT_DESC currentDesc = {
				"POSITION",
				 0,
				 DXGI_FORMAT_R32G32B32_FLOAT,
				 0,
				 D3D12_APPEND_ALIGNED_ELEMENT,
				 D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				 0
		};

		if (std::strcmp(paramDesc.SemanticName, "TEXCOORD") == 0)
		{
			currentDesc.SemanticName = "TEXCOORD";
			currentDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
		}
		else if (std::strcmp(paramDesc.SemanticName, "NORMAL") == 0)
		{
			currentDesc.SemanticName = "NORMAL";
			currentDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
		}
		else if (std::strcmp(paramDesc.SemanticName, "TANGENT") == 0)
		{
			currentDesc.SemanticName = "TANGENT";
			currentDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		}
		else if (std::strcmp(paramDesc.SemanticName, "COLOR") == 0)
		{
			currentDesc.SemanticName = "COLOR";
			currentDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		}

		Elems.push_back(currentDesc);
	}

	return Elems;
}

PipelineAndRootSig DuckyPipelineManager::CreatePSO(GraphicsPipelineDesc& mainDesc)
{
	RootSignatureDesc drawSig = {};

	for (int i = 0; i < mainDesc.NumParams; i++)
	{
		drawSig.parameters.emplace_back(mainDesc.Params[i]);
	}

	for (int i = 0; i < mainDesc.NumSamplers; i++)
	{
		drawSig.staticSamplers.emplace_back(mainDesc.Samplers[i]);
	}

	PipelineAndRootSig newPipeline;
#ifdef _DEBUG
	ShaderCompilationOutput vertexShaderOutput;
	if (!mCompiler->CompileShaderDXC(mainDesc.VSShader.File.c_str(), mainDesc.VSShader.Entry.c_str(), L"vs_6_6", vertexShaderOutput))
	{
		DuckyLog::Error(std::wstring_view(L"Couldn't Compile vertex shader: " + mainDesc.VSShader.File));
		return newPipeline;
	}

	ShaderCompilationOutput pixelShaderOutput;
	if (!mCompiler->CompileShaderDXC(mainDesc.PSShader.File.c_str(), mainDesc.PSShader.Entry.c_str(), L"ps_6_6", pixelShaderOutput))
	{
		DuckyLog::Error(std::wstring_view(L"Couldn't Compile pixel shader: " + mainDesc.PSShader.File));
		return newPipeline;
	}
#else
	std::vector<std::byte> vs = LoadShaderBytecode(mShaderPath / mainDesc.VSShader.File.c_str());
	std::vector<std::byte> ps = LoadShaderBytecode(mShaderPath / mainDesc.PSShader.File.c_str());
#endif


	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
	rootSigDesc.NumParameters = static_cast<UINT>(drawSig.parameters.size());
	rootSigDesc.pParameters = drawSig.parameters.empty() ? nullptr : drawSig.parameters.data();
	rootSigDesc.NumStaticSamplers = static_cast<UINT>(drawSig.staticSamplers.size());
	rootSigDesc.pStaticSamplers = drawSig.staticSamplers.empty() ? nullptr : drawSig.staticSamplers.data();

	ID3DBlob* rootSigBlob = nullptr;

	HRESULT hResult = D3D12SerializeRootSignature(
		&rootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob,
		nullptr
	);

	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"couldn't serialize root sig"))) return newPipeline;

	hResult = mDevicePtr->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&newPipeline.rootSig));
	rootSigBlob->Release();
	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"couldn't create root sig"))) return newPipeline;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = mPipelineStateFactory[mainDesc.Type]();

	desc.pRootSignature = newPipeline.rootSig.Get();

#ifdef _DEBUG
	desc.VS.pShaderBytecode = vertexShaderOutput.shaderBlob.Get()->GetBufferPointer();
	desc.VS.BytecodeLength  = vertexShaderOutput.shaderBlob.Get()->GetBufferSize();
	desc.PS.pShaderBytecode = pixelShaderOutput.shaderBlob.Get()->GetBufferPointer();
	desc.PS.BytecodeLength  = pixelShaderOutput.shaderBlob.Get()->GetBufferSize();

	std::vector<D3D12_INPUT_ELEMENT_DESC> elems = CreateInputLayout(desc.VS.pShaderBytecode, desc.VS.BytecodeLength);
#else
	desc.VS.pShaderBytecode = vs.data();
	desc.VS.BytecodeLength  = vs.size();
	desc.PS.pShaderBytecode = ps.data();
	desc.PS.BytecodeLength  = ps.size();

	std::vector<D3D12_INPUT_ELEMENT_DESC> elems = CreateInputLayout(vs.data(), vs.size());
#endif

	desc.InputLayout.pInputElementDescs = elems.data();
	desc.InputLayout.NumElements = elems.size();
	desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;

	hResult = mDevicePtr->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&newPipeline.pipeLineState));

	DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"couldn't create pipeline")); 

	return newPipeline;
}

PipelineAndRootSig DuckyPipelineManager::CreateComputePSO(ComputePipelineDesc& MainDesc)
{
	RootSignatureDesc compSig = {};

	for (int i = 0; i < MainDesc.NumParams; i++)
	{
		compSig.parameters.emplace_back(MainDesc.Params[i]);
	}

	PipelineAndRootSig newPipeline;

	ShaderCompilationOutput computeShaderOutput;
	if (!mCompiler->CompileShaderDXC(MainDesc.CSShader.File.c_str(), MainDesc.CSShader.Entry.c_str(), L"cs_6_6", computeShaderOutput))
	{
		DuckyLog::Error(std::wstring_view(L"Couldn't Compile vertex shader: " + MainDesc.CSShader.File));
		return newPipeline;
	}

	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	rootSigDesc.NumParameters = static_cast<UINT>(compSig.parameters.size());
	rootSigDesc.pParameters = compSig.parameters.empty() ? nullptr : compSig.parameters.data();
	rootSigDesc.NumStaticSamplers = 0;
	rootSigDesc.pStaticSamplers = nullptr;

	ID3DBlob* rootSigBlob = nullptr;

	HRESULT hResult = D3D12SerializeRootSignature(
		&rootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob,
		nullptr
	);

	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"couldn't serialize root sig"))) return newPipeline;

	hResult = mDevicePtr->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&newPipeline.rootSig));
	rootSigBlob->Release();
	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"couldn't create root sig"))) return newPipeline;

	D3D12_COMPUTE_PIPELINE_STATE_DESC comDesc{};

	comDesc.CS.BytecodeLength = computeShaderOutput.shaderBlob.Get()->GetBufferSize();
	comDesc.CS.pShaderBytecode = computeShaderOutput.shaderBlob.Get()->GetBufferPointer();
	comDesc.pRootSignature = newPipeline.rootSig.Get();

	hResult = mDevicePtr->CreateComputePipelineState(&comDesc, IID_PPV_ARGS(&newPipeline.pipeLineState));

	DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"couldn't create compute pipeline"));

	return newPipeline;
}

std::vector<std::byte> LoadShaderBytecode(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);

	if (!file) throw std::runtime_error("Failed to open shader");

	const auto size = file.tellg();
	file.seekg(0);
	std::vector<std::byte> data(static_cast<size_t>(size));
	file.read(reinterpret_cast<char*>(data.data()),size);

	return data;
}
