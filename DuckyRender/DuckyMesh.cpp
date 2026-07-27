#include "DuckyMesh.h"
#include "D3DDeviceManager.h"



bool DuckyMeshData::DuckyPrimitive::InitPrimitive(D3DDeviceManager* DeviceManager,
								   BufferInfo& VertexBufferInfo,
								   BufferInfo& IndexBufferInfo,
								   size_t TextureHash)
{
	InitBuffer(DeviceManager, VertexBufferInfo, &mVertices.resource);
	vertexView.BufferLocation = mVertices.resource->GetGPUVirtualAddress();
	vertexView.SizeInBytes = VertexBufferInfo.BufferSize;
	vertexView.StrideInBytes = VertexBufferInfo.Stride;

	InitBuffer(DeviceManager, IndexBufferInfo, &mIndices.resource);
	indexView.BufferLocation = mIndices.resource->GetGPUVirtualAddress();

	if (IndexBufferInfo.Stride == 1) indexView.Format = DXGI_FORMAT_R8_UINT;
	else if (IndexBufferInfo.Stride == 2) indexView.Format = DXGI_FORMAT_R16_UINT;
	else indexView.Format = DXGI_FORMAT_R32_UINT; // assume 4 bytes!

	indexView.SizeInBytes = IndexBufferInfo.BufferSize;

	mNumVertices = VertexBufferInfo.BufferSize / VertexBufferInfo.Stride;
	mNumIndices = IndexBufferInfo.BufferSize / IndexBufferInfo.Stride;

	mHashedTextureName = TextureHash;

	return true;
}

bool DuckyMeshData::DuckyPrimitive::InitBuffer(D3DDeviceManager* DeviceManager, BufferInfo& BufferInfo, ID3D12Resource** Data)
{
	*Data = DeviceManager->CreateBuffer(BufferInfo.Data.size());
	if (*Data == nullptr) return false;

	void* map = nullptr;
	HRESULT hResult = (*Data)->Map(0, nullptr, &map);
	std::memcpy(map, BufferInfo.Data.data(), BufferInfo.Data.size());
	(*Data)->Unmap(0, nullptr);

	return true;
}


bool DuckyMeshData::Init(D3DDeviceManager* DeviceManager, std::vector<BufferInfo>& VertexInfos, std::vector<BufferInfo>& IndexInfos, std::vector<size_t>& TextureHashes)
{

	if (VertexInfos.size() != IndexInfos.size()) return false;

	for (int i = 0; i < VertexInfos.size(); i++)
	{
		DuckyPrimitive newPrim;
		newPrim.InitPrimitive(DeviceManager, VertexInfos[i], IndexInfos[i], TextureHashes[i]);
		// need move or new Prim will go out of scope if reference, and we don't want to copy!
		AddPrimitive(std::move(newPrim));
	}

	return true;
}

bool DuckyMeshData::Init(D3DDeviceManager* DeviceManager, std::ifstream& InFile, UINT DescriptorHeapHandle)
{
	size_t numPrimitives = 0;
	InFile.read((char*)&numPrimitives, sizeof(size_t));

	XMMATRIX transform;
	InFile.read((char*)&transform, sizeof(float) * 16);

	std::vector<BufferInfo> vertices;
	std::vector<BufferInfo> indices;
	std::vector<size_t> textureHashes;

	for (size_t i = 0; i < numPrimitives; i++)
	{
		size_t albedoNameLength = 0;
		InFile.read((char*)&albedoNameLength, sizeof(size_t));
		std::string albedoName(albedoNameLength, ' ');
		InFile.read((char*)&albedoName[0], albedoNameLength);
		std::wstring wideVersion(albedoName.begin(), albedoName.end());

		// read texture file
		size_t hashIndex = DeviceManager->InitTexture(wideVersion.c_str(), DescriptorHeapHandle);
		textureHashes.push_back(hashIndex);

		BufferInfo vertBufferInfo;
		vertBufferInfo.Stride = 20;
		InFile.read((char*)&vertBufferInfo.BufferSize, sizeof(size_t));
		vertBufferInfo.Data.resize(vertBufferInfo.BufferSize);
		InFile.read(reinterpret_cast<char*>(vertBufferInfo.Data.data()),vertBufferInfo.BufferSize);

		BufferInfo indexBufferInfo;
		indexBufferInfo.Stride = 4;
		InFile.read((char*)&indexBufferInfo.BufferSize, sizeof(size_t));
		indexBufferInfo.Data.resize(indexBufferInfo.BufferSize);
		InFile.read(reinterpret_cast<char*>(indexBufferInfo.Data.data()), indexBufferInfo.BufferSize);

		vertices.emplace_back(std::move(vertBufferInfo));
		indices.emplace_back(std::move(indexBufferInfo));
	}

	return Init(DeviceManager, vertices, indices, textureHashes);
}

void DuckyMeshData::DrawMesh(ID3D12GraphicsCommandList* mCmdList, D3DDeviceManager* DeviceManager)
{
	for (const DuckyPrimitive& prim : mPrimitives)
	{
		DescriptorHeapResource texture = DeviceManager->GetTexture(prim.mHashedTextureName);
		mCmdList->SetGraphicsRootDescriptorTable(1, texture.descHandle);
		mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		mCmdList->IASetVertexBuffers(0, 1, &prim.vertexView);
		mCmdList->IASetIndexBuffer(&prim.indexView);
		mCmdList->DrawIndexedInstanced(prim.mNumIndices, 1, 0, 0, 0);
	}
}