#include "DuckyMesh.h"
#include "D3DDeviceManager.h"



bool DuckyMeshData::DuckyPrimitive::InitPrimitive(D3DDeviceManager* DeviceManager,
								   BufferInfo& VertexBufferInfo,
								   BufferInfo& IndexBufferInfo,
								   size_t TextureHash,
								   size_t NormalHash,
								   size_t MetallicHash,	
								   PBRValues& pbrValues)
{
	if(!InitBuffer(DeviceManager, VertexBufferInfo, mVertices.resource)) return false;
	vertexView.BufferLocation = mVertices.resource->GetGPUVirtualAddress();
	vertexView.SizeInBytes = VertexBufferInfo.BufferSize;
	vertexView.StrideInBytes = VertexBufferInfo.Stride;

	if(!InitBuffer(DeviceManager, IndexBufferInfo, mIndices.resource)) return false;
	indexView.BufferLocation = mIndices.resource->GetGPUVirtualAddress();


	switch (IndexBufferInfo.Stride)
	{
		case 2:
			indexView.Format = DXGI_FORMAT_R16_UINT;
			break;
		case 4:
			indexView.Format = DXGI_FORMAT_R32_UINT;
			break;
		default:
			return false;
	}

	indexView.SizeInBytes = IndexBufferInfo.BufferSize;

	if (VertexBufferInfo.Stride == 0 || IndexBufferInfo.Stride == 0) return false;

	mNumVertices = VertexBufferInfo.BufferSize / VertexBufferInfo.Stride;
	mNumIndices = IndexBufferInfo.BufferSize / IndexBufferInfo.Stride;

	mHashedTextureName = TextureHash;
	mHashedNormalMapName = NormalHash;
	mHashedMetallicRoughnessName = MetallicHash;

	mMaterialValues = pbrValues;

	return true;
}

bool DuckyMeshData::DuckyPrimitive::InitBuffer(D3DDeviceManager* DeviceManager, BufferInfo& BufferInfo, ComPtr<ID3D12Resource>& Data)
{
	Data = DeviceManager->CreateBuffer(BufferInfo.Data.size());
	if (!Data) return false;

	void* map = nullptr;

	HRESULT hResult = Data->Map(0, nullptr, &map);
	if (FAILED(hResult)) return false;

	std::memcpy(map, BufferInfo.Data.data(), BufferInfo.Data.size());
	Data->Unmap(0, nullptr);

	return true;
}


bool DuckyMeshData::Init(D3DDeviceManager* DeviceManager, std::vector<BufferInfo>& VertexInfos, std::vector<BufferInfo>& IndexInfos, std::vector<size_t>& TextureHashes, std::vector<PBRValues>& pbrValues)
{
	if (VertexInfos.size() != IndexInfos.size() || VertexInfos.size() != TextureHashes.size()/3) return false;

	for (int i = 0; i < VertexInfos.size(); i++)
	{
		DuckyPrimitive newPrim;
		if(!newPrim.InitPrimitive(DeviceManager, VertexInfos[i], IndexInfos[i], TextureHashes[i*3], TextureHashes[i*3+1], TextureHashes[i * 3 + 2], pbrValues[i])) return false;
		// need move or new Prim will go out of scope if reference, and we don't want to copy!
		AddPrimitive(std::move(newPrim));
	}

	return true;
}

bool DuckyMeshData::Init(D3DDeviceManager* DeviceManager, std::ifstream& InFile, UINT DescriptorHeapHandle)
{
	size_t numPrimitives = 0;
	InFile.read((char*)&numPrimitives, sizeof(size_t));

	std::vector<BufferInfo> vertices;
	std::vector<BufferInfo> indices;
	std::vector<size_t> textureHashes;
	std::vector<PBRValues> pbrValues;

	for (size_t i = 0; i < numPrimitives; i++)
	{
		// only two textures - albedo normal, metallic roughness supported right now
		for (int tex = 0; tex < 3; tex++)
		{
			//check for null texture
			int checkVal = 0;
			InFile.read((char*)&checkVal, sizeof(int));
			if (checkVal != -1)
			{
				size_t textureNameLength = 0;
				InFile.read((char*)&textureNameLength, sizeof(size_t));
				std::string textureName(textureNameLength, ' ');
				InFile.read((char*)&textureName[0], textureNameLength);
				std::wstring wideVersion(textureName.begin(), textureName.end());

				// read texture file
				size_t hashIndex = DeviceManager->InitTexture(wideVersion.c_str(), DescriptorHeapHandle);
				if (hashIndex != INVALID_HANDLE) textureHashes.push_back(hashIndex);
			}
			else
			{
				textureHashes.push_back(INVALID_HANDLE);
			}
		}

		// pbr values
		PBRValues newValues;
		InFile.read((char*)&newValues.roughness, sizeof(float));
		InFile.read((char*)&newValues.metal, sizeof(float));
		pbrValues.push_back(newValues);

		// vertex and info data
		BufferInfo vertBufferInfo;
		InFile.read((char*)&vertBufferInfo.Stride, sizeof(size_t));
		InFile.read((char*)&vertBufferInfo.BufferSize, sizeof(size_t));
		vertBufferInfo.Data.resize(vertBufferInfo.BufferSize);
		InFile.read(reinterpret_cast<char*>(vertBufferInfo.Data.data()),vertBufferInfo.BufferSize);

		BufferInfo indexBufferInfo;
		InFile.read((char*)&indexBufferInfo.Stride, sizeof(unsigned int));
		InFile.read((char*)&indexBufferInfo.BufferSize, sizeof(size_t));
		indexBufferInfo.Data.resize(indexBufferInfo.BufferSize);
		InFile.read(reinterpret_cast<char*>(indexBufferInfo.Data.data()), indexBufferInfo.BufferSize);

		vertices.emplace_back(std::move(vertBufferInfo));
		indices.emplace_back(std::move(indexBufferInfo));
	}

	return Init(DeviceManager, vertices, indices, textureHashes, pbrValues);
}

void DuckyMeshData::DrawMesh(ID3D12GraphicsCommandList* mCmdList, D3DDeviceManager* DeviceManager)
{
	for (const DuckyPrimitive& prim : mPrimitives)
	{
		DescriptorHeapResource* texture = DeviceManager->GetTexture(prim.mHashedTextureName);
		if (texture == nullptr) continue;
		mCmdList->SetGraphicsRootDescriptorTable(2, texture->descHandle);
		mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		mCmdList->IASetVertexBuffers(0, 1, &prim.vertexView);
		mCmdList->IASetIndexBuffer(&prim.indexView);
		mCmdList->DrawIndexedInstanced(prim.mNumIndices, 1, 0, 0, 0);
	}
}