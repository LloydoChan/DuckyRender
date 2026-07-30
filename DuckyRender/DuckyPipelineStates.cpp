#include "DuckyPipelineStates.h"

D3D12_GRAPHICS_PIPELINE_STATE_DESC MakeOpaquePipelineState()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC result{};

    D3D12_RENDER_TARGET_BLEND_DESC renderTarget{};

    renderTarget.BlendEnable = FALSE;
    renderTarget.LogicOpEnable = FALSE;
    renderTarget.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;


    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};

    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc =D3D12_COMPARISON_FUNC_LESS;
    depthStencilDesc.StencilEnable = FALSE;

    result.DepthStencilState = depthStencilDesc;
    result.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    D3D12_RASTERIZER_DESC rasterizerDesc{};

    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.MultisampleEnable = false;

    result.BlendState.RenderTarget[0]        = renderTarget;
    result.BlendState.AlphaToCoverageEnable  = false;
    result.BlendState.IndependentBlendEnable = false;

    result.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    result.RasterizerState = rasterizerDesc;

    return result;
}

D3D12_GRAPHICS_PIPELINE_STATE_DESC MakeMaskedPipelineState()
{
    return D3D12_GRAPHICS_PIPELINE_STATE_DESC();
}

D3D12_GRAPHICS_PIPELINE_STATE_DESC MakeTransparentPipelineState()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC newState = MakeOpaquePipelineState();

    D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};
    renderTargetBlendDesc.BlendEnable = TRUE;
    renderTargetBlendDesc.LogicOpEnable = FALSE;

    renderTargetBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    renderTargetBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    renderTargetBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;

    renderTargetBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    renderTargetBlendDesc.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    renderTargetBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;

    renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};

    newState.DepthStencilState.DepthEnable    = FALSE;
    newState.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    newState.DepthStencilState.StencilEnable  = FALSE;

    newState.BlendState.RenderTarget[0] = renderTargetBlendDesc;

    return newState;
}
