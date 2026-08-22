#pragma once
#include "DuckyPipelineManager.h"
#include "DuckyGraphicsContext.h"
#include "DuckyRenderTypes.h"

class DuckyRenderer
{
	public:
		bool Init(D3DDeviceManager* Manager, GraphicsPipelineDesc* Pipelines, UINT NumPipelines);

		void ExecuteDraws(ID3D12GraphicsCommandList* list,
				PipelineType type,
				ID3D12Resource* indirectBuffer,
				UINT drawCount,
				UINT64 offset,
			    ID3D12Resource* DrawCountBuffer);

		const PipelineAndRootSig& GetPipelineSig(PipelineType Type) { return mPipelines[(int)Type]; }
		ID3D12CommandSignature* GetCommandSig() { return mDrawCommandSignature.Get(); }
		DuckyPipelineManager* GetPipelineManager() { return &mPipelineManager; }
	private:
		std::array<PipelineAndRootSig, static_cast<size_t>(PipelineType::COUNT)> mPipelines;

		ComPtr<ID3D12CommandSignature> mDrawCommandSignature;
		DuckyGraphicsContext mGraphicsContext;
		DuckyPipelineManager mPipelineManager;
};