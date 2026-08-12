#pragma once
#include "DuckyCompiler.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

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
		PipelineAndRootSig CreatePSO(LPCWSTR vertexShader, LPCWSTR vertexEntry, LPCWSTR pixelShader, LPCWSTR pixelEntry, RootSignatureDesc& NewRootSigDesc, D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc);
	private:
		std::unique_ptr<DuckyCompiler> mCompiler;
		ID3D12Device* mDevicePtr;
		std::wofstream* mFilePtr;
};