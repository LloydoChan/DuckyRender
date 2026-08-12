#include "pch.h"
#include "DuckyPipelineManager.h"
#include "DuckyTools.h"

bool DuckyPipelineManager::Init(std::wofstream* FilePtr, ID3D12Device* DevicePtr)
{
	mFilePtr = FilePtr;
	mCompiler = std::make_unique<DuckyCompiler>();
	if (!mCompiler->Init(FilePtr)) return false;

	mDevicePtr = DevicePtr;

	return true;
}

std::vector<D3D12_INPUT_ELEMENT_DESC> DuckyPipelineManager::CreateInputLayout(ShaderCompilationOutput& shaderCompData)
{
	std::vector<D3D12_INPUT_ELEMENT_DESC> Elems;

	ComPtr<ID3D12ShaderReflection> vertReflectionData;

	DxcBuffer reflectionData = { shaderCompData.reflectionBlob->GetBufferPointer(),
								 shaderCompData.reflectionBlob->GetBufferSize(),
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

PipelineAndRootSig DuckyPipelineManager::CreatePSO(const ShaderDesc& VSShader, const ShaderDesc& PSShader, 
													D3D12_ROOT_PARAMETER* Params, UINT NumParams, 
													D3D12_STATIC_SAMPLER_DESC* Samplers, UINT NumSamplers, 
													D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc)
{
	RootSignatureDesc drawSig = {};

	for (int i = 0; i < NumParams; i++)
	{
		drawSig.parameters.emplace_back(Params[i]);
	}

	for (int i = 0; i < NumSamplers; i++)
	{
		drawSig.staticSamplers.emplace_back(Samplers[i]);
	}

	PipelineAndRootSig newPipeline;

	ShaderCompilationOutput vertexShaderOutput;
	if (!mCompiler->CompileShaderDXC(VSShader.File.c_str(), VSShader.Entry.c_str(), L"vs_6_6", vertexShaderOutput)) return newPipeline;
	ShaderCompilationOutput pixelShaderOutput;
	if (!mCompiler->CompileShaderDXC(PSShader.File.c_str(), PSShader.Entry.c_str(), L"ps_6_6", pixelShaderOutput)) return newPipeline;

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

	if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "couldn't serialize root sig ", *mFilePtr)) return newPipeline;

	hResult = mDevicePtr->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&newPipeline.rootSig));
	rootSigBlob->Release();
	if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "couldn't create root sig ", *mFilePtr)) return newPipeline;

	desc.pRootSignature = newPipeline.rootSig.Get();

	desc.VS.pShaderBytecode = vertexShaderOutput.shaderBlob.Get()->GetBufferPointer();
	desc.VS.BytecodeLength = vertexShaderOutput.shaderBlob.Get()->GetBufferSize();
	desc.PS.pShaderBytecode = pixelShaderOutput.shaderBlob.Get()->GetBufferPointer();
	desc.PS.BytecodeLength = pixelShaderOutput.shaderBlob.Get()->GetBufferSize();

	std::vector<D3D12_INPUT_ELEMENT_DESC> elems = CreateInputLayout(vertexShaderOutput);

	desc.InputLayout.pInputElementDescs = elems.data();
	desc.InputLayout.NumElements = elems.size();
	desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;

	hResult = mDevicePtr->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&newPipeline.pipeLineState));

	if (FAILED(hResult))
		OutputErrorFromHResult(hResult, "couldn't create graphics pipeline ", *mFilePtr);

	return newPipeline;
}
