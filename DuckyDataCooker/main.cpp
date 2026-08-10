#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <DirectXMath.h>
#include <set>
#include "tiny_gltf.h"

using namespace std;
using namespace DirectX;


struct CookedVertex
{
	XMFLOAT3 position {0.f,0.f,0.f};
	XMFLOAT3 normal   {0.f, 1.f, 0.f};
	XMFLOAT4 tangent  {1.f, 0.f, 0.f, 1.f};
	XMFLOAT2 texcoord0{0.f, 0.f};
	XMFLOAT4 color0   {1.f,1.f,1.f,1.f};
};

struct MaterialInfo
{
	XMFLOAT4 baseColor{1.f,1.f,1.f,1.f};
	XMFLOAT3 emissiveColor{ 1.f,1.f,1.f};

	float normalScale = 0.f;
	float roughness = 0.f; 
	float metal = 0.f; 

	// alpha info
	unsigned int blendMode = 0; 
	float alphaCutoff = 0.5f;
	unsigned int doubleSided = 0;

	unsigned int padding1;
	unsigned int padding2;

	int primTextureIndex = -1;
	int normTextureIndex = -1; 
	int metallicIndex = -1; 
	int emissiveIndex = -1;

};


struct BufferPointerStruct
{
	const tinygltf::Accessor*   outAccessor = nullptr;
	const tinygltf::BufferView* outBufferView = nullptr;
	const tinygltf::Buffer*     outBuffer = nullptr;
};

struct BufferStartAndStride
{
	const unsigned char* start = nullptr;
	size_t stride = 0;
};

struct MeshOutput
{
	unsigned int mNumPrimitives;
};

struct PrimitiveOutput
{
	unsigned char* mVertexData = nullptr;
	unsigned char* mIndexData = nullptr;
};

enum class TextureType
{
	BASE_COLOR = 0,
	NORMAL = 1,
	METALLIC_ROUGHNESS = 2,
	EMISSIVE = 3,
};

using TexturePair = std::pair<TextureType, std::string>;
std::map<TextureType, std::string> CompressionTypes = { TexturePair {TextureType::BASE_COLOR, "BC7_UNORM_SRGB"},
														TexturePair {TextureType::NORMAL, "BC7_UNORM"}, //normals are linear
														TexturePair {TextureType::METALLIC_ROUGHNESS, "BC7_UNORM"}, // so are roughness textures
														TexturePair {TextureType::EMISSIVE, "BC7_UNORM_SRGB"}};

const float stdMax = std::numeric_limits<float>::max();
const float stdMin = std::numeric_limits<float>::lowest();

struct AABB
{
	XMFLOAT4 mMin { stdMax, stdMax, stdMax, stdMax };
	XMFLOAT4 mMax{ stdMin, stdMin, stdMin, stdMin };
};

size_t totalMeshes = 0;

bool ConvertToDds(TextureType Type, const std::string& texconvPath, const std::string& inputJpg, const std::string& outputDir, string prefix) {
	
	std::string conversionFormat = CompressionTypes[Type];
	// Example command: texconv.exe -f BC1_UNORM -y input.jpg -o C:/output
	std::string cmd = texconvPath + " -px " + prefix + " -f " + conversionFormat + "  -y \"" + inputJpg + "\" -o \"" + outputDir + "\"";

	int result = std::system(cmd.c_str());

	if (result == 0) { std::cout << "Conversion successful!\n"; return true; }
	else { std::cerr << "Conversion failed with code: " << result << "\n"; return false; }
}

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

bool GetBufferView(const tinygltf::Primitive& Primitive, const tinygltf::Model& Model, const char* AttributeName, BufferPointerStruct& ReturnValues)
{ 
	const auto& attrib = Primitive.attributes.find(AttributeName);

	if (attrib == Primitive.attributes.end()) return false;

	ReturnValues.outAccessor   = &Model.accessors[attrib->second];
	ReturnValues.outBufferView = &Model.bufferViews[ReturnValues.outAccessor->bufferView];
	ReturnValues.outBuffer     = &Model.buffers[ReturnValues.outBufferView->buffer];

	return true;
}

void InitStartAndStride(BufferPointerStruct& PointerInfo, BufferStartAndStride& StartAndStride)
{
	StartAndStride.start = PointerInfo.outBuffer->data.data() + PointerInfo.outBufferView->byteOffset + PointerInfo.outAccessor->byteOffset;
	StartAndStride.stride = static_cast<size_t>(PointerInfo.outAccessor->ByteStride(*PointerInfo.outBufferView));
}

void MemCpyOverToCookedVertex(BufferStartAndStride& StartAndStride, void* Elem, size_t ElemSize)
{
	memcpy(Elem, (void*)StartAndStride.start, ElemSize);
	StartAndStride.start += StartAndStride.stride;
}

void FindAllVertexData(const tinygltf::Model& Model, 
						vector<vector<size_t>>& Offsets, 
						vector<int>& PrimitiveMaterials, 
						stringstream& VertexDataStream,
						stringstream& BoundingBoxStream)
{
	size_t currentVertOffset = 0;

	for (const auto& mesh : Model.meshes)
	{
		vector<size_t> primOffsets;
		for (const auto& primitive : mesh.primitives)
		{
			PrimitiveMaterials.push_back(primitive.material);
			primOffsets.push_back(currentVertOffset);
			std::cout << "global offset for vertices " << currentVertOffset << std::endl;

			BufferPointerStruct posBuffer;
			GetBufferView(primitive, Model, "POSITION", posBuffer);

			BufferPointerStruct normBuffer;
			bool bHasNormals = GetBufferView(primitive, Model, "NORMAL", normBuffer);

			BufferPointerStruct tangentBuffer;
			bool bHasTangents = GetBufferView(primitive, Model, "TANGENT", tangentBuffer);

			BufferPointerStruct uvBuffer;
			bool bHasUVs = GetBufferView(primitive, Model, "TEXCOORD_0", uvBuffer);

			BufferPointerStruct colorBuffer;
			bool bHasCols = GetBufferView(primitive, Model, "COLOR_0", colorBuffer);

			size_t Count = posBuffer.outAccessor->count;

			//entire size
			size_t BufferInfoSize = sizeof(CookedVertex) * Count;
			unsigned int CookedVertexSize = sizeof(CookedVertex);

			BufferStartAndStride posInfo;
			InitStartAndStride(posBuffer, posInfo);

			BufferStartAndStride normInfo, tangentInfo, uvInfo, colorInfo;
			if (bHasNormals)   InitStartAndStride(normBuffer, normInfo);
			if (bHasTangents)  InitStartAndStride(tangentBuffer, tangentInfo);
			if (bHasUVs)       InitStartAndStride(uvBuffer, uvInfo);
			if (bHasCols)      InitStartAndStride(colorBuffer, colorInfo);

			std::cout << "found vertex data " << Count << " vertices " << std::endl;

			// get min / max bounding box points for primitive
			AABB localBoundingBox;
			localBoundingBox.mMin.x = static_cast<float>(posBuffer.outAccessor->minValues[0]);
			localBoundingBox.mMin.y = static_cast<float>(posBuffer.outAccessor->minValues[1]);
			localBoundingBox.mMin.z = static_cast<float>(posBuffer.outAccessor->minValues[2]);
			localBoundingBox.mMin.w = 1.f;

			localBoundingBox.mMax.x = static_cast<float>(posBuffer.outAccessor->maxValues[0]);
			localBoundingBox.mMax.y = static_cast<float>(posBuffer.outAccessor->maxValues[1]);
			localBoundingBox.mMax.z = static_cast<float>(posBuffer.outAccessor->maxValues[2]);
			localBoundingBox.mMax.w = 1.f;

			// write out localBoundingBox
			BoundingBoxStream.write(reinterpret_cast<const char*>(&localBoundingBox), sizeof(AABB));

			for (int elem = 0; elem < Count; elem++)
			{
				CookedVertex cookedVertex{};

				MemCpyOverToCookedVertex(posInfo, (void*)&cookedVertex.position, sizeof(cookedVertex.position));
				if (bHasNormals)	MemCpyOverToCookedVertex(normInfo, (void*)&cookedVertex.normal, sizeof(cookedVertex.normal));
				if (bHasTangents)	MemCpyOverToCookedVertex(tangentInfo, (void*)&cookedVertex.tangent, sizeof(cookedVertex.tangent));
				if (bHasUVs)		MemCpyOverToCookedVertex(uvInfo, (void*)&cookedVertex.texcoord0, sizeof(cookedVertex.texcoord0));
				if (bHasCols)		MemCpyOverToCookedVertex(colorInfo, (void*)&cookedVertex.color0, sizeof(cookedVertex.color0));

				VertexDataStream.write((const char*)&cookedVertex, sizeof(CookedVertex));
				currentVertOffset++;
			}
		}
		primOffsets.push_back(currentVertOffset); // need one last one
		Offsets.push_back(primOffsets);
	}
	PrimitiveMaterials.push_back(-1);


}

void CountMeshNodes(const tinygltf::Node& Node, const tinygltf::Model& Model, size_t& numMeshNodes)
{
	if (Node.mesh != -1) numMeshNodes++;

	for (const auto& childNode : Node.children)
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
			XMVECTORF32 nextVector{  static_cast<float>(Node.matrix[i]), 
									 static_cast<float>(Node.matrix[i + 1]), 
									 static_cast<float>(Node.matrix[i + 2]), 
									 static_cast<float>(Node.matrix[i + 3])};

			nextTransform.r[i/4] = nextVector;
		}

		transform = nextTransform * transform;
	}

	streampos p = 0;
	if (Node.mesh != -1)
	{
		XMFLOAT4X4 storedWorld;
		XMStoreFloat4x4(&storedWorld,transform);
		tinygltf::Mesh mesh = Model.meshes[Node.mesh];
		int nodeMesh = Node.mesh;
		DataToFlushOut.write((char*)&nodeMesh, sizeof(int));
		DataToFlushOut.write((char*)&storedWorld, sizeof(XMFLOAT4X4));
	}

	for (const auto& childNode : Node.children)
	{
		const tinygltf::Node& nextNode = Model.nodes[childNode];
		WriteOutNodeData(nextNode, Model, DataToFlushOut, transform);
	}
}

void WriteOutTextureData(int index,
						 const tinygltf::Model& Model,
						 std::vector<std::string>& orderedNames,
						 TextureType Type, 
						 const std::filesystem::path& InputPath, 
						 const std::filesystem::path& OutputPath)
{
	static std::unordered_map<std::string, std::string> assignedNames;
	
	if (index != -1)
	{
		const tinygltf::Texture tex = Model.textures[index];
		const tinygltf::Image img = Model.images[tex.source];

		std::string textureName = img.uri;

		auto itr = assignedNames.find(textureName);

		if (itr == assignedNames.end())
		{
			static int prefix = 0;
			string prefixString = "_" + to_string(prefix);
			prefix++;
			string OutFilenameString = prefixString + std::filesystem::path(textureName).filename().replace_extension(".dds").string();
			const std::filesystem::path expectedDdsPath = OutputPath / "Textures" / OutFilenameString;

			string outputDirStr = OutputPath.string() + "/Textures/";
			string expectedDDSPathStr = expectedDdsPath.string();
			size_t expectedDDSPathLength = expectedDDSPathStr.length();

			orderedNames[index] = expectedDDSPathStr;
			const std::filesystem::path expectedInputPath = InputPath / textureName;

			ConvertToDds(Type, "texConv.exe", expectedInputPath.string(), outputDirStr, prefixString);

			assignedNames[textureName] = expectedDDSPathStr;
		}
	}
}

unsigned int DetermineAlphaMode(const std::string& str)
{
	if (str == "OPAQUE") return 0;
	else if (str == "MASK") return 1;
	else if (str == "BLEND") return 2;

	return 0;
}

void ProcessMaterials(const tinygltf::Model& Model,
					  stringstream& MaterialDataStream)
{
	size_t numMaterial = Model.materials.size();
	MaterialDataStream.write((const char*)&numMaterial, sizeof(size_t));

	for (const tinygltf::Material& material : Model.materials)
	{
		MaterialInfo newMaterial{};
		const tinygltf::PbrMetallicRoughness& pbrValues = material.pbrMetallicRoughness;
		newMaterial.primTextureIndex = pbrValues.baseColorTexture.index != -1 ? pbrValues.baseColorTexture.index : -1 ;
		newMaterial.normTextureIndex = material.normalTexture.index != -1 ? material.normalTexture.index : -1;
		newMaterial.normalScale = (float)material.normalTexture.scale;
		newMaterial.metallicIndex = pbrValues.metallicRoughnessTexture.index != -1 ? pbrValues.metallicRoughnessTexture.index : -1;
		newMaterial.emissiveIndex = material.emissiveTexture.index != -1 ? material.emissiveTexture.index : -1;

		const std::vector<double>& baseColorValues = pbrValues.baseColorFactor;
		newMaterial.baseColor =	XMFLOAT4 ((float)baseColorValues[0], (float)baseColorValues[1], (float)baseColorValues[2], (float)baseColorValues[3]);
		const std::vector<double>& emissiveColorValues = material.emissiveFactor;
		newMaterial.emissiveColor = XMFLOAT3((float)emissiveColorValues[0], (float)emissiveColorValues[1], (float)emissiveColorValues[2]);
		newMaterial.roughness = static_cast<float>(pbrValues.roughnessFactor);
		newMaterial.metal	  = static_cast<float>(pbrValues.metallicFactor);
	
		newMaterial.blendMode = DetermineAlphaMode(material.alphaMode);
		newMaterial.alphaCutoff = static_cast<float>(material.alphaCutoff);
		newMaterial.doubleSided = material.doubleSided;

		MaterialDataStream.write((const char*)&newMaterial, sizeof(MaterialInfo));
	}
}

void ProcessTextures(const tinygltf::Model& Model,
					 std::vector<std::string>& orderedNames,
					 const std::filesystem::path& TextureInputAssetPath,
					 const std::filesystem::path& TextureOutputAssetPath)
{
	for (const tinygltf::Material& material : Model.materials)
	{
		unsigned int textureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
		if (textureIndex != -1)
		{
			WriteOutTextureData(textureIndex, Model, orderedNames, TextureType::BASE_COLOR, TextureInputAssetPath, TextureOutputAssetPath);
		}

		textureIndex = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
		if (textureIndex != -1)
		{
			WriteOutTextureData(textureIndex, Model, orderedNames, TextureType::METALLIC_ROUGHNESS, TextureInputAssetPath, TextureOutputAssetPath);
		}

		textureIndex = material.normalTexture.index;
		if (textureIndex != -1)
		{
			WriteOutTextureData(textureIndex, Model, orderedNames, TextureType::NORMAL, TextureInputAssetPath, TextureOutputAssetPath);
		}

		textureIndex = material.emissiveTexture.index;
		if (textureIndex != -1)
		{
			WriteOutTextureData(textureIndex, Model, orderedNames, TextureType::EMISSIVE, TextureInputAssetPath, TextureOutputAssetPath);
		}
	}
}

void FindAllIndexData(const tinygltf::Model& Model, stringstream& MeshDataStream, vector<vector<size_t>>& IndexOffsets)
{
	size_t globalIndexOffset = 0;

	for (const auto& mesh : Model.meshes)
	{
		vector<size_t> currentPrim;
		for (const auto& primitive : mesh.primitives)
		{
			currentPrim.push_back(globalIndexOffset);
			cout << "global index offset: " << globalIndexOffset << endl;
			if (primitive.indices >= 0)
			{
				const tinygltf::Accessor& accessor = Model.accessors[primitive.indices];
				const tinygltf::BufferView& bufferView = Model.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = Model.buffers[bufferView.buffer];

				unsigned int stride = 0;
				std::cout << "found index data " << accessor.count << " indices " << std::endl;

				if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
				{
					cout << "yikes! dx12 accepts 16 bit indices as smallest" << endl;
					return;
				}
				else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) stride = 2;
				else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)   stride = 4;

				const unsigned char* start = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
				
				for (unsigned int elem = 0; elem < accessor.count; elem++)
				{
					if (stride == 2)
					{
						unsigned short idx = (unsigned short)(*start);
						MeshDataStream.write((char*)start, sizeof(unsigned short));
					}
					else
					{
						unsigned int idx = (unsigned int)(*start);
						MeshDataStream.write((char*)start, sizeof(unsigned int));
					}

					start += stride;
				}

				globalIndexOffset += accessor.count;
			}
			else
			{
				// if there are no indices, generate our own in a sequence, makes it easier for rendering engine to handle
				std::cout << " No index Data, generating own" << std::endl;
				auto itr = primitive.attributes.find("POSITION");
				const tinygltf::Accessor& accessor = Model.accessors[itr->second];
				
				for (unsigned int i = 0; i < accessor.count; i++)
				{
					MeshDataStream.write((char*)&i, sizeof(unsigned int));
					globalIndexOffset++;
				}

				currentPrim.push_back(globalIndexOffset);
			}
		}
		currentPrim.push_back(globalIndexOffset);
		IndexOffsets.push_back(currentPrim);
	}

	
}

void FindMeshData(const tinygltf::Model& Model, 
				  stringstream& MeshDataStream,
				  stringstream& OffsetsStream,
				  stringstream& BoundingBoxStream)
{
	//write out a mega buffer of mesh data!
	vector<vector<size_t>> vertexOffsets;
	vector<vector<size_t>> indexOffsets;
	vector<int> primMaterials;

	FindAllVertexData(Model, vertexOffsets, primMaterials, MeshDataStream, BoundingBoxStream);
	FindAllIndexData(Model, MeshDataStream, indexOffsets);

	int numMeshes = vertexOffsets.size();
	OffsetsStream.write((const char*)&numMeshes, sizeof(int));
	int primOffset = 0;
	for (int i = 0; i < numMeshes; i++)
	{
		int primsSize = vertexOffsets[i].size() -1;
		OffsetsStream.write((const char*)&primsSize, sizeof(int));

		for (int j = 0; j < primsSize; j++)
		{
			OffsetsStream.write((const char*)&primMaterials[primOffset++], sizeof(int));

			// vertex number and offset
			size_t Offset = vertexOffsets[i][j];
			size_t Num = vertexOffsets[i][j + 1] - Offset;

			OffsetsStream.write((const char*)&Num, sizeof(size_t));
			OffsetsStream.write((const char*)&Offset, sizeof(size_t));
			
			// same for indices
			Offset = indexOffsets[i][j];
			Num = indexOffsets[i][j + 1] - Offset;

			OffsetsStream.write((const char*)&Num, sizeof(size_t));
			OffsetsStream.write((const char*)&Offset, sizeof(size_t));

			AABB nextBB;
			BoundingBoxStream.read((char*)&nextBB, sizeof(AABB));
			OffsetsStream.write((const char*)&nextBB, sizeof(AABB));
		}
	}
}

int main(int argc, char** argv)
{
	tinygltf::TinyGLTF loader;
	tinygltf::Model model;
	std::string error;
	std::string warning;

	if (argc < 2)
	{
		cout << "please give a input model to the command line" << endl;
		return 1;
	}

	string path = "..//Assets//InputAssets//";
	string asset(argv[1]);
	string suffix = "//scene.gltf";

	bool success = loader.LoadASCIIFromFile(&model, &error, &warning, path + asset + suffix);

	if (!success)
	{
		std::cout << "Error: " << error.c_str();
		return 1;
	}

	const std::filesystem::path inputDirectory = std::filesystem::path("..") / "Assets" / "InputAssets" / asset ;
	const std::filesystem::path outputDirectory = std::filesystem::path("..") / "Assets" / "CookedAssets" / asset;
	std::filesystem::create_directories(outputDirectory / "Textures");
	ofstream outputFile(outputDirectory.string() + "//CookedData.Ducky", ios::binary);

	if (!outputFile) return 1;

	// three streams that will be output later in this order
	// this allows easier traversal of data but output in a different order later!
	stringstream instanceTransformData;
	stringstream materialsData;
	stringstream offsetData;
	stringstream bufferData;
	stringstream boundingBoxData;

	const tinygltf::Scene& scene = model.scenes[model.defaultScene];
	const tinygltf::Node& rootNode = model.nodes[scene.nodes[0]];

	// Num Meshes and Transform Data
	size_t numMeshNodes = 0;
	CountMeshNodes(rootNode, model, numMeshNodes);
	instanceTransformData.write((char*)&numMeshNodes, sizeof(size_t));

	XMMATRIX Transform = XMMatrixIdentity();
	WriteOutNodeData(rootNode, model, instanceTransformData, Transform);

	// Textures and Materials

	std::vector<std::string> orderedNames(model.textures.size());

	string textureOutputPath = outputDirectory.string();
	ProcessTextures(model, orderedNames, inputDirectory, textureOutputPath);

	size_t numTextures = model.textures.size();
	materialsData.write((const char*)&numTextures, sizeof(size_t));
	for (const std::string& str : orderedNames)
	{
		size_t strLength = str.length();
		materialsData.write((const char*)&strLength, sizeof(size_t));
		materialsData.write((const char*)&str[0], strLength);
	}

	ProcessMaterials(model, materialsData);
	FindMeshData(model, bufferData, offsetData, boundingBoxData);

	outputFile << instanceTransformData.rdbuf() << materialsData.rdbuf() << offsetData.rdbuf() << bufferData.rdbuf();
	outputFile.close();

	return 0;
}