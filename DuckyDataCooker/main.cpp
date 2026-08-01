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

bool ConvertToDds(TextureType Type, const std::string& texconvPath, const std::string& inputJpg, const std::string& outputDir) {
	
	std::string conversionFormat = CompressionTypes[Type];
	// Example command: texconv.exe -f BC1_UNORM -y input.jpg -o C:/output
	std::string cmd = texconvPath + " -f " + conversionFormat +"  -y \"" + inputJpg + "\" -o \"" + outputDir + "\"";

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

void FindPrimitiveData(const tinygltf::Primitive& Primitive, const tinygltf::Model& Model, stringstream& DataToFlushOut)
{

	BufferPointerStruct posBuffer;
	GetBufferView(Primitive, Model, "POSITION", posBuffer);

	BufferPointerStruct normBuffer;
	bool bHasNormals = GetBufferView(Primitive, Model, "NORMAL", normBuffer);

	BufferPointerStruct tangentBuffer;
	bool bHasTangents = GetBufferView(Primitive, Model, "TANGENT", tangentBuffer);

	BufferPointerStruct uvBuffer;
	bool bHasUVs = GetBufferView(Primitive, Model, "TEXCOORD_0", uvBuffer);

	BufferPointerStruct colorBuffer;
	bool bHasCols = GetBufferView(Primitive, Model, "COLOR_0", colorBuffer);

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
	

	//TODO this is temp
	DataToFlushOut.write((const char*)&CookedVertexSize, sizeof(unsigned int));
	DataToFlushOut.write((const char*)&BufferInfoSize, sizeof(size_t));

	for (int elem = 0; elem < Count; elem++)
	{
		CookedVertex cookedVertex{};

		MemCpyOverToCookedVertex(posInfo, (void*)&cookedVertex.position, sizeof(cookedVertex.position));
		if (bHasNormals)	MemCpyOverToCookedVertex(normInfo, (void*)&cookedVertex.normal,		sizeof(cookedVertex.normal));
		if (bHasTangents)	MemCpyOverToCookedVertex(tangentInfo, (void*)&cookedVertex.tangent, sizeof(cookedVertex.tangent));
		if (bHasUVs)		MemCpyOverToCookedVertex(uvInfo, (void*)&cookedVertex.texcoord0,	sizeof(cookedVertex.texcoord0));
		if (bHasCols)		MemCpyOverToCookedVertex(colorInfo, (void*)&cookedVertex.color0,	sizeof(cookedVertex.color0));

		DataToFlushOut.write((const char*)&cookedVertex, sizeof(CookedVertex));
	}

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
	DataToFlushOut.write(reinterpret_cast<const char*>(&localBoundingBox), sizeof(AABB));
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
						 stringstream& DataToFlushOut, 
						 TextureType Type, 
						 const std::filesystem::path& InputPath, 
						 const std::filesystem::path& OutputPath)
{
	static std::set<string> alreadySeen;

	if (index != -1)
	{
		const tinygltf::Texture tex = Model.textures[index];
		const tinygltf::Image img = Model.images[tex.source];

		std::string textureName = img.uri;


		const std::filesystem::path expectedDdsPath =
			OutputPath / "Textures" /
			std::filesystem::path(textureName).filename()
			.replace_extension(".dds");

		string outputDirStr = OutputPath.string() + "/Textures/";
		string expectedDDSPathStr = expectedDdsPath.string();
		size_t expectedDDSPathLength = expectedDDSPathStr.length();

		DataToFlushOut.write((char*)&expectedDDSPathLength, sizeof(size_t));
		DataToFlushOut.write((char*)&expectedDDSPathStr[0], expectedDDSPathLength);

		if (alreadySeen.contains(textureName)) return;
		alreadySeen.insert(textureName);

		const std::filesystem::path expectedInputPath =
			InputPath / textureName;

		ConvertToDds(Type, "texConv.exe", expectedInputPath.string(), outputDirStr);
	}
}

unsigned int DetermineAlphaMode(const std::string& str)
{
	if (str == "OPAQUE") return 0;
	else if (str == "MASK") return 1;
	else if (str == "BLEND") return 2;
}

void WriteOutMeshData(const tinygltf::Node& Node, 
					  const tinygltf::Model& Model, 
					  stringstream& DataToFlushOut, 
					  AABB& GlobalBoundingBox, 
					  const std::filesystem::path& InputPath, 
					  const std::filesystem::path& OutputPath)
{
	// first write out Node info, get transforms and mesh indices of all nodes...
	//top node

	// now write out Mesh Data
	for (const auto& mesh : Model.meshes)
	{
		size_t numPrims = mesh.primitives.size();
		DataToFlushOut.write((char*)&numPrims, sizeof(size_t));

		for (const auto& primitive : mesh.primitives)
		{
			// get texture info for this primitive
			const tinygltf::Material& primMaterial = Model.materials[primitive.material];
			int primTextureIndex = primMaterial.pbrMetallicRoughness.baseColorTexture.index;

			DataToFlushOut.write((char*)&primTextureIndex, sizeof(int));
			WriteOutTextureData(primTextureIndex, Model, DataToFlushOut, TextureType::BASE_COLOR, InputPath, OutputPath);

			int normTextureIndex = primMaterial.normalTexture.index;
			DataToFlushOut.write((char*)&normTextureIndex, sizeof(int));
			WriteOutTextureData(normTextureIndex, Model, DataToFlushOut, TextureType::NORMAL, InputPath, OutputPath);

			int metallicIndex = primMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
			DataToFlushOut.write((char*)&metallicIndex, sizeof(int));
			WriteOutTextureData(metallicIndex, Model, DataToFlushOut, TextureType::METALLIC_ROUGHNESS, InputPath, OutputPath);

			int emissiveIndex = primMaterial.emissiveTexture.index;
			DataToFlushOut.write((char*)&emissiveIndex, sizeof(int));
			WriteOutTextureData(emissiveIndex, Model, DataToFlushOut, TextureType::EMISSIVE, InputPath, OutputPath);

			const tinygltf::PbrMetallicRoughness& pbrValues = primMaterial.pbrMetallicRoughness;
			const std::vector<double>& baseColorValues = primMaterial.pbrMetallicRoughness.baseColorFactor;
			XMFLOAT4 baseColor ((float)baseColorValues[0], (float)baseColorValues[1], (float)baseColorValues[2], (float)baseColorValues[3]);
			DataToFlushOut.write((char*)&baseColor, sizeof(float) * 4);

			float normalScale = (float)primMaterial.normalTexture.scale;
			DataToFlushOut.write((char*)&normalScale, sizeof(float));

			float roughness = static_cast<float>(pbrValues.roughnessFactor);
			DataToFlushOut.write((char*)&roughness, sizeof(float));

			float metal = static_cast<float>(pbrValues.metallicFactor);
			DataToFlushOut.write((char*)&metal, sizeof(float));

			// alpha info
			unsigned int blendMode = DetermineAlphaMode(primMaterial.alphaMode);
			DataToFlushOut.write((char*)&blendMode, sizeof(unsigned int));

			if (blendMode == 1) 
				cout << "mask!" << endl;

			float alphaCutoff = static_cast<float>(primMaterial.alphaCutoff);
			DataToFlushOut.write((char*)&alphaCutoff, sizeof(float));

			DataToFlushOut.write((char*)&primMaterial.doubleSided, sizeof(unsigned int));

			
			FindPrimitiveData(primitive, Model, DataToFlushOut);
			
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

				DataToFlushOut.write((char*)&stride, sizeof(unsigned int));

				const unsigned char* start = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
				const unsigned char* end = start + accessor.count * stride;

				size_t size = end - start;
				DataToFlushOut.write((char*)&size, sizeof(size_t));
				DataToFlushOut.write((char*)start, size);

			}
			else
			{
				// if there are no indices, generate our own in a sequence, makes it easier for rendering engine to handle
				std::cout << " No index Data, generating own" << std::endl;
				auto itr = primitive.attributes.find("POSITION");
				const tinygltf::Accessor& accessor = Model.accessors[itr->second];
				unsigned int stride = sizeof(unsigned int);
				size_t indexBufferSize = accessor.count * stride;
				DataToFlushOut.write((char*)&stride, sizeof(unsigned int));
				DataToFlushOut.write((char*)&indexBufferSize, sizeof(size_t));
				for (unsigned int i = 0; i < accessor.count; i++)
				{
					DataToFlushOut.write((char*)&i, sizeof(unsigned int));
				}
			}
		}
	}

	//DataToFlushOut.write((char*)&GlobalBoundingBox, sizeof(AABB));
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

	const std::filesystem::path outputDirectory = std::filesystem::path("..") / "Assets" / "CookedAssets" / asset;
	ofstream outputFile(outputDirectory.string() + "//CookedData.Ducky", std::ios::binary);
	std::filesystem::create_directories(outputDirectory / "Textures");


	if (!outputFile) return 1;
	stringstream dataToFlushOut;

	const tinygltf::Scene& scene = model.scenes[model.defaultScene];
	const tinygltf::Node& rootNode = model.nodes[scene.nodes[0]];

	size_t numMeshNodes = 0;
	CountMeshNodes(rootNode, model, numMeshNodes);
	dataToFlushOut.write((char*)&numMeshNodes, sizeof(size_t));

	XMMATRIX Transform = XMMatrixIdentity();
	WriteOutNodeData(rootNode, model, dataToFlushOut, Transform);

	string textureOutputPath = outputDirectory.string();

	const std::filesystem::path inputPath = std::filesystem::path("..") / "Assets" / "InputAssets" / asset;

	size_t numMeshes = model.meshes.size();
	dataToFlushOut.write((char*)&numMeshes, sizeof(size_t));
	std::filesystem::create_directories(textureOutputPath);
	AABB globalBoundingBox;
	WriteOutMeshData(model.nodes[scene.nodes[0]], model, dataToFlushOut, globalBoundingBox, inputPath, outputDirectory);

	outputFile << dataToFlushOut.rdbuf();
	size_t s = outputFile.tellp();
	outputFile.close();

	return 0;
}