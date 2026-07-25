#include "DuckyMesh.h"
#include "D3DDeviceManager.h"

bool DuckyMesh::DuckyPrimitive::InitPrimitive(D3DDeviceManager* DeviceManager,
								   BufferInfo& VertexBufferInfo,
								   BufferInfo& IndexBufferInfo)
{
	InitBuffer(DeviceManager, VertexBufferInfo, &mVertices.vertBuffPointer);
	mVertices.vbView.BufferLocation = mVertices.vertBuffPointer->GetGPUVirtualAddress();
	mVertices.vbView.SizeInBytes = VertexBufferInfo.BufferSize;
	mVertices.vbView.StrideInBytes = VertexBufferInfo.Stride;

	InitBuffer(DeviceManager, IndexBufferInfo, &mIndices.idxBuffPointer);
	mIndices.ibView.BufferLocation = mIndices.idxBuffPointer->GetGPUVirtualAddress();
	if (IndexBufferInfo.Stride == 2) mIndices.ibView.Format = DXGI_FORMAT_R16_UINT;;
	mIndices.ibView.SizeInBytes = IndexBufferInfo.BufferSize;

	mNumVertices = VertexBufferInfo.BufferSize / VertexBufferInfo.Stride;
	mNumIndices = IndexBufferInfo.BufferSize / IndexBufferInfo.Stride;
	return true;
}

bool DuckyMesh::DuckyPrimitive::InitBuffer(D3DDeviceManager* DeviceManager, BufferInfo& BufferInfo, ID3D12Resource** Data)
{
	*Data = DeviceManager->CreateBuffer(BufferInfo.BufferSize);
	if (Data == nullptr) return false;

	void* map = nullptr;
	HRESULT hResult = (*Data)->Map(0, nullptr, &map);
	std::memcpy(map, BufferInfo.Data, BufferInfo.BufferSize);
	(*Data)->Unmap(0, nullptr);
}


bool DuckyMesh::Init(D3DDeviceManager* DeviceManager, std::vector<BufferInfo>& VertexInfos, std::vector<BufferInfo>& IndexInfos)
{
	if (VertexInfos.size() != IndexInfos.size()) return false;

	for (int i = 0; i < VertexInfos.size(); i++)
	{
		DuckyPrimitive newPrim;
		newPrim.InitPrimitive(DeviceManager, VertexInfos[i], IndexInfos[i]);
		// need move or new Prim will go out of scope if reference, and we don't want to copy!
		AddPrimitive(std::move(newPrim));
	}
}

void DuckyMesh::DrawMesh(ID3D12GraphicsCommandList* mCmdList)
{
	for (const DuckyPrimitive& prim : mPrimitives)
	{
		mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		mCmdList->IASetVertexBuffers(0, 1, &prim.mVertices.vbView);
		mCmdList->IASetIndexBuffer(&prim.mIndices.ibView);
		mCmdList->DrawIndexedInstanced(prim.mNumIndices, 1, 0, 0, 0);
	}
}
