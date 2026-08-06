#include "pch.h"
#include "DuckyMesh.h"
#include "D3DDeviceManager.h"

void DuckyPrimitive::Draw(ID3D12GraphicsCommandList* commandList) const
{
	commandList->DrawIndexedInstanced(mNumIndices, 1, mIndexOffset, mVertexOffset, 0);
}


AABB::AABB()
{
	mVertices[AABB_MIN] = {(std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)() , (std::numeric_limits<float>::max)() , 1.f};
	mVertices[AABB_MAX] = {(std::numeric_limits<float>::lowest)(), (std::numeric_limits<float>::lowest)() , (std::numeric_limits<float>::lowest)() , 1.f};
}

AABB::AABB(const XMVECTOR& Min, const XMVECTOR& Max)
{
	mVertices[AABB_MIN] = Min;
	mVertices[AABB_MAX] = Max;

	RegenerateBox(Min, Max);
}

void AABB::RegenerateBox(const XMVECTOR& Min, const XMVECTOR& Max)
{
	mVertices[2] = { XMVectorGetX(Max), XMVectorGetY(Max), XMVectorGetZ(Max), 1.f};
	mVertices[3] = { XMVectorGetX(Min), XMVectorGetY(Min), XMVectorGetZ(Max), 1.f };
	mVertices[4] = { XMVectorGetX(Max), XMVectorGetY(Min), XMVectorGetZ(Min), 1.f };
	mVertices[5] = { XMVectorGetX(Max), XMVectorGetY(Max), XMVectorGetZ(Min), 1.f };
	mVertices[6] = { XMVectorGetX(Min), XMVectorGetY(Max), XMVectorGetZ(Min), 1.f };
	mVertices[7] = { XMVectorGetX(Max), XMVectorGetY(Min), XMVectorGetZ(Max), 1.f };
}
