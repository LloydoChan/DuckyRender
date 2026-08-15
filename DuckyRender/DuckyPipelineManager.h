#pragma once
#include "DuckyCompiler.h"
#include "DuckyRenderTypes.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

struct ShaderDesc
{
	std::wstring File;
	std::wstring Entry = L"main";
};

struct GraphicsPipelineDesc
{
	ShaderDesc VSShader;
	ShaderDesc PSShader;
	D3D12_ROOT_PARAMETER* Params;
	UINT NumParams;
	D3D12_STATIC_SAMPLER_DESC* Samplers;
	UINT NumSamplers;
	PipelineType Type;
};

struct ComputePipelineDesc
{
	ShaderDesc CSShader;
	D3D12_ROOT_PARAMETER* Params;
	UINT NumParams;
};

struct PipelineAndRootSig
{
	ComPtr<ID3D12RootSignature> rootSig;
	ComPtr<ID3D12PipelineState> pipeLineState;
};

struct RootSignatureDesc
{
	std::vector<D3D12_ROOT_PARAMETER> parameters;
	std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers;
};

class DuckyPipelineManager
{
	public:
		bool Init(std::wofstream* FilePtr, ID3D12Device* DevicePtr);
		std::vector<D3D12_INPUT_ELEMENT_DESC> CreateInputLayout(ShaderCompilationOutput& shaderCompData);
		PipelineAndRootSig CreatePSO(GraphicsPipelineDesc& MainDesc);
		PipelineAndRootSig CreateComputePSO(ComputePipelineDesc& MainDesc);
	private:
		std::unique_ptr<DuckyCompiler> mCompiler;
		ID3D12Device* mDevicePtr;
		std::wofstream* mFilePtr;

		std::map<PipelineType, std::function<D3D12_GRAPHICS_PIPELINE_STATE_DESC()>> mPipelineStateFactory;
};