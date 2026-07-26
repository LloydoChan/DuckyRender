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

	if (IndexBufferInfo.Stride == 1) mIndices.ibView.Format = DXGI_FORMAT_R8_UINT;
	else if (IndexBufferInfo.Stride == 2) mIndices.ibView.Format = DXGI_FORMAT_R16_UINT;
	else mIndices.ibView.Format = DXGI_FORMAT_R32_UINT; // assume 4 bytes!

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


bool DuckyMesh::Init(D3DDeviceManager* DeviceManager, std::vector<BufferInfo>& VertexInfos, std::vector<BufferInfo>& IndexInfos, XMMATRIX& transform)
{
	mTransform = transform;

	if (VertexInfos.size() != IndexInfos.size()) return false;

	for (int i = 0; i < VertexInfos.size(); i++)
	{
		DuckyPrimitive newPrim;
		newPrim.InitPrimitive(DeviceManager, VertexInfos[i], IndexInfos[i]);
		// need move or new Prim will go out of scope if reference, and we don't want to copy!
		AddPrimitive(std::move(newPrim));
	}
}

bool DuckyMesh::Init(D3DDeviceManager* DeviceManager, std::ifstream& InFile)
{
	size_t numPrimitives = 0;
	InFile.read((char*)&numPrimitives, sizeof(size_t));

	XMMATRIX transform;
	InFile.read((char*)&transform, sizeof(float) * 16);

	std::vector<BufferInfo> vertices;
	std::vector<BufferInfo> indices;

	for (size_t i = 0; i < numPrimitives; i++)
	{
		BufferInfo vertBufferInfo;
		vertBufferInfo.Stride = 12;
		InFile.read((char*)&vertBufferInfo.BufferSize, sizeof(size_t));
		vertBufferInfo.Data = new unsigned char[vertBufferInfo.BufferSize];
		InFile.read((char*)vertBufferInfo.Data, vertBufferInfo.BufferSize);
		
		BufferInfo indexBufferInfo;
		indexBufferInfo.Stride = 4;
		InFile.read((char*)&indexBufferInfo.BufferSize, sizeof(size_t));
		indexBufferInfo.Data = new unsigned char[indexBufferInfo.BufferSize];
		InFile.read((char*)indexBufferInfo.Data, indexBufferInfo.BufferSize);

		vertices.emplace_back(std::move(vertBufferInfo));
		indices.emplace_back(std::move(indexBufferInfo));
	}

	return Init(DeviceManager, vertices, indices, transform);
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
