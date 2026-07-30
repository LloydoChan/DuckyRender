#pragma once
#include <d3d12.h>

D3D12_GRAPHICS_PIPELINE_STATE_DESC MakeOpaquePipelineState();
D3D12_GRAPHICS_PIPELINE_STATE_DESC MakeMaskedPipelineState();
D3D12_GRAPHICS_PIPELINE_STATE_DESC MakeTransparentPipelineState();