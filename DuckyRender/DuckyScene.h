#pragma once
#include "DuckyMesh.h"
#include "DuckyUploadContext.h"

const XMFLOAT4 BaseColorFallback{ 1.f, 1.f, 1.f, 1.f };
const XMFLOAT4 NormalFallback{ 0.5f, 0.5f, 1.f, 1.f };
const XMFLOAT4 EmissiveFallback{ 0.f,0.f,0.f, 0.f };

class DuckyScene
{
    public:
        bool Init(std::ifstream& InFile, D3DDeviceManager* DeviceManager, DuckyUploadContext& UploadContext);

        const std::vector<DuckyMeshData>& GetMeshes() const { return mMeshes; }
        const std::vector<DuckyMeshInstance>& GetInstances() const { return mInstances; }
        const std::vector<DuckyMaterial>& GetMaterials() const { return mMaterials; }
        const std::vector<DescriptorHeapResource>& GetTextures() const { return mTextures; }

        const  DescriptorHeapResource& GetInstancesHeapBuffer() const { return mGPUInstances; }
        const  MappedDescriptorHeapResource& GetMaterialsHeapBuffer() const { return mGPUMaterials; }
        const  MappedDescriptorHeapResource& GetDrawsHeapBuffer() const     { return mGPUDraws; }

        const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return mVbView; }
        const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const { return mIbView; }
    private:
        bool InitInstanceData(std::ifstream& InFile, D3DDeviceManager* DeviceManager, DuckyUploadContext& UploadContext);
        bool InitMaterials(std::ifstream& InFile, D3DDeviceManager* DeviceManager);
        bool InitTextures(std::ifstream& InFile, D3DDeviceManager* DeviceManager);
        bool InitMeshes(std::ifstream& InFile, D3DDeviceManager* DeviceManager);
        bool InitVertexAndIndexMegaBuffer(std::ifstream& ModelFile, D3DDeviceManager* DeviceManager, DuckyUploadContext& UploadContext);

        std::vector<DuckyMeshData>          mMeshes;
        std::vector<DuckyMeshInstance>      mInstances;
        std::vector<DuckyMaterial>          mMaterials;
        std::vector<DescriptorHeapResource> mTextures;

        DescriptorHeapResource mGPUInstances;
        MappedDescriptorHeapResource mGPUMaterials;
        MappedDescriptorHeapResource mGPUDraws;

        AABB Bounds;

        int mBaseColorFallbackHandle = 0;
        int mNormalColorFallbackHandle = 0;
        int mEmissiveColorFallbackHandle = 0;
        int mMetallicRougnessFallbackHandle = 0;

        Microsoft::WRL::ComPtr<ID3D12Resource> mVertices;
        Microsoft::WRL::ComPtr<ID3D12Resource> mIndices;

        D3D12_VERTEX_BUFFER_VIEW mVbView;
        D3D12_INDEX_BUFFER_VIEW  mIbView;
};