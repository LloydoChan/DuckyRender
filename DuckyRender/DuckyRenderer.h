#pragma once
#include "DuckyPipelineManager.h"
#include "DuckyGraphicsContext.h"

#ifdef OPAQUE
#undef OPAQUE
#endif

enum class PipelineType : uint32_t
{
	OPAQUE,
	ALPHA,
	MASKED,
	OPAQUE_DBL,
	ALPHA_DBL,
	MASKED_DBL,
	COUNT
};

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