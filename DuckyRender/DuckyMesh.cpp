#include "DuckyMesh.h"
#include "D3DDeviceManager.h"



bool DuckyPrimitive::InitPrimitive(D3DDeviceManager* DeviceManager,
								   PrimitiveLoadData& LoadData,
								   UINT MaterialIndex)
{
	if(!InitBuffer(DeviceManager, LoadData.vertexBuffer, mVertices.resource)) return false;
	mVertexView.BufferLocation = mVertices.resource->GetGPUVirtualAddress();
	mVertexView.SizeInBytes = LoadData.vertexBuffer.BufferSize;
	mVertexView.StrideInBytes = LoadData.vertexBuffer.Stride;

	if(!InitBuffer(DeviceManager, LoadData.indexBuffer, mIndices.resource)) return false;
	mIndexView.BufferLocation = mIndices.resource->GetGPUVirtualAddress();


	switch (LoadData.indexBuffer.Stride)
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

	mIndexView.SizeInBytes = LoadData.indexBuffer.BufferSize;

	if (LoadData.vertexBuffer.Stride == 0 || LoadData.indexBuffer.Stride == 0) return false;

	mNumVertices = LoadData.vertexBuffer.BufferSize / LoadData.vertexBuffer.Stride;
	mNumIndices = LoadData.indexBuffer.BufferSize / LoadData.indexBuffer.Stride;

	mMaterialIndex = MaterialIndex;

	mPrimitiveAABB = LoadData.boundingBox;

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

		if (!primitive.InitPrimitive(DeviceManager, loadData, materialIndex)) return false;

		mPrimitives.emplace_back(std::move(primitive));
	}

	return true;
}

bool DuckyMeshData::ReadPrimitive(D3DDeviceManager* deviceManager, std::ifstream& inputFile, UINT descriptorHeapHandle, PrimitiveLoadData& output)
{
	output.material.mBaseColorTexture		    = ReadTexture(deviceManager,inputFile,descriptorHeapHandle);
	output.material.mNormalTexture				= ReadTexture(deviceManager,inputFile,descriptorHeapHandle);
	output.material.mMetallicRoughnessTexture   = ReadTexture(deviceManager,inputFile,descriptorHeapHandle);
	output.material.mEmissive					= ReadTexture(deviceManager,inputFile,descriptorHeapHandle);

	inputFile.read(reinterpret_cast<char*>(&output.material.constants.mBaseColorFactor), sizeof(float) * 4);
	inputFile.read(reinterpret_cast<char*>(&output.material.constants.mNormalScale), sizeof(float));
	inputFile.read(reinterpret_cast<char*>(&output.material.constants.mRoughnessFactor),sizeof(float));
	inputFile.read(reinterpret_cast<char*>(&output.material.constants.mMetallicFactor),sizeof(float));

	inputFile.read(reinterpret_cast<char*>(&output.material.constants.alphaMode), sizeof(unsigned int));
	inputFile.read(reinterpret_cast<char*>(&output.material.constants.alphaCutoff), sizeof(float));
	inputFile.read(reinterpret_cast<char*>(&output.material.constants.doubleSided), sizeof(unsigned int));

	if (!inputFile) return false;

	output.material.constants.mHasBaseColorTexture				= output.material.mBaseColorTexture != NO_TEXTURE;
	output.material.constants.mHasNormalTexture					= output.material.mNormalTexture != NO_TEXTURE;
	output.material.constants.mHasMetallicRoughnessTexture		= output.material.mMetallicRoughnessTexture != NO_TEXTURE;
	output.material.constants.mHasEmissiveTexture			    = output.material.mEmissive != NO_TEXTURE;


	if (!ReadBufferInfo(inputFile, output.vertexBuffer))return false;

	// read AABB data
	XMFLOAT4 min{};
	XMFLOAT4 max{};
	inputFile.read(reinterpret_cast<char*>(&min), sizeof(XMFLOAT4));
	inputFile.read(reinterpret_cast<char*>(&max), sizeof(XMFLOAT4));

	output.boundingBox.SetMin(min);
	output.boundingBox.SetMax(max);

	if (!ReadBufferInfo(inputFile,output.indexBuffer))return false;

	return true;
}

size_t DuckyMeshData::ReadTexture(D3DDeviceManager* deviceManager, std::ifstream& inputFile, UINT descriptorHeapHandle)
{
	int textureCheck = -1;

	inputFile.read(reinterpret_cast<char*>(&textureCheck), sizeof(textureCheck));

	if (!inputFile) return INVALID_HANDLE;

	if (textureCheck == -1) return NO_TEXTURE;

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

AABB::AABB(const XMFLOAT4& Min, const XMFLOAT4& Max)
{
	mVertices[AABB_MIN] = Min;
	mVertices[AABB_MAX] = Max;

	mVertices[2] = {Min.x, Max.y, Max.z, 1.f};
	mVertices[3] = {Min.x, Min.y, Max.z, 1.f};
	mVertices[4] = {Max.x, Min.y, Min.z, 1.f};
	mVertices[5] = {Max.x, Max.y, Min.z, 1.f};
	mVertices[6] = {Min.x, Max.y, Min.z, 1.f};
	mVertices[7] = {Max.x, Min.y, Max.z, 1.f};
}
