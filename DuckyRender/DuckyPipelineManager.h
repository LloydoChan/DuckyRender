#pragma once
#include "DuckyCompiler.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;


struct ShaderDesc
{
	std::wstring File;
	std::wstring Entry = L"main";
};

struct GraphicsPipelineDesc
{
	ShaderDesc VS;
	ShaderDesc PS;

	D3D12_BLEND_DESC BlendState;
	D3D12_RASTERIZER_DESC RasterizerState;
	D3D12_DEPTH_STENCIL_DESC DepthStencilState;

	D3D12_PRIMITIVE_TOPOLOGY_TYPE TopologyType;

	DXGI_FORMAT RTVFormat;
	DXGI_FORMAT DSVFormat;
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
		PipelineAndRootSig CreatePSO(const ShaderDesc& VSShader, const ShaderDesc& PSShader,
									 D3D12_ROOT_PARAMETER* Params, UINT NumParams, 
									 D3D12_STATIC_SAMPLER_DESC* Samplers, UINT NumSamplers,
									 D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc);
	private:
		std::unique_ptr<DuckyCompiler> mCompiler;
		ID3D12Device* mDevicePtr;
		std::wofstream* mFilePtr;
};