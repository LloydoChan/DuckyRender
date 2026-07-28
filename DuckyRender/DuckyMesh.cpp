#include "DuckyMesh.h"
#include "D3DDeviceManager.h"



bool DuckyPrimitive::InitPrimitive(D3DDeviceManager* DeviceManager,
								   BufferInfo& VertexBufferInfo,
								   BufferInfo& IndexBufferInfo,
								   UINT MaterialIndex)
{
	if(!InitBuffer(DeviceManager, VertexBufferInfo, mVertices.resource)) return false;
	mVertexView.BufferLocation = mVertices.resource->GetGPUVirtualAddress();
	mVertexView.SizeInBytes = VertexBufferInfo.BufferSize;
	mVertexView.StrideInBytes = VertexBufferInfo.Stride;

	if(!InitBuffer(DeviceManager, IndexBufferInfo, mIndices.resource)) return false;
	mIndexView.BufferLocation = mIndices.resource->GetGPUVirtualAddress();


	switch (IndexBufferInfo.Stride)
	{
		case 2:
			mIndexView.Format = DXGI_FORMAT_R16_UINT;
			break;
		case 4:
			mIndexView.Format = DXGI_FORMAT_R32_UINT;
			break;
		default:
			return false;
	}

	mIndexView.SizeInBytes = IndexBufferInfo.BufferSize;

	if (VertexBufferInfo.Stride == 0 || IndexBufferInfo.Stride == 0) return false;

	mNumVertices = VertexBufferInfo.BufferSize / VertexBufferInfo.Stride;
	mNumIndices = IndexBufferInfo.BufferSize / IndexBufferInfo.Stride;

	mMaterialIndex = MaterialIndex;

	return true;
}

bool DuckyPrimitive::InitBuffer(D3DDeviceManager* DeviceManager, BufferInfo& BufferInfo, ComPtr<ID3D12Resource>& Data)
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

void DuckyPrimitive::BindGeometry(ID3D12GraphicsCommandList* commandList) const
{
	commandList->IASetVertexBuffers(0, 1, &mVertexView);
	commandList->IASetIndexBuffer(&mIndexView);
}

void DuckyPrimitive::Draw(ID3D12GraphicsCommandList* commandList) const
{
	commandList->DrawIndexedInstanced(mNumIndices, 1, 0, 0, 0);
}


bool DuckyMeshData::Init(D3DDeviceManager* DeviceManager, std::ifstream& InFile, UINT DescriptorHeapHandle)
{
	if (DeviceManager == nullptr || !InFile) return false;

	size_t numPrimitives = 0;

	InFile.read(reinterpret_cast<char*>(&numPrimitives), sizeof(numPrimitives));

	if (!InFile) return false;

	mPrimitives.clear();
	mMaterials.clear();

	mPrimitives.reserve(numPrimitives);
	mMaterials.reserve(numPrimitives);

	for (size_t primitiveIndex = 0; primitiveIndex < numPrimitives; ++primitiveIndex)
	{
		PrimitiveLoadData loadData;

		if (!ReadPrimitive(DeviceManager,InFile, DescriptorHeapHandle, loadData)) return false;

		const uint32_t materialIndex = static_cast<uint32_t>(mMaterials.size());

		mMaterials.emplace_back(std::move(loadData.material));

		DuckyPrimitive primitive;

		if (!primitive.InitPrimitive(DeviceManager, loadData.vertexBuffer, loadData.indexBuffer, materialIndex)) return false;

		mPrimitives.emplace_back(std::move(primitive));
	}

	return true;
}

bool DuckyMeshData::ReadPrimitive(D3DDeviceManager* deviceManager, std::ifstream& inputFile, UINT descriptorHeapHandle, PrimitiveLoadData& output)
{
	output.material.mBaseColorTexture		    = ReadTexture(deviceManager,inputFile,descriptorHeapHandle);
	output.material.mNormalTexture				= ReadTexture(deviceManager,inputFile,descriptorHeapHandle);
	output.material.mMetallicRoughnessTexture   = ReadTexture(deviceManager,inputFile,descriptorHeapHandle);

	inputFile.read(reinterpret_cast<char*>(&output.material.constants.mRoughnessFactor),sizeof(float));
	inputFile.read(reinterpret_cast<char*>(&output.material.constants.mMetallicFactor),sizeof(float));

	if (!inputFile) return false;

	output.material.constants.mBaseColorTexture			= output.material.mBaseColorTexture != INVALID_HANDLE;
	output.material.constants.mNormalTexture			= output.material.mNormalTexture != INVALID_HANDLE;
	output.material.constants.mMetallicRoughnessTexture = output.material.mMetallicRoughnessTexture != INVALID_HANDLE;

	if (!ReadBufferInfo(inputFile, output.vertexBuffer))return false;
	if (!ReadBufferInfo(inputFile,output.indexBuffer))return false;

	return true;
}

size_t DuckyMeshData::ReadTexture(D3DDeviceManager* deviceManager, std::ifstream& inputFile, UINT descriptorHeapHandle)
{
	int textureCheck = -1;

	inputFile.read(reinterpret_cast<char*>(&textureCheck), sizeof(textureCheck));

	if (!inputFile) return INVALID_HANDLE;

	if (textureCheck == -1) return INVALID_HANDLE;

	size_t textureNameLength = 0;

	inputFile.read(reinterpret_cast<char*>(&textureNameLength), sizeof(textureNameLength));

	if (!inputFile || textureNameLength == 0) return INVALID_HANDLE;

	std::string textureName(textureNameLength,'\0');

	inputFile.read(textureName.data(),static_cast<std::streamsize>(textureNameLength));

	if (!inputFile) return INVALID_HANDLE;

	const std::wstring wideTextureName(textureName.begin(), textureName.end());

	return deviceManager->InitTexture(wideTextureName.c_str(), descriptorHeapHandle);
}

bool DuckyMeshData::ReadBufferInfo(std::ifstream& inputFile, BufferInfo& output)
{
	inputFile.read(reinterpret_cast<char*>(&output.Stride), sizeof(output.Stride));
	inputFile.read(reinterpret_cast<char*>(&output.BufferSize), sizeof(output.BufferSize));

	if (!inputFile || output.Stride == 0 || output.BufferSize == 0) return false;

	output.Data.resize(output.BufferSize);

	inputFile.read(reinterpret_cast<char*>(output.Data.data()),static_cast<std::streamsize>(output.BufferSize));

	return static_cast<bool>(inputFile);
}