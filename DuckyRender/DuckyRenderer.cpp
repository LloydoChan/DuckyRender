#include "pch.h"
#include "DuckyRenderer.h"

namespace RootParameter
{
	constexpr UINT PerFrame = 0;
	constexpr UINT DrawConstants = 1;
	constexpr UINT Count = 2;
};

bool DuckyRenderer::Init(D3DDeviceManager* Manager, GraphicsPipelineDesc* Pipelines, UINT NumPipelines)
{
    if (!mPipelineManager.Init(Manager->GetDevice())) return false;

    for (int i = 0; i < NumPipelines; i++)
    {
		PipelineType type = Pipelines[i].Type;
		int index = static_cast<int>(type);
        mPipelines[index] = mPipelineManager.CreatePSO(Pipelines[i]);
        if (mPipelines[index].rootSig == nullptr || mPipelines[index].pipeLineState == nullptr) return false;
    }

	D3D12_INDIRECT_ARGUMENT_DESC indirectDescs[2]{};
	indirectDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
	indirectDescs[0].Constant.RootParameterIndex = RootParameter::DrawConstants;
	indirectDescs[0].Constant.DestOffsetIn32BitValues = 0;
	indirectDescs[0].Constant.Num32BitValuesToSet = 1;

	indirectDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

	D3D12_COMMAND_SIGNATURE_DESC signatureDesc{};
	signatureDesc.NumArgumentDescs = 2;
	signatureDesc.pArgumentDescs = indirectDescs;
	signatureDesc.ByteStride = sizeof(IndirectCommand);

	HRESULT hResult = Manager->GetDevice()->CreateCommandSignature(&signatureDesc, mPipelines[(int)PipelineType::OPAQUE].rootSig.Get(), IID_PPV_ARGS(&mDrawCommandSignature));
	if (FAILED(hResult)) return false;

    return true;
}

void DuckyRenderer::ExecuteDraws(ID3D12GraphicsCommandList* list, 
								 PipelineType type, 
								 ID3D12Resource* indirectBuffer, 
								 UINT drawCount, 
								 UINT64 offset, 
								 ID3D12Resource* DrawCountBuffer)
{
	const auto& pipeline = mPipelines[static_cast<size_t>(type)];

	list->SetPipelineState(pipeline.pipeLineState.Get());

	list->ExecuteIndirect(mDrawCommandSignature.Get(),
						  drawCount,
						  indirectBuffer,
						  offset,
						  DrawCountBuffer,
						  UINT64(type) * sizeof(uint32_t));
}
