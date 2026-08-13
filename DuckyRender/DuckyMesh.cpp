#include "pch.h"
#include "DuckyMesh.h"
#include "D3DDeviceManager.h"

void DuckyPrimitive::Draw(ID3D12GraphicsCommandList* commandList) const
{
	commandList->DrawIndexedInstanced(mNumIndices, 1, mIndexOffset, mVertexOffset, 0);
}


AABB::AABB()
{
	mVertices[AABB_MAX] = {(std::numeric_limits<float>::lowest)(), (std::numeric_limits<float>::lowest)(), (std::numeric_limits<float>::lowest)(), 1.f};
	mVertices[AABB_MIN] = {(std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)(), 1.f};
	RegenerateBox();
}


void AABB::RegenerateBox()
{
	mVertices[2] = { mVertices[AABB_MIN].x, mVertices[AABB_MIN].y, mVertices[AABB_MAX].z, 1.f };
	mVertices[3] = { mVertices[AABB_MIN].x, mVertices[AABB_MAX].y, mVertices[AABB_MAX].z, 1.f };
	mVertices[4] = { mVertices[AABB_MAX].x, mVertices[AABB_MAX].y, mVertices[AABB_MIN].z, 1.f };
	mVertices[5] = { mVertices[AABB_MAX].x, mVertices[AABB_MIN].y, mVertices[AABB_MIN].z, 1.f };
	mVertices[6] = { mVertices[AABB_MAX].x, mVertices[AABB_MIN].y, mVertices[AABB_MAX].z, 1.f };
	mVertices[7] = { mVertices[AABB_MIN].x, mVertices[AABB_MAX].y, mVertices[AABB_MIN].z, 1.f };
}
