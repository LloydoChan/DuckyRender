#pragma once
#include "DuckyPipelineManager.h"
#include "DuckyGraphicsContext.h"
#include "DuckyRenderTypes.h"

class DuckyRenderer
{
	public:
		bool Init();
		void Render();
	private:
		std::array<PipelineAndRootSig, static_cast<size_t>(PipelineType::COUNT)> mPipelines;

		ComPtr<ID3D12CommandSignature> mDrawCommandSignature;
		DuckyGraphicsContext mGraphicsContext;
};