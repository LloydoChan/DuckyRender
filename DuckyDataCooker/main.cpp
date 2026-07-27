#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <DirectXMath.h>
#include "tiny_gltf.h"

using namespace std;
using namespace DirectX;

struct MeshOutput
{
	unsigned int mNumPrimitives;
};

struct PrimitiveOutput
{
	unsigned char* mVertexData = nullptr;
	unsigned char* mIndexData = nullptr;
};

size_t totalMeshes = 0;

void DebugMatrix(XMMATRIX& nextTransform)
{
	XMFLOAT4X4 debugMatrix;
	XMStoreFloat4x4(
		&debugMatrix,
		nextTransform);

	std::cout
		<< std::setw(10) << debugMatrix._41 << ", "
		<< std::setw(10) << debugMatrix._42 << ", "
		<< std::setw(10) << debugMatrix._43 << ", "
		<< std::setw(10) << debugMatrix._44 << std::endl;
}

void FindPrimitiveDataPositionUVs(const tinygltf::Primitive& Primitive, const tinygltf::Model& Model, stringstream& DataToFlushOut)
{
	const tinygltf::Accessor& vertexAccessor = Model.accessors[Primitive.attributes.find("POSITION")->second];
	const tinygltf::BufferView& vertexBufferView = Model.bufferViews[vertexAccessor.bufferView];
	const tinygltf::Buffer& vertexBuffer = Model.buffers[vertexBufferView.buffer];

	const tinygltf::Accessor& uvAccessor = Model.accessors[Primitive.attributes.find("TEXCOORD_0")->second];
	const tinygltf::BufferView& uvBufferView = Model.bufferViews[uvAccessor.bufferView];
	const tinygltf::Buffer& uvBuffer = Model.buffers[uvBufferView.buffer];

	const unsigned char* vertexStart = &vertexBuffer.data[vertexAccessor.byteOffset + vertexBufferView.byteOffset];
	const unsigned char* uvStart     = &uvBuffer.data[uvAccessor.byteOffset + uvBufferView.byteOffset];

	size_t size = vertexAccessor.count * sizeof(float) * 5;
	DataToFlushOut.write((char*)&size, sizeof(size_t));
	size_t vertSize = sizeof(float) * 3;
	size_t uvSize = sizeof(float) * 2;
	for (int elem = 0; elem < vertexAccessor.count; elem++)
	{
		DataToFlushOut.write(reinterpret_cast<const char*>(vertexStart), vertSize);
		vertexStart += vertSize;
		DataToFlushOut.write(reinterpret_cast<const char*>(uvStart), uvSize);
		uvStart += uvSize;
	}
	
	size_t pos = DataToFlushOut.tellp();
}

void CountMeshNodes(const tinygltf::Node& Node, const tinygltf::Model& Model, size_t& numMeshNodes)
{
	if (Node.mesh != -1) numMeshNodes++;

	for (auto childNode : Node.children)
	{
		const tinygltf::Node& nextNode = Model.nodes[childNode];
		CountMeshNodes(nextNode, Model, numMeshNodes);
	}
}

void WriteOutNodeData(const tinygltf::Node& Node, const tinygltf::Model& Model, stringstream& DataToFlushOut, XMMATRIX transform)
{
	XMMATRIX nextTransform = XMMatrixIdentity();
	if (Node.matrix.size() != 0)
	{
		unsigned int i = 0;
		for (unsigned int i = 0; i < 16; i+=4)
		{
			XMVECTORF32 nextVector{ static_cast<float>(Node.matrix[i]), 
									 static_cast<float>(Node.matrix[i + 1]), 
									 static_cast<float>(Node.matrix[i + 2]), 
									 static_cast<float>(Node.matrix[i + 3])};

			nextTransform.r[i/4] = nextVector;
		}

		transform = nextTransform * transform;
	}
	else if (Node.translation.size() > 0 || Node.scale.size() > 0 || Node.rotation.size() > 0)
	{
		cout << "huh" << endl;
	}

	streampos p = 0;
	if (Node.mesh != -1)
	{
		XMFLOAT4X4 storedWorld;
		XMStoreFloat4x4(&storedWorld,transform);
		XMMATRIX transposed = XMMatrixTranspose(transform);
		DebugMatrix(transposed);
		tinygltf::Mesh mesh = Model.meshes[Node.mesh];
		int nodeMesh = Node.mesh;
		DataToFlushOut.write((char*)&nodeMesh, sizeof(int));
		DataToFlushOut.write((char*)&storedWorld, sizeof(XMFLOAT4X4));
	}

	for (auto childNode : Node.children)
	{
		const tinygltf::Node& nextNode = Model.nodes[childNode];
		WriteOutNodeData(nextNode, Model, DataToFlushOut, transform);
	}
}

void WriteOutMeshData(const tinygltf::Node& Node, const tinygltf::Model& Model, stringstream& DataToFlushOut)
{
	// first write out Node info, get transforms and mesh indices of all nodes...
	//top node

	// now write out Mesh Data
	for (auto& mesh : Model.meshes)
	{
		size_t numPrims = mesh.primitives.size();
		DataToFlushOut.write((char*)&numPrims, sizeof(size_t));

		for (const auto& primitive : mesh.primitives)
		{
			// get texture info for this primitive
			const tinygltf::Material primMaterial = Model.materials[primitive.material];
			int primTextureIndex = primMaterial.pbrMetallicRoughness.baseColorTexture.index;
			const tinygltf::Image img = Model.images[primTextureIndex];
			std::string albedoName = img.uri;
			size_t albedoNameLength = albedoName.length();
			DataToFlushOut.write((char*)&albedoNameLength, sizeof(size_t));
			DataToFlushOut.write((char*)&albedoName[0], albedoNameLength);

			size_t pos_t = DataToFlushOut.tellp();
			FindPrimitiveDataPositionUVs(primitive, Model, DataToFlushOut);
			
			pos_t = DataToFlushOut.tellp();
			if (primitive.indices >= 0)
			{
				const tinygltf::Accessor& accessor = Model.accessors[primitive.indices];
				const tinygltf::BufferView& bufferView = Model.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = Model.buffers[bufferView.buffer];

				unsigned int stride = 0;
				pos_t = DataToFlushOut.tellp();
				std::cout << "found index data " << accessor.count << " indices " << std::endl;

				if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) stride = 1;
				else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) stride = 2;
				else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)   stride = 4;

				const unsigned char* start = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
				const unsigned char* end = start + accessor.count * stride;
				size_t size = end - start;
				DataToFlushOut.write((char*)&size, sizeof(size_t));
				DataToFlushOut.write((char*)start, size);

			}
		}
	}
}

int main()
{
	tinygltf::TinyGLTF loader;
	tinygltf::Model model;
	std::string error;
	std::string warning;

	bool success = loader.LoadASCIIFromFile(&model, &error, &warning, "models//scene.gltf");

	if (!success)
	{
		std::cout << "Error: " << error.c_str();
		return 1;
	}
	
	ofstream outputFile("models//CookedData.Ducky", std::ios::binary);
	stringstream dataToFlushOut;

	const tinygltf::Scene& scene = model.scenes[model.defaultScene];
	const tinygltf::Node& rootNode = model.nodes[scene.nodes[0]];

	size_t numMeshNodes = 0;
	CountMeshNodes(rootNode, model, numMeshNodes);
	dataToFlushOut.write((char*)&numMeshNodes, sizeof(size_t));

	XMMATRIX Transform = XMMatrixIdentity();
	WriteOutNodeData(rootNode, model, dataToFlushOut, Transform);

	size_t numMeshes = model.meshes.size();
	dataToFlushOut.write((char*)&numMeshes, sizeof(size_t));
	WriteOutMeshData(model.nodes[scene.nodes[0]], model, dataToFlushOut);

	outputFile << dataToFlushOut.rdbuf();
	size_t s = outputFile.tellp();
	outputFile.close();

	return 0;
}