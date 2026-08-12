#include "pch.h"
#include "DuckyScene.h"
#include "DuckyRenderTypes.h"

bool DuckyScene::Init(std::ifstream& InFile, D3DDeviceManager* DeviceManager)
{
	InitInstanceData(InFile, DeviceManager);
	InitTextures(InFile, DeviceManager);
	InitMaterials(InFile, DeviceManager);
	InitMeshes(InFile, DeviceManager);
	InitVertexAndIndexMegaBuffer(InFile, DeviceManager);
    return true;
}

bool DuckyScene::InitInstanceData(std::ifstream& InFile, D3DDeviceManager* DeviceManager)
{
	size_t numInstances = 0;

	InFile.read(reinterpret_cast<char*>(&numInstances), sizeof(numInstances));

	if (!InFile) return false;

	for (size_t instanceIndex = 0; instanceIndex < numInstances; ++instanceIndex)
	{
		DuckyMeshInstance instance;

		InFile.read(reinterpret_cast<char*>(&instance.mMeshDataIndex), sizeof(instance.mMeshDataIndex));
		InFile.read(reinterpret_cast<char*>(&instance.mTransform), sizeof(instance.mTransform));

		if (!InFile) return false;

		mInstances.emplace_back(std::move(instance));
	}

	const size_t bufferSize = mInstances.size() * sizeof(GPUInstance);
	mGPUInstances = DeviceManager->CreateStructuredBuffer(bufferSize, sizeof(GPUInstance));

	HRESULT result = mGPUInstances.buffer->Map(0, nullptr, &mGPUInstances.mapped);

	if (FAILED(result)) return false;

	GPUInstance* dst = static_cast<GPUInstance*>(mGPUInstances.mapped);

	for (const DuckyMeshInstance& instance : mInstances)
	{
		XMMATRIX normal = XMMatrixInverse(nullptr, instance.mTransform);
		XMMATRIX world = XMMatrixTranspose(instance.mTransform);
		XMStoreFloat4x4(&dst->mWorld, world);
		XMStoreFloat4x4(&dst->mNormal, normal);
		++dst;
	}

	return true;
}

bool DuckyScene::InitMaterials(std::ifstream& InFile, D3DDeviceManager* DeviceManager)
{
	size_t numMaterials = 0;
	InFile.read((char*)&numMaterials, sizeof(size_t));

	// get a buffer for the materials and copy into mapped dest
	size_t bufferSize = numMaterials * sizeof(GPUMaterial);
	mGPUMaterials = DeviceManager->CreateStructuredBuffer(bufferSize, sizeof(GPUMaterial));

	HRESULT result = mGPUMaterials.buffer->Map(0, nullptr, &mGPUMaterials.mapped);
	GPUMaterial* dst = (GPUMaterial*)mGPUMaterials.mapped;

	if (!FAILED(result))
	{
		for (int i = 0; i < numMaterials; i++)
		{
			DuckyMaterial nextMaterial{};
			InFile.read((char*)&nextMaterial, sizeof(DuckyMaterial));

			GPUMaterial gpuFriendly{};

			gpuFriendly.alphaCutoff = nextMaterial.constants.alphaCutoff;
			gpuFriendly.alphaMode = static_cast<uint32_t>(nextMaterial.constants.alphaMode);
			gpuFriendly.doubleSided = nextMaterial.constants.doubleSided;
			gpuFriendly.RoughnessFactor = nextMaterial.constants.mRoughnessFactor;
			gpuFriendly.MetallicFactor = nextMaterial.constants.mMetallicFactor;
			gpuFriendly.NormalScale = nextMaterial.constants.mNormalScale;
			gpuFriendly.BaseColorFactor = nextMaterial.constants.mBaseColorFactor;

			if (nextMaterial.mBaseColorTexture != -1)
			{
				gpuFriendly.BaseColorTexture = mTextures[nextMaterial.mBaseColorTexture].heapOffset;
			}
			else
			{
				gpuFriendly.BaseColorTexture = mTextures[mBaseColorFallbackHandle].heapOffset;
			}

			if (nextMaterial.mNormalTexture != -1)
			{
				gpuFriendly.NormalTexture = mTextures[nextMaterial.mNormalTexture].heapOffset;
			}
			else
			{
				gpuFriendly.NormalTexture = mTextures[mNormalColorFallbackHandle].heapOffset;
			}

			if (nextMaterial.mMetallicRoughnessTexture != -1)
			{
				gpuFriendly.MetallicRoughnessTexture = mTextures[nextMaterial.mMetallicRoughnessTexture].heapOffset;
			}
			else
			{
				gpuFriendly.MetallicRoughnessTexture = mTextures[mMetallicRougnessFallbackHandle].heapOffset;
			}

			if (nextMaterial.mEmissive != -1)
			{
				gpuFriendly.EmissiveTexture = mTextures[nextMaterial.mEmissive].heapOffset;
			}
			else
			{
				gpuFriendly.EmissiveTexture = mTextures[mEmissiveColorFallbackHandle].heapOffset;
			}

			memcpy(dst, &gpuFriendly, sizeof(GPUMaterial));

			dst++;

			mMaterials.push_back(nextMaterial);
		}
	}
	else
	{
		return false;
	}

	return true;
}

bool DuckyScene::InitTextures(std::ifstream& InFile, D3DDeviceManager* DeviceManager)
{
	size_t numTextures = 0;
	InFile.read((char*)&numTextures, sizeof(size_t));

	for (size_t i = 0; i < numTextures; i++)
	{
		size_t textureNameLength = 0;
		InFile.read((char*)&textureNameLength, sizeof(size_t));
		std::string fileName(textureNameLength, '\0');

		if (textureNameLength == 0)
		{
			DescriptorHeapResource dummyResource;
			mTextures.emplace_back(dummyResource);
			continue;
		}

		InFile.read((char*)&fileName[0], textureNameLength);

		if (!InFile) return false;

		const std::wstring wideTextureName(fileName.begin(), fileName.end());

		DescriptorHeapResource newResource = DeviceManager->CreateTexture(wideTextureName.c_str());
		if (newResource.buffer == nullptr) return false;
		mTextures.emplace_back(newResource);
	}

	return true;

	// init fallback textures
	DescriptorHeapResource newResource = DeviceManager->CreateFallbackTexture(L"BaseColorFallbackTexture", BaseColorFallback, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	if (newResource.buffer == nullptr) return false;
	mTextures.emplace_back(newResource);
	mBaseColorFallbackHandle = mTextures.size() - 1;

	newResource = DeviceManager->CreateFallbackTexture(L"NormalFallbackTexture", NormalFallback, DXGI_FORMAT_R8G8B8A8_UNORM);
	if (newResource.buffer == nullptr) return false;
	mTextures.emplace_back(newResource);
	mNormalColorFallbackHandle = mTextures.size() - 1;

	newResource = DeviceManager->CreateFallbackTexture(L"EmissiveFallbackTexture", EmissiveFallback, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	if (newResource.buffer == nullptr) return false;
	mTextures.emplace_back(newResource);
	mEmissiveColorFallbackHandle = mTextures.size() - 1;

	newResource = DeviceManager->CreateFallbackTexture(L"MetallicRoughnessFallbackTexture", BaseColorFallback, DXGI_FORMAT_R8G8B8A8_UNORM);
	if (newResource.buffer == nullptr) return false;
	mTextures.emplace_back(newResource);
	mMetallicRougnessFallbackHandle = mTextures.size() - 1;

	return true;
}

bool DuckyScene::InitMeshes(std::ifstream& InFile, D3DDeviceManager* DeviceManager)
{
	int numMeshes = 0;
	InFile.read((char*)&numMeshes, sizeof(int));

	for (int i = 0; i < numMeshes; i++)
	{
		DuckyMeshData newMesh;
		int numPrims = 0;
		InFile.read((char*)&numPrims, sizeof(int));

		for (int j = 0; j < numPrims; j++)
		{
			int materialIndex = 0;
			size_t numberVertices = 0;
			size_t vertexOffset = 0;
			size_t numberIndices = 0;
			size_t indexOffset = 0;

			InFile.read((char*)&materialIndex, sizeof(int));

			InFile.read((char*)&numberVertices, sizeof(size_t));
			InFile.read((char*)&vertexOffset, sizeof(size_t));
			InFile.read((char*)&numberIndices, sizeof(size_t));
			InFile.read((char*)&indexOffset, sizeof(size_t));

			XMFLOAT4 min, max;
			InFile.read((char*)&min, sizeof(XMFLOAT4));
			InFile.read((char*)&max, sizeof(XMFLOAT4));

			AABB newBB(min, max);

			DuckyPrimitive newPrimitive(numberIndices, numberVertices, indexOffset, vertexOffset, materialIndex, newBB);
			newMesh.AddPrimitive(newPrimitive);
		}

		mMeshes.emplace_back(newMesh);
	}

	return true;
}

bool DuckyScene::InitVertexAndIndexMegaBuffer(std::ifstream& ModelFile, D3DDeviceManager* DeviceManager)
{
	size_t numVertices = 0;
	size_t numIndices = 0;

	for (const DuckyMeshData& mesh : mMeshes)
	{
		for (const DuckyPrimitive& primitive : mesh.GetPrimitives())
		{
			numVertices += primitive.GetNumVertices();
			numIndices += primitive.GetNumIndices();
		}
	}

	size_t vertexBufferSize = numVertices * sizeof(CookedVertex);
	size_t indexBufferSize = numIndices * sizeof(unsigned int);
	//now create vertex and index buffers
	mVertices = DeviceManager->CreateBuffer(vertexBufferSize);
	mIndices = DeviceManager->CreateBuffer(indexBufferSize);

	void* mappedVertices = nullptr;
	HRESULT result = mVertices->Map(0, nullptr, &mappedVertices);

	if (FAILED(result)) return false;

	ModelFile.read(static_cast<char*>(mappedVertices), static_cast<std::streamsize>(vertexBufferSize));

	mVertices->Unmap(0, nullptr);

	if (!ModelFile) return false;

	void* mappedIndices = nullptr;

	result = mIndices->Map(0, nullptr, &mappedIndices);

	if (FAILED(result)) return false;

	ModelFile.read(static_cast<char*>(mappedIndices), static_cast<std::streamsize>(indexBufferSize));

	mIndices->Unmap(0, nullptr);

	if (!ModelFile) return false;

	mVbView.BufferLocation = mVertices->GetGPUVirtualAddress();
	mVbView.SizeInBytes = vertexBufferSize;
	mVbView.StrideInBytes = sizeof(CookedVertex);

	mIbView.BufferLocation = mIndices->GetGPUVirtualAddress();
	mIbView.SizeInBytes = indexBufferSize;
	mIbView.Format = DXGI_FORMAT_R32_UINT;

	return true;
}
