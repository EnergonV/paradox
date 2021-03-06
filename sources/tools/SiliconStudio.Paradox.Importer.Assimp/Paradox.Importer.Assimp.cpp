// Copyright (c) 2014 Silicon Studio Corp. (http://siliconstudio.co.jp)
// This file is distributed under GPL v3. See LICENSE.md for details.
#include "stdafx.h"
#include "../SiliconStudio.Paradox.Assimp.Translation/Extension.h"
#include "../SiliconStudio.Paradox.Importer.Common/ImporterUtils.h"

#include <string>
#include <map>
#include <set>

using namespace System;
using namespace System::Collections;
using namespace System::Collections::Generic;
using namespace System::Diagnostics;
using namespace System::IO;
using namespace SiliconStudio;
using namespace SiliconStudio::Core;
using namespace SiliconStudio::Core::Diagnostics;
using namespace SiliconStudio::Core::IO;
using namespace SiliconStudio::Core::Mathematics;
using namespace SiliconStudio::Core::Serialization;
using namespace SiliconStudio::Core::Serialization::Assets;
using namespace SiliconStudio::Core::Serialization::Contents;
using namespace SiliconStudio::Paradox::Assets::Materials;
using namespace SiliconStudio::Paradox::Rendering;
using namespace SiliconStudio::Paradox::Rendering::Materials;
using namespace SiliconStudio::Paradox::Rendering::Materials::ComputeColors;
using namespace SiliconStudio::Paradox::AssimpNet;
using namespace SiliconStudio::Paradox::Animations;
using namespace SiliconStudio::Paradox::Engine;
using namespace SiliconStudio::Paradox::Extensions;
using namespace SiliconStudio::Paradox::Graphics;
using namespace SiliconStudio::Paradox::Graphics::Data;
using namespace SiliconStudio::Paradox::Shaders;

using namespace Assimp;
using namespace SiliconStudio::Paradox::Importer::Common;

namespace SiliconStudio { namespace Paradox { namespace Importer { namespace AssimpNET {

public ref class MaterialInstantiation
{
public:
	List<String^>^ Parameters;
	MaterialAsset^ Material;
	String^ MaterialName;
};

public ref class MaterialInstances
{
public:
	MaterialInstances()
	{
		Instances = gcnew List<MaterialInstantiation^>();
	}

	aiMaterial* SourceMaterial;
	List<MaterialInstantiation^>^ Instances;
	String^ MaterialsName;
};

public ref class MeshInfo
{
public:
	MeshInfo()
	{
		HasSkinningPosition = false;
		HasSkinningNormal = false;
		TotalClusterCount = 0;
	}

	MeshDraw^ Draw;
	List<MeshBoneDefinition>^ Bones;
	String^ Name;
	int MaterialIndex;
	bool HasSkinningPosition;
	bool HasSkinningNormal;
	int TotalClusterCount;
};

public ref class MeshConverter
{
public:
	property Logger^ Logger;
	property bool AllowUnsignedBlendIndices;
	property float ScaleImport;

private:
	Stopwatch^ internalStopwatch;

	// ----- From here: convertion-wise data ----- //
	
	String^ vfsInputFilename;
	String^ vfsOutputFilename;
	String^ vfsInputPath;

	Model^ modelData;
	List<ModelNodeDefinition> nodes;
	Dictionary<IntPtr, int> nodeMapping;
	Dictionary<String^, int>^ textureNameCount;

	List<Mesh^>^ effectMeshes;	// array of EffectMeshes built from the aiMeshes (same order)
	Dictionary<String^, List<Entity^ >^ >^ nodeNameToNodeData; // access to the built nodeData via their String ID (may not be unique)
	Dictionary<IntPtr, Entity^>^ nodePtrToNodeData; // access to the built nodeData via their corresponding aiNode
	Dictionary<int, List<Entity^>^ >^ meshIndexToReferingNodes; // access all the nodes that reference a Mesh Index

public:
	MeshConverter(Core::Diagnostics::Logger^ logger)
	{
		if(logger == nullptr)
			logger = Core::Diagnostics::GlobalLogger::GetLogger("Importer Assimp");

		internalStopwatch = gcnew Stopwatch();
		
		Logger = logger;
	}

private:
	const aiScene* Initialize(String^ inputFilename, String^ outputFilename, Assimp::Importer* importer, unsigned int flags)
	{
		// reset all cached data for this new stream convertion
		ResetConvertionData();

		// TODO: correct this hack
		auto splitString = inputFilename->Split(':');
		auto unixStyleInputFilename = "/" + splitString[0] + splitString[1];

		this->vfsInputFilename = inputFilename;
		this->vfsOutputFilename = outputFilename;
		this->vfsInputPath = VirtualFileSystem::GetParentFolder(inputFilename);

		/*
		auto stream = VirtualFileSystem::Drive->OpenStream(unixStyleInputFilename, VirtualFileMode::Open, VirtualFileAccess::Read, VirtualFileShare::Read, StreamFlags::None);

		// check and possibly adjust the input parameters
		if(stream == nullptr)
			throw gcnew ArgumentNullException("Input Stream");
		
		// set file wise info
		//absolutePath = absPath;
		//relativePath = dstSrcRelativePath;

		// copies the all the stream in local memory
		// Not the best memory-wise, need to be optimized
		internalStopwatch->StartNew();
		auto memoryStream = gcnew MemoryStream();
		stream->CopyTo(memoryStream);
		auto buffer = memoryStream->ToArray();
		Logger->Verbose("File Stream copied - Time taken: {0} ms.", nullptr, internalStopwatch->ElapsedMilliseconds);

		//importer->SetIOHandler(new IOSystemCustom());

		// load the mesh from its original format to Assimp data structures
		internalStopwatch->StartNew();
		
		cli::pin_ptr<System::Byte> bufferStart = &buffer[0];
		return importer->ReadFileFromMemory(bufferStart, buffer->Length, flags);
		*/
		
		std::string unmanaged = msclr::interop::marshal_as<std::string>(inputFilename);
		return importer->ReadFile(unmanaged, flags);
	}

	// Reset all data related to one specific file conversion
	void ResetConvertionData()
	{
		effectMeshes = gcnew List<Mesh^>();
		nodeNameToNodeData = gcnew Dictionary<String^, List<Entity^ >^ >();
		nodePtrToNodeData = gcnew Dictionary<IntPtr, Entity^>();
		meshIndexToReferingNodes = gcnew Dictionary<int, List<Entity^>^ >();
		textureNameCount = gcnew Dictionary<String^, int>();
	}
	
	void ExtractEmbededTexture(aiTexture* texture)
	{
		Logger->Warning("The input file contains embeded textures. Embeded textures are not currently supported. This texture will be ignored",
						gcnew NotImplementedException("Embeded textures extraction"),
						CallerInfo::Get(__FILEW__, __FUNCTIONW__, __LINE__));
	}

	void NormalizeVertexWeights(std::vector<std::vector<std::pair<short,float> > >& controlPts, int nbBoneByVertex)
	{
		for(unsigned int vertexId=0; vertexId<controlPts.size(); ++vertexId)
		{
			auto& curVertexWeights = controlPts[vertexId];

			// check that one vertex has not more than 'nbBoneByVertex' associated bones
			if((int) curVertexWeights.size() > nbBoneByVertex)
				Logger->Warning("The input file contains vertices that are associated to more than {0} bones. In current version of the system, a single vertex can only be associated to {0} bones. Extra bones will be ignored",
								gcnew ArgumentOutOfRangeException("To much bones influencing a single vertex"),
								CallerInfo::Get(__FILEW__, __FUNCTIONW__, __LINE__));

			// resize the weights so that they contains exactly the number of bone weights required
			curVertexWeights.resize(nbBoneByVertex, std::pair<short, float>(0, 0.0f));

			float totalWeight = 0.f;
			for(int boneId=0; boneId<nbBoneByVertex; ++boneId)
				totalWeight += curVertexWeights[boneId].second;

			if(totalWeight == 0.f) // Assimp weights are positive, so in this case all weights are nulls
				continue;

			for(int boneId=0; boneId<nbBoneByVertex; ++boneId)
				curVertexWeights[boneId].second /= totalWeight;
		}
	}
	
	bool IsTransparent(aiMaterial* pMaterial)
	{
		float opacity;
		if(pMaterial->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS)
			return (opacity != 1.0);
		return false;
	}

	MeshInfo^ ProcessMesh(const aiScene* scene, aiMesh* mesh, std::map<aiMesh*, std::string>& meshNames)
	{
		// constant declaration
		const int nbBonesByVertex = 4;
		
		List<MeshBoneDefinition>^ bones = nullptr;
		bool hasSkinningPosition = false;
		bool hasSkinningNormal = false;
		int totalClusterCount = 0;

		auto scaling = Matrix::Scaling(ScaleImport);
		auto scalingInvert = scaling;
		scalingInvert.Invert();

		// Build the bone's indices/weights and attach bones to NodeData 
		//(bones info are present in the mesh so that is why we have to perform that here)
		std::vector<std::vector<std::pair<short, float> > > vertexIndexToBoneIdWeight;
		if(mesh->HasBones())
		{
			bones = gcnew List<MeshBoneDefinition>();

			// TODO: change this to support shared meshes across nodes

			// size of the array is already known
			vertexIndexToBoneIdWeight.resize(mesh->mNumVertices);

			// Build skinning clusters and fill controls points data stutcture
			for(unsigned int boneId = 0; boneId < mesh->mNumBones; ++boneId)
			{
				auto bone = mesh->mBones[boneId];

				// Fill controlPts with bone controls on the mesh
				for(unsigned int vtxWeightId=0; vtxWeightId<bone->mNumWeights; ++vtxWeightId)
				{
					auto vtxWeight = bone->mWeights[vtxWeightId];
					vertexIndexToBoneIdWeight[vtxWeight.mVertexId].push_back(std::make_pair(boneId, vtxWeight.mWeight));
				}

				// find the node where the bone is mapped - based on the name(?)
				int nodeIndex = -1;
				auto boneName = gcnew String(bone->mName.C_Str());
				for (int nodeDefId = 0; nodeDefId < nodes.Count; ++nodeDefId)
				{
					auto nodeDef = nodes[nodeDefId];
					if (nodeDef.Name->Equals(boneName))
					{
						nodeIndex = nodeDefId;
						break;
					}
				}
				if (nodeIndex == -1)
				{
					// TODO: log an error
					nodeIndex = 0;
				}

				MeshBoneDefinition boneDef;
				boneDef.NodeIndex = nodeIndex;
				auto bindPoseMatrix = aiMatrixToMatrix(bone->mOffsetMatrix);
				boneDef.LinkToMeshMatrix = scalingInvert * bindPoseMatrix * scaling;
				bones->Add(boneDef);
			}
			NormalizeVertexWeights(vertexIndexToBoneIdWeight, nbBonesByVertex);

			totalClusterCount = mesh->mNumBones;
			if (totalClusterCount > 0)
				hasSkinningPosition = true;
		}

		// Build the vertex declaration
		auto vertexElements = gcnew List<VertexElement>();
		int vertexStride = 0;

		int positionOffset = vertexStride;
		vertexElements->Add(VertexElement::Position<Vector3>(0, vertexStride));
		vertexStride += sizeof(Vector3);

		int normalOffset = vertexStride;
		if (mesh->HasNormals())
		{
			vertexElements->Add(VertexElement::Normal<Vector3>(0, vertexStride));
			vertexStride += sizeof(Vector3);
		}

		int uvOffset = vertexStride;
		const int sizeUV = sizeof(Vector2); // 3D uv not supported
		for (unsigned int uvChannel = 0; uvChannel < mesh->GetNumUVChannels(); ++uvChannel)
		{
			vertexElements->Add(VertexElement::TextureCoordinate<Vector2>(uvChannel, vertexStride));	
			vertexStride += sizeUV;
		}

		int colorOffset = vertexStride;
		const int sizeColor = sizeof(Color);
		for (unsigned int colorChannel=0; colorChannel< mesh->GetNumColorChannels(); ++colorChannel)
		{
			vertexElements->Add(VertexElement::Color<Color>(colorChannel, vertexStride));
			vertexStride += sizeColor;
		}

		int tangentOffset = vertexStride;
		if (mesh->HasTangentsAndBitangents())
		{
			vertexElements->Add(VertexElement::Tangent<Vector3>(0, vertexStride));
			vertexStride += sizeof(Vector3);
		}

		int bitangentOffset = vertexStride;
		if (mesh->HasTangentsAndBitangents())
		{
			vertexElements->Add(VertexElement::BiTangent<Vector3>(0, vertexStride));
			vertexStride += sizeof(Vector3);
		}

		int blendIndicesOffset = vertexStride;
		bool controlPointIndices16 = (AllowUnsignedBlendIndices && totalClusterCount > 256) || (!AllowUnsignedBlendIndices && totalClusterCount > 128);
		if (vertexIndexToBoneIdWeight.size() > 0)
		{
			if (controlPointIndices16)
			{
				if (AllowUnsignedBlendIndices)
				{
					vertexElements->Add(VertexElement("BLENDINDICES", 0, PixelFormat::R16G16B16A16_UInt, vertexStride));
					vertexStride += sizeof(unsigned short) * 4;
				}
				else
				{
					vertexElements->Add(VertexElement("BLENDINDICES", 0, PixelFormat::R16G16B16A16_SInt, vertexStride));
					vertexStride += sizeof(short) * 4;
				}
			}
			else
			{
				if (AllowUnsignedBlendIndices)
				{
					vertexElements->Add(VertexElement("BLENDINDICES", 0, PixelFormat::R8G8B8A8_UInt, vertexStride));
					vertexStride += sizeof(unsigned char) * 4;
				}
				else
				{
					vertexElements->Add(VertexElement("BLENDINDICES", 0, PixelFormat::R8G8B8A8_SInt, vertexStride));
					vertexStride += sizeof(char) * 4;
				}
			}
		}

		int blendWeightOffset = vertexStride;
		if (vertexIndexToBoneIdWeight.size() > 0)
		{
			vertexElements->Add(VertexElement("BLENDWEIGHT", 0, PixelFormat::R32G32B32A32_Float, vertexStride));
			vertexStride += sizeof(float) * 4;
		}

		// Build the vertices data buffer 
		auto vertexBuffer = gcnew array<Byte>(vertexStride * mesh->mNumVertices);
		pin_ptr<Byte> vbPointer = &vertexBuffer[0];
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		{
			auto position = aiVector3ToVector3(mesh->mVertices[i]);
			auto position4 = Vector4(position, 1.0f);
			auto newPosition = (Vector3)Vector4::Transform(position4, scaling);

			*((Vector3*)(vbPointer + positionOffset)) = newPosition;

			if (mesh->HasNormals())
				*((Vector3*)(vbPointer + normalOffset)) = aiVector3ToVector3(mesh->mNormals[i]);

			for (unsigned int uvChannel = 0; uvChannel < mesh->GetNumUVChannels(); ++uvChannel)
			{
				auto textureCoord = mesh->mTextureCoords[uvChannel][i];
				*((Vector2*)(vbPointer + uvOffset + sizeUV*uvChannel)) = Vector2(textureCoord.x, textureCoord.y);	// 3D uv not supported
			}
			
			for (unsigned int colorChannel=0; colorChannel< mesh->GetNumColorChannels(); ++colorChannel)
			{
				auto color = aiColor4ToColor(mesh->mColors[colorChannel][i]);
				*((Color*)(vbPointer + colorOffset + sizeColor*colorChannel)) = color;	
			}

			if (mesh->HasTangentsAndBitangents())
			{
				*((Vector3*)(vbPointer + tangentOffset)) = aiVector3ToVector3(mesh->mTangents[i]);
				*((Vector3*)(vbPointer + bitangentOffset)) = aiVector3ToVector3(mesh->mBitangents[i]);
			}

			if (vertexIndexToBoneIdWeight.size() > 0)
			{
				for(int bone=0; bone<nbBonesByVertex; ++bone)
				{
					if (controlPointIndices16)
					{
						if (AllowUnsignedBlendIndices)
							((unsigned short*)(vbPointer + blendIndicesOffset))[bone] = (unsigned short)vertexIndexToBoneIdWeight[i][bone].first;
						else
							((short*)(vbPointer + blendIndicesOffset))[bone] = (short)vertexIndexToBoneIdWeight[i][bone].first;
					}
					else
					{
						if (AllowUnsignedBlendIndices)
							((unsigned char*)(vbPointer + blendIndicesOffset))[bone] = (unsigned char)vertexIndexToBoneIdWeight[i][bone].first;
						else
							((char*)(vbPointer + blendIndicesOffset))[bone] = (char)vertexIndexToBoneIdWeight[i][bone].first;
					}
					((float*)(vbPointer + blendWeightOffset))[bone] = vertexIndexToBoneIdWeight[i][bone].second;
				}
			}
			vbPointer += vertexStride;
		}

		// Build the indices data buffer
		const int nbIndices = 3 * mesh->mNumFaces;
		array<Byte>^ indexBuffer;
		bool is32BitIndex = mesh->mNumVertices > 65535;
		if (is32BitIndex)
			indexBuffer = gcnew array<Byte>(sizeof(unsigned int) * nbIndices);
		else
			indexBuffer = gcnew array<Byte>(sizeof(unsigned short) * nbIndices);

		pin_ptr<Byte> ibPointer = &indexBuffer[0];
		for (unsigned int i = 0; i < mesh->mNumFaces; i++)
		{
			if (is32BitIndex)
			{
				for (int j = 0; j < 3; ++j)
				{
					*((unsigned int*)ibPointer) = mesh->mFaces[i].mIndices[j];
					ibPointer += sizeof(unsigned int);
				}
			}
			else
			{
				for (int j = 0; j < 3; ++j)
				{
					*((unsigned short*)ibPointer) = (unsigned short)(mesh->mFaces[i].mIndices[j]);
					ibPointer += sizeof(unsigned short);
				}
			}
		}

		// Build the mesh data
		auto vertexBufferBinding = VertexBufferBinding(GraphicsSerializerExtensions::ToSerializableVersion(gcnew BufferData(BufferFlags::VertexBuffer, vertexBuffer)), gcnew VertexDeclaration(vertexElements->ToArray()), mesh->mNumVertices, 0, 0);
		auto indexBufferBinding = gcnew IndexBufferBinding(GraphicsSerializerExtensions::ToSerializableVersion(gcnew BufferData(BufferFlags::IndexBuffer, indexBuffer)), is32BitIndex, nbIndices, 0);

		auto drawData = gcnew MeshDraw();
		auto vbb = gcnew List<VertexBufferBinding>();
		vbb->Add(vertexBufferBinding);
		drawData->VertexBuffers = vbb->ToArray();
		drawData->IndexBuffer = indexBufferBinding;
		drawData->PrimitiveType = PrimitiveType::TriangleList;
		drawData->DrawCount = nbIndices;

		auto meshInfo = gcnew MeshInfo();
		meshInfo->Draw = drawData;
		meshInfo->Name = gcnew String(meshNames[mesh].c_str());
		meshInfo->Bones = bones;
		meshInfo->MaterialIndex = mesh->mMaterialIndex;
		meshInfo->HasSkinningPosition = hasSkinningPosition;
		meshInfo->HasSkinningNormal = hasSkinningNormal;
		meshInfo->TotalClusterCount = totalClusterCount;
		
		return meshInfo;
	}

	// Register all the nodes in dictionnaries
	void RegisterNodes(aiNode* fromNode, int parentIndex, std::map<aiNode*, std::string>& nodeNames, std::map<int, std::vector<int>*>& meshIndexToNodeIndex)
	{
		int nodeIndex = nodes.Count;

		// assign the index of the node to the index of the mesh
		for(unsigned int m=0; m<fromNode->mNumMeshes; ++m)
		{
			int meshIndex = fromNode->mMeshes[m];

			if (meshIndexToNodeIndex.find(meshIndex) == meshIndexToNodeIndex.end())
				meshIndexToNodeIndex[meshIndex] = new std::vector<int>();
			meshIndexToNodeIndex[meshIndex]->push_back(nodeIndex);
		}

		// get node transformation
		aiVector3t<float> aiTranslation;
		aiVector3t<float> aiScaling;
		aiQuaterniont<float> aiOrientation;
		fromNode->mTransformation.Decompose(aiScaling, aiOrientation, aiTranslation);

		// Create node
		ModelNodeDefinition modelNodeDefinition;
		modelNodeDefinition.ParentIndex = parentIndex;
		modelNodeDefinition.Transform.Translation = Vector3(aiTranslation.x, aiTranslation.y, aiTranslation.z) * ScaleImport;
		modelNodeDefinition.Transform.Rotation = aiQuaternionToQuaternion(aiOrientation);
		
		if (parentIndex == -1)
			modelNodeDefinition.Transform.Scaling = Vector3::One;
		else
			modelNodeDefinition.Transform.Scaling = Vector3(aiScaling.x, aiScaling.y, aiScaling.z);
		
		modelNodeDefinition.Name = gcnew String(nodeNames[fromNode].c_str());
		modelNodeDefinition.Flags = ModelNodeFlags::Default;
		nodes.Add(modelNodeDefinition);

		// register the children
		for(unsigned int child=0; child<fromNode->mNumChildren; ++child)
		{
			RegisterNodes(fromNode->mChildren[child], nodeIndex, nodeNames, meshIndexToNodeIndex);
		}
	}

	// Register all the nodes in dictionnaries
	void RegisterNodes_old(aiNode* fromNode)
	{
		auto curNode = gcnew Entity();

		// the name
		curNode->Name = aiStringToString(fromNode->mName);

		// register the children
		for(unsigned int child=0; child<fromNode->mNumChildren; ++child)
			RegisterNodes_old(fromNode->mChildren[child]);

		// add the current node to the hash tables in order to be able to find it easily when processing attributes 
		if(!nodeNameToNodeData->ContainsKey(curNode->Name))
			nodeNameToNodeData->Add(curNode->Name, gcnew List<Entity^>());
		nodeNameToNodeData[curNode->Name]->Add(curNode);
		nodePtrToNodeData->Add((IntPtr)fromNode, curNode); 

		// add the meshes refered to the dictionnary (needed for bones animation in processMesh)
		for(unsigned int m=0; m<fromNode->mNumMeshes; ++m)
		{
			int index = fromNode->mMeshes[m];
			if(!meshIndexToReferingNodes->ContainsKey(index))
				meshIndexToReferingNodes->Add(index, gcnew List<Entity^>());
			meshIndexToReferingNodes[index]->Add(curNode);
		}
	}

	void ProcessAnimationCurveVector(AnimationClip^ animationClip, const aiVectorKey* keys, unsigned int nbKeys, String^ partialTargetName, double ticksPerSec, bool scaled)
	{
		auto scaling = Matrix::Scaling(ScaleImport);

		auto animationCurve = gcnew AnimationCurve<Vector3>();

		// Switch to cubic implicit interpolation mode for Vector3
		animationCurve->InterpolationType = AnimationCurveInterpolationType::Cubic;

		CompressedTimeSpan lastKeyTime;

		for(unsigned int keyId=0; keyId<nbKeys; ++keyId)
		{
			auto aiKey = keys[keyId];
			KeyFrameData<Vector3> key;

			auto time = aiTimeToPdxTimeSpan(aiKey.mTime, ticksPerSec);
			auto value = aiVector3ToVector3(aiKey.mValue);

			key.Time = time;
			lastKeyTime = time;
			
			key.Value.X = value.X;
			key.Value.Y = value.Y;
			key.Value.Z = value.Z;

			if(scaled) key.Value = (Vector3)Vector3::Transform(key.Value, scaling);

			animationCurve->KeyFrames->Add(key);
			if(keyId == 0 || keyId == nbKeys-1) // discontinuity at animation first and last frame
				animationCurve->KeyFrames->Add(key); // add 2 times the same frame at discontinuities to have null gradient
		}

		animationClip->AddCurve(partialTargetName, animationCurve);

		if (nbKeys > 0)
		{
			if (animationClip->Duration < lastKeyTime)
				animationClip->Duration = lastKeyTime;
		}
	}

	void ProcessAnimationCurveQuaternion(AnimationClip^ animationClip, const aiQuatKey* keys, unsigned int nbKeys, String^ partialTargetName, double ticksPerSec)
	{
		auto animationCurve = gcnew AnimationCurve<Quaternion>();

		CompressedTimeSpan lastKeyTime;

		for(unsigned int keyId=0; keyId<nbKeys; ++keyId)
		{
			auto aiKey = keys[keyId];
			KeyFrameData<Quaternion> key;
			
			auto time = aiTimeToPdxTimeSpan(aiKey.mTime, ticksPerSec);
			auto value = aiQuaternionToQuaternion(aiKey.mValue);
			
			key.Time = time;
			lastKeyTime = time;

			key.Value.X = value.X;
			key.Value.Y = value.Y;
			key.Value.Z = value.Z;
			key.Value.W = value.W;
			
			animationCurve->KeyFrames->Add(key);
		}

		animationClip->AddCurve(partialTargetName, animationCurve);

		if (nbKeys > 0)
		{
			if (animationClip->Duration < lastKeyTime)
				animationClip->Duration = lastKeyTime;
		}
	}
	
	void ProcessNodeAnimation(AnimationClip^ animationClip, const aiNodeAnim* nodeAnim, double ticksPerSec)
	{
		// Find the nodes on which the animation is performed
		auto nodeName = aiStringToString(nodeAnim->mNodeName);
		
		// The scales
		ProcessAnimationCurveVector(animationClip, nodeAnim->mScalingKeys, nodeAnim->mNumScalingKeys, String::Format("Transform.Scale[{0}]", nodeName), ticksPerSec, false);
		// The rotation
		ProcessAnimationCurveQuaternion(animationClip, nodeAnim->mRotationKeys, nodeAnim->mNumRotationKeys, String::Format("Transform.Rotation[{0}]", nodeName), ticksPerSec);
		// The translation
		ProcessAnimationCurveVector(animationClip, nodeAnim->mPositionKeys, nodeAnim->mNumPositionKeys, String::Format("Transform.Position[{0}]", nodeName), ticksPerSec, true);
	}

	AnimationClip^ ProcessAnimation(const aiScene* scene)
	{
		auto animationClip = gcnew AnimationClip();
		std::set<std::string> visitedNodeNames;

		for (unsigned int i = 0; i < scene->mNumAnimations; ++i)
		{
			auto aiAnim = scene->mAnimations[i];

			// Assimp animations have two different channels of animations ((1) on Nodes, (2) on Meshes).
			// Nevertheless the second one do not seems to be usable in assimp 3.0 so it will be ignored here.

			// name of the animation (dropped)
			auto animName = aiStringToString(aiAnim->mName); // used only be the logger

			// animation speed
			auto ticksPerSec = aiAnim->mTicksPerSecond;

			// animation using meshes (not supported)
			for(unsigned int meshAnimId = 0; meshAnimId<aiAnim->mNumMeshChannels; ++meshAnimId)
			{
				auto meshName = aiStringToString(aiAnim->mMeshChannels[meshAnimId]->mName);
				Logger->Warning("Mesh animation are not currently supported. Animation '{0}' on mesh {1} will be ignored", animName, meshName,
								CallerInfo::Get(__FILEW__, __FUNCTIONW__, __LINE__));
			}

			// animation on nodes
			for(unsigned int nodeAnimId=0; nodeAnimId<aiAnim->mNumChannels; ++nodeAnimId)
			{
				auto nodeAnim = aiAnim->mChannels[nodeAnimId];
				auto nodeName = std::string(nodeAnim->mNodeName.C_Str());
				if (visitedNodeNames.find(nodeName) == visitedNodeNames.end())
				{
					visitedNodeNames.insert(nodeName);
					ProcessNodeAnimation(animationClip, nodeAnim, ticksPerSec);
				}
				else
				{
					Logger->Error("Animation '{0}' uses two nodes with the same name ({1}). The animation cannot be resolved.", animName, aiStringToString(nodeAnim->mNodeName),
								  CallerInfo::Get(__FILEW__, __FUNCTIONW__, __LINE__));
					return nullptr;
				}
			}
		}
		return animationClip;
	}

	ComputeTextureColor^ GetTextureReferenceNode(String^ vfsOutputPath, String^ sourceTextureFile, int textureUVSetIndex, Vector2 textureUVscaling, bool wrapTextureU, bool wrapTextureV, MaterialAsset^ finalMaterial, SiliconStudio::Core::Diagnostics::Logger^ logger)
	{
		// TODO: compare with FBX importer - see if there could be some conflict between texture names
		auto textureValue = TextureLayerGenerator::GenerateMaterialTextureNode(vfsOutputPath, sourceTextureFile, textureUVSetIndex, textureUVscaling, wrapTextureU, wrapTextureV, Logger);

		auto attachedReference = AttachedReferenceManager::GetAttachedReference(textureValue->Texture);
		auto referenceName = attachedReference->Url;

		// find a new and correctName
		if (!textureNameCount->ContainsKey(referenceName))
			textureNameCount->Add(referenceName, 1);
		else
		{
			int count = textureNameCount[referenceName];
			textureNameCount[referenceName] = count + 1;
			referenceName = String::Concat(referenceName, "_", count);
		}

		return textureValue;
	}

	IComputeColor^ GenerateOneTextureTypeLayers(aiMaterial* pMat, aiTextureType textureType, int& textureCount, SiliconStudio::Paradox::Assets::Materials::MaterialAsset^ finalMaterial)
	{
		AssimpNet::Material::Stack^ stack = NetTranslation::Materials::convertAssimpStackCppToCs(pMat, textureType);
		int set;
		System::Collections::Stack^ compositionFathers = gcnew System::Collections::Stack();
		std::stack<int> sets;

		sets.push(0);
		auto nbTextures = pMat->GetTextureCount(textureType);
		IComputeColor^ curComposition = nullptr, ^ newCompositionFather = nullptr;
		IComputeColor^ curCompositionFather = nullptr;

		bool isRootElement = true;
		IComputeColor^ rootMaterial = nullptr;

		while (!stack->IsEmpty)
		{
			auto top = stack->Pop();

			if (!isRootElement)
			{
				if (compositionFathers->Count == 0)
					Logger->Error(String::Format("Texture Stack Invalid : Operand without Operation."));

				curCompositionFather = (IComputeColor^)compositionFathers->Pop();
			}

			set = sets.top();
			sets.pop();
			auto type = top->type;
			auto strength = top->blend;
			auto alpha = top->alpha;
			
			if (type == AssimpNet::Material::StackType::Operation)
			{
				auto realTop = (AssimpNet::Material::StackOperation^) top;
				AssimpNet::Material::Operation op = realTop->operation;
				auto binNode = gcnew ComputeBinaryColor(nullptr, nullptr, BinaryOperator::Add);

				switch (op)
				{
					case AssimpNet::Material::Operation::Add3ds:
					case AssimpNet::Material::Operation::AddMaya:
						binNode->Operator = BinaryOperator::Add; //BinaryOperator::Add3ds;
						break;
					case AssimpNet::Material::Operation::Multiply3ds:
					case AssimpNet::Material::Operation::MultiplyMaya:
						binNode->Operator = BinaryOperator::Multiply;
						break;
					default:
						binNode->Operator = BinaryOperator::Add;
						break;
				}

				curComposition = binNode;
			}
			else if (type == AssimpNet::Material::StackType::Color)
			{
				auto realTop = (AssimpNet::Material::StackColor^)top;
				Color3 col = realTop->color;
				curComposition = gcnew ComputeColor(Color4(col.R, col.G, col.B, alpha));
			}
			else if (type == AssimpNet::Material::StackType::Texture)
			{
				auto realTop = (AssimpNet::Material::StackTexture^)top;
				String ^texPath = realTop->texturePath;
				int indexUV = realTop->channel;
				auto textureValue = GetTextureReferenceNode(vfsOutputFilename, texPath, indexUV, Vector2::One, false, false, finalMaterial, Logger);
				curComposition = textureValue;
			}
			
			newCompositionFather = curComposition;
			
			if (strength != 1.f)
			{
				float strengthAlpha = strength;
				if (type != AssimpNet::Material::StackType::Color)
					strengthAlpha *= alpha;
				
				
				auto factorComposition = gcnew ComputeFloat4(Vector4(strength, strength, strength, strengthAlpha));
				curComposition = gcnew ComputeBinaryColor(curComposition, factorComposition, BinaryOperator::Multiply);
			}
			else if (alpha != 1.f && type != AssimpNet::Material::StackType::Color)
			{
				auto factorComposition = gcnew ComputeFloat4(Vector4(1.0f, 1.0f, 1.0f, alpha));
				curComposition = gcnew ComputeBinaryColor(curComposition, factorComposition, BinaryOperator::Multiply);
			}

			if (isRootElement)
			{
				rootMaterial = curComposition;
				isRootElement = false;
				compositionFathers->Push(curCompositionFather);
			}
			else
			{
				if (set == 0)
				{
					((ComputeBinaryColor^)curCompositionFather)->LeftChild = curComposition;
					compositionFathers->Push(curCompositionFather);
					sets.push(1);
				}
				else if (set == 1)
				{
					((ComputeBinaryColor^)curCompositionFather)->RightChild = curComposition;
				}
				else
				{
					Logger->Error(String::Format("Texture Stack Invalid : Invalid Operand Number {0}.", set));
				}	
			}

			if (type == AssimpNet::Material::StackType::Operation)
			{
				compositionFathers->Push(newCompositionFather);
				sets.push(0);
			}
		}

		return rootMaterial;
	}

	void BuildLayeredSurface(aiMaterial* pMat, bool hasBaseColor, bool hasBaseValue, Color4 baseColor, float baseValue, aiTextureType textureType, SiliconStudio::Paradox::Assets::Materials::MaterialAsset^ finalMaterial)
	{
		auto nbTextures = pMat->GetTextureCount(textureType);
		
		IComputeColor^ computeColorNode;
		int textureCount = 0;
		if (nbTextures == 0)
		{
			if (hasBaseColor)
			{
				computeColorNode = gcnew ComputeColor(baseColor);
			}
			//else if (hasBaseValue)
			//{
			//	computeColorNode = gcnew MaterialFloatComputeNode(baseValue);
			//}
		}
		else
		{
			computeColorNode = GenerateOneTextureTypeLayers(pMat, textureType, textureCount, finalMaterial);
		}

		if (computeColorNode == nullptr)
		{
			return;
		}
		
		if (textureType == aiTextureType_DIFFUSE)
		{
			if (pMat->GetTextureCount(aiTextureType_LIGHTMAP) > 0)
			{
				auto lightMap = GenerateOneTextureTypeLayers(pMat, aiTextureType_LIGHTMAP, textureCount, finalMaterial);
				if (lightMap != nullptr)
					computeColorNode = gcnew ComputeBinaryColor(computeColorNode, lightMap, BinaryOperator::Add);
			}

			finalMaterial->Attributes->Diffuse = gcnew MaterialDiffuseMapFeature(computeColorNode);

			// TODO TEMP: Set a default diffuse model
			finalMaterial->Attributes->DiffuseModel = gcnew MaterialDiffuseLambertModelFeature();
		}
		else if (textureType == aiTextureType_SPECULAR)
		{
			auto specularFeature = gcnew MaterialSpecularMapFeature();
			specularFeature->SpecularMap = computeColorNode;
			finalMaterial->Attributes->Specular = specularFeature;

			// TODO TEMP: Set a default specular model
			auto specularModel = gcnew MaterialSpecularMicrofacetModelFeature();
			specularModel->Fresnel = gcnew MaterialSpecularMicrofacetFresnelSchlick();
			specularModel->Visibility = gcnew MaterialSpecularMicrofacetVisibilityImplicit();
			specularModel->NormalDistribution = gcnew MaterialSpecularMicrofacetNormalDistributionBlinnPhong();
			finalMaterial->Attributes->SpecularModel = specularModel;
		}
		else if (textureType == aiTextureType_EMISSIVE)
		{
			// TODO: Add support
		}
		else if (textureType == aiTextureType_AMBIENT)
		{
			// TODO: Add support
		}
		else if (textureType == aiTextureType_REFLECTION)
		{
			// TODO: Add support
		}
		if (textureType == aiTextureType_OPACITY)
		{
			// TODO: Add support
		}
		else if (textureType == aiTextureType_SHININESS)
		{
			// TODO: Add support
		}
		if (textureType == aiTextureType_SPECULAR)
		{
			// TODO: Add support
		}
		else if (textureType == aiTextureType_NORMALS)
		{
			finalMaterial->Attributes->Surface = gcnew MaterialNormalMapFeature(computeColorNode);
		}
		else if (textureType == aiTextureType_DISPLACEMENT)
		{
			// TODO: Add support
		}
		else if (textureType == aiTextureType_SHININESS)
		{
			// TODO: Add support
		}
		else if (textureType == aiTextureType_HEIGHT)
		{
			// TODO: Add support
		}
	}
	
	MaterialAsset^ ProcessMeshMaterial(aiMaterial* pMaterial)
	{
		auto finalMaterial = gcnew MaterialAsset();

		// Set material specular components
		float specIntensity;
		if (AI_SUCCESS == pMaterial->Get(AI_MATKEY_SHININESS_STRENGTH, specIntensity))
		{
			if (specIntensity > 0)
			{
				// TODO: Add Specular Intensity
				//auto specularIntensityMap = gcnew MaterialFloatComputeColor(specIntensity);
				//specularIntensityMap->Key = MaterialKeys::SpecularIntensity;
				//specularIntensityMap->AutoAssignKey = false;
				//specularIntensityMap->IsReducible = false;
				//finalMaterial->AddColorNode(MaterialParameters::SpecularIntensityMap, "specularIntensity", specularIntensityMap);
			}
		}

		//// ---------------------------------------------------------------------------------
  //      // Iterate on all custom Paradox Properties and add them to the mesh.
  //      // Key must be in the format: Paradox_KeyName
  //      // ---------------------------------------------------------------------------------
		//for(unsigned int i = 0; i<pMaterial->mNumProperties; ++i)
		//{
		//	auto pProp = pMaterial->mProperties[i];
		//	auto propertyName = aiStringToString(pProp->mKey);
  //          if (propertyName->StartsWith("PX_")) 
		//	{
  //              int index = propertyName->IndexOf('_');
  //              propertyName = propertyName->Substring(index);
  //              propertyName = propertyName->Replace('_','.');
  //              // TODO Paradox Change name 
  //              propertyName = gcnew String("SiliconStudio.Paradox.Rendering") + propertyName;

		//		switch (pProp->mDataLength)
		//		{
  //                  case sizeof(double):
  //                      {
		//					auto value = *((double*)pProp->mData);
//							ParameterKey<float>^ key = gcnew ParameterKey<float>(propertyName, 1);
  //                          finalMaterial->SetParameter(key, (float)value);
  //                      }
  //                      break;
  //                  case 3*sizeof(double):
  //                      {
  //                          auto value = (double*)pProp->mData;
//                            ParameterKey<Vector3>^ key = gcnew ParameterKey<Vector3>(propertyName, 1);
  //                          finalMaterial->SetParameter(key, Vector3((float)value[0], (float)value[1], (float)value[2]));
  //                      }
  //                      break;
  //                  default:
  //                      Console::WriteLine("Warning, Type for property [{0}] is not supported", propertyName);
  //                      break;
  //              }
  //          }
		//}

		// Build the material Diffuse, Specular, NormalMap and DisplacementColor surfaces.
		aiColor3D color;
		Color4 diffColor;
		Color4 specColor;
		Color4 ambientColor;
		Color4 emissiveColor;
		Color4 reflectiveColor;
		Color4 dummyColor;
		float specPower;
		float opacity;

		bool hasDiffColor = false;
		bool hasSpecColor = false;
		bool hasAmbientColor = false;
		bool hasEmissiveColor = false;
		bool hasReflectiveColor = false;
		bool hasSpecPower = false;
		bool hasOpacity = false;

		if(pMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) // always keep black color for diffuse
		{
			diffColor = aiColor3ToColor4(color);
			hasDiffColor = true;
		}
		if(pMaterial->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS && IsNotBlackColor(color))
		{
			specColor = aiColor3ToColor4(color);
			hasSpecColor = true;
		}
		if(pMaterial->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS && IsNotBlackColor(color))
		{
			ambientColor = aiColor3ToColor4(color);
			hasAmbientColor = true;
		}
		if(pMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS && IsNotBlackColor(color))
		{
			emissiveColor = aiColor3ToColor4(color);
			hasEmissiveColor = true;
		}
		if(pMaterial->Get(AI_MATKEY_COLOR_REFLECTIVE, color) == AI_SUCCESS && IsNotBlackColor(color))
		{
			reflectiveColor = aiColor3ToColor4(color);
			hasReflectiveColor = true;
		}

		hasSpecPower = (AI_SUCCESS == pMaterial->Get(AI_MATKEY_SHININESS, specPower) && specPower > 0);
		if(pMaterial->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS && opacity < 1.0)
		{
			finalMaterial->Parameters->Set(MaterialKeys::HasTransparency, true);
			hasOpacity = true;
		}

		BuildLayeredSurface(pMaterial, hasDiffColor, false, diffColor, 0.0f, aiTextureType_DIFFUSE, finalMaterial);
		BuildLayeredSurface(pMaterial, hasSpecColor, false, specColor, 0.0f, aiTextureType_SPECULAR, finalMaterial);
		BuildLayeredSurface(pMaterial, false, false, dummyColor, 0.0f, aiTextureType_NORMALS, finalMaterial);
		BuildLayeredSurface(pMaterial, false, false, dummyColor, 0.0f, aiTextureType_DISPLACEMENT, finalMaterial);
		BuildLayeredSurface(pMaterial, hasAmbientColor, false, ambientColor, 0.0f, aiTextureType_AMBIENT, finalMaterial);
		BuildLayeredSurface(pMaterial, false, hasOpacity, dummyColor, opacity, aiTextureType_OPACITY, finalMaterial);
		BuildLayeredSurface(pMaterial, false, hasSpecPower, dummyColor, specPower, aiTextureType_SHININESS, finalMaterial);
		BuildLayeredSurface(pMaterial, hasEmissiveColor, false, emissiveColor, 0.0f, aiTextureType_EMISSIVE, finalMaterial);
		BuildLayeredSurface(pMaterial, false, false, dummyColor, 0.0f, aiTextureType_HEIGHT, finalMaterial);
		BuildLayeredSurface(pMaterial, hasReflectiveColor, false, reflectiveColor, 0.0f, aiTextureType_REFLECTION, finalMaterial);

		return finalMaterial;
	}

	bool IsNotBlackColor(aiColor3D color)
	{
		return color.r != 0 || color.g != 0 || color.b != 0;
	}
	
	template<class T>
	void GenerateUniqueNames(std::map<T*, std::string>& finalNames, std::vector<std::string>& baseNames, T** objectsToName)
	{
		std::map<std::string, int> itemNameTotalCount;
		std::map<std::string, int> itemNameCurrentCount;
		std::vector<std::string> tempNames;

		for (int i = 0; i < baseNames.size(); ++i)
		{
			// Clean the name by removing unwanted characters
			T* lItem = objectsToName[i];
			std::string itemName = baseNames[i];

			auto itemPart = std::string();

			int itemNameSplitPosition = itemName.find('#');
			if (itemNameSplitPosition != std::string::npos)
			{
				itemPart = itemName.substr(itemNameSplitPosition + 1);
				itemName = itemName.substr(0, itemNameSplitPosition);
			}

			itemNameSplitPosition = itemNameSplitPosition = itemName.find("__");
			if (itemNameSplitPosition != std::string::npos)
			{
				itemPart = itemName.substr(itemNameSplitPosition + 2);
				itemName = itemName.substr(0, itemNameSplitPosition);
			}

			// remove all bad characters
			ReplaceCharacter(itemName, ':', '_');
			RemoveCharacter(itemName, ' ');
			tempNames.push_back(itemName);

			// count the occurences of this name
			if (itemNameTotalCount.count(itemName) == 0)
				itemNameTotalCount[itemName] = 1;
			else
				itemNameTotalCount[itemName] = itemNameTotalCount[itemName] + 1;
		}

		for (int i = 0; i < baseNames.size(); ++i)
		{
			T* lItem = objectsToName[i];
			auto itemName = tempNames[i];
			int currentCount = 0;

			if (itemNameTotalCount[itemName] > 1)
			{
				if (itemNameCurrentCount.count(itemName) == 0)
					itemNameCurrentCount[itemName] = 1;
				else
					itemNameCurrentCount[itemName] = itemNameCurrentCount[itemName] + 1;

				itemName = itemName + "_" + std::to_string(itemNameCurrentCount[itemName]);
			}

			finalNames[lItem] = itemName;
		}
	}

	void GenerateMaterialNames(const aiScene* scene, std::map<aiMaterial*, std::string>& materialNames)
	{
		std::vector<std::string> baseNames;
		for (uint32_t i = 0;  i < scene->mNumMaterials; i++)
		{
			auto lMaterial = scene->mMaterials[i];
			
			std::string materialName = "";
			aiString aiName;
			if (lMaterial->Get(AI_MATKEY_NAME, aiName) == AI_SUCCESS)
				materialName = std::string(aiName.C_Str());
			baseNames.push_back(materialName);
		}

		GenerateUniqueNames(materialNames, baseNames, scene->mMaterials);
	}

	void GenerateMeshNames(const aiScene* scene, std::map<aiMesh*, std::string>& meshNames)
	{
		std::vector<std::string> baseNames;
		for (uint32_t i = 0; i < scene->mNumMeshes; i++)
		{
			auto lMesh = scene->mMeshes[i];
			std::string meshName = std::string(lMesh->mName.C_Str());
			baseNames.push_back(meshName);
		}

		GenerateUniqueNames(meshNames, baseNames, scene->mMeshes);
	}

	void GenerateAnimationNames(const aiScene* scene, std::map<aiAnimation*, std::string>& animationNames)
	{
		std::vector<std::string> baseNames;
		for (uint32_t i = 0; i < scene->mNumAnimations; i++)
		{
			auto lAnimation = scene->mAnimations[i];
			std::string animationName = std::string(lAnimation->mName.C_Str());
			baseNames.push_back(animationName);
		}

		GenerateUniqueNames(animationNames, baseNames, scene->mAnimations);
	}

	void GetNodeNames(aiNode* node, std::vector<std::string>& nodeNames, std::vector<aiNode*>& orderedNodes)
	{
		nodeNames.push_back(std::string(node->mName.C_Str()));
		orderedNodes.push_back(node);
		for (uint32_t i = 0; i < node->mNumChildren; ++i)
			GetNodeNames(node->mChildren[i], nodeNames, orderedNodes);
	}

	void GenerateNodeNames(const aiScene* scene, std::map<aiNode*, std::string>& nodeNames)
	{
		std::vector<std::string> baseNames;
		std::vector<aiNode*> orderedNodes;
		GetNodeNames(scene->mRootNode, baseNames, orderedNodes);

		// Need to create the array of the nodes
		int nodeCount = orderedNodes.size();
		aiNode** nodeArray = new aiNode*[nodeCount];
		for (int i = 0; i < nodeCount; ++i)
			nodeArray[i] = orderedNodes[i];

		GenerateUniqueNames(nodeNames, baseNames, nodeArray);

		delete[] nodeArray;
	}
	
	List<String^>^ ExtractTextureDependencies(const aiScene *scene)
	{
		auto textureNames = gcnew List<String^>();
		
		// get internal textures (not supported)
		/*for(int i=0; i<scene->mNumTextures; ++i)
		{
			auto texture  = scene->mTextures[i];

			if(texture == nullptr)
				continue;
			
			//TODO: add material name
			std::string baseName = "GenTexture";
			baseName.append(std::to_string(i));
			
			textureNames->Add(gcnew String(baseName.c_str()));
		}*/
		
		// texture search is done by type so we need to loop on them
		std::vector<aiTextureType> allTextureTypes;
		allTextureTypes.push_back(aiTextureType_DIFFUSE);
		allTextureTypes.push_back(aiTextureType_SPECULAR);
		allTextureTypes.push_back(aiTextureType_AMBIENT);
		allTextureTypes.push_back(aiTextureType_EMISSIVE);
		allTextureTypes.push_back(aiTextureType_HEIGHT);
		allTextureTypes.push_back(aiTextureType_NORMALS);
		allTextureTypes.push_back(aiTextureType_SHININESS);
		allTextureTypes.push_back(aiTextureType_OPACITY);
		allTextureTypes.push_back(aiTextureType_DISPLACEMENT);
		allTextureTypes.push_back(aiTextureType_LIGHTMAP);
		allTextureTypes.push_back(aiTextureType_REFLECTION);
		//allTextureTypes.push_back(aiTextureType_NONE);
		//allTextureTypes.push_back(aiTextureType_UNKNOWN);
		
		for (uint32_t i = 0; i < scene->mNumMaterials; i++)
		{
			for (std::vector<aiTextureType>::iterator it = allTextureTypes.begin(); it != allTextureTypes.end(); ++it)
			{
				auto lMaterial = scene->mMaterials[i];
				auto nbTextures = lMaterial->GetTextureCount(*it);
				for (uint32_t j = 0; j < nbTextures; ++j)
				{
					aiString path;
					aiTextureMapping mapping;
					unsigned int index;
					float blend;
					aiTextureOp textureOp;
					aiTextureMapMode mapMode;
					if (AI_SUCCESS == lMaterial->GetTexture(*it, 0, &path, &mapping, &index, &blend, &textureOp, &mapMode))
					{
						auto relFileName = gcnew String(path.C_Str());
						String^ fileNameToUse = Path::Combine(vfsInputPath, relFileName);
						textureNames->Add(fileNameToUse);
						break;
					}
				}
			}
		}

		return textureNames;
	}

	Dictionary<String^, MaterialAsset^>^ ExtractMaterials(const aiScene *scene, std::map<aiMaterial*, std::string>& materialNames)
	{
		GenerateMaterialNames(scene, materialNames);
		
		auto materials = gcnew Dictionary<String^, MaterialAsset^>();
		for (uint32_t i = 0; i < scene->mNumMaterials; i++)
		{
			std::map<std::string, int> dict;
			auto lMaterial = scene->mMaterials[i];
			auto materialName = materialNames[lMaterial];
			materials->Add(gcnew String(materialName.c_str()), ProcessMeshMaterial(lMaterial));
		}
		return materials;
	}

	String^ SearchMeshNode(aiNode* node, unsigned int meshIndex, std::map<aiNode*, std::string>& nodeNames)
	{
		for (uint32_t i = 0; i < node->mNumMeshes; ++i)
		{
			if (node->mMeshes[i] == meshIndex)
				return gcnew String(nodeNames[node].c_str());
		}

		for (uint32_t i = 0; i < node->mNumChildren; ++i)
		{
			auto res = SearchMeshNode(node->mChildren[i], meshIndex, nodeNames);
			if (res != nullptr)
				return res;
		}

		return nullptr;
	}

	List<MeshParameters^>^ ExtractModel(const aiScene* scene, std::map<aiMesh*, std::string>& meshNames, std::map<aiMaterial*, std::string>& materialNames, std::map<aiNode*, std::string>& nodeNames)
	{
		GenerateMeshNames(scene, meshNames);
		
		auto meshList = gcnew List<MeshParameters^>();
		for (uint32_t i = 0; i < scene->mNumMeshes; ++i)
		{
			auto mesh = scene->mMeshes[i];
			auto meshParams = gcnew MeshParameters();
			meshParams->MeshName = gcnew String(meshNames[mesh].c_str());
			auto lMaterial = scene->mMaterials[mesh->mMaterialIndex];
			meshParams->MaterialName = gcnew String(materialNames[lMaterial].c_str());
			meshParams->NodeName = SearchMeshNode(scene->mRootNode, i, nodeNames);
			meshList->Add(meshParams);
		}
		return meshList;
	}

	List<String^>^ ExtractAnimations(const aiScene* scene, std::map<aiAnimation*, std::string>& animationNames)
	{
		if (scene->mNumAnimations == 0)
			return nullptr;

		GenerateAnimationNames(scene, animationNames);
		
		auto animationList = gcnew List<String^>();
		for (std::map<aiAnimation*, std::string>::iterator it = animationNames.begin(); it != animationNames.end(); ++it)
		{
			animationList->Add(gcnew String(it->second.c_str()));
		}
		return animationList;
	}
	
	Model^ ConvertAssimpScene(const aiScene *scene)
	{
		modelData = gcnew Model();
		modelData->Hierarchy = gcnew ModelViewHierarchyDefinition();

		std::map<aiMesh*, std::string> meshNames;
		GenerateMeshNames(scene, meshNames);

		std::map<aiNode*, std::string> nodeNames;
		GenerateNodeNames(scene, nodeNames);

		// register the nodes and fill hierarchy
		std::map<int, std::vector<int>*> meshIndexToNodeIndex;

		RegisterNodes(scene->mRootNode, -1, nodeNames, meshIndexToNodeIndex);
		modelData->Hierarchy->Nodes = nodes.ToArray();

		// meshes
		for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
		{
			if (meshIndexToNodeIndex.find(i) != meshIndexToNodeIndex.end())
			{
				auto meshInfo = ProcessMesh(scene, scene->mMeshes[i], meshNames);

				for (std::vector<int>::iterator nodeIndexPtr = meshIndexToNodeIndex[i]->begin(); nodeIndexPtr != meshIndexToNodeIndex[i]->end(); ++nodeIndexPtr)
				{
					auto nodeMeshData = gcnew Mesh();
					nodeMeshData->Draw = meshInfo->Draw;
					nodeMeshData->Name = meshInfo->Name;
					nodeMeshData->MaterialIndex = meshInfo->MaterialIndex;
					nodeMeshData->NodeIndex = *nodeIndexPtr;

					if (meshInfo->Bones != nullptr)
					{
						nodeMeshData->Skinning = gcnew MeshSkinningDefinition();
						nodeMeshData->Skinning->Bones = meshInfo->Bones->ToArray();
					}
					if (meshInfo->HasSkinningPosition || meshInfo->HasSkinningNormal || meshInfo->TotalClusterCount > 0)
					{
						if (meshInfo->HasSkinningPosition)
							nodeMeshData->Parameters->Set(MaterialKeys::HasSkinningPosition, true);
						if (meshInfo->HasSkinningNormal)
							nodeMeshData->Parameters->Set(MaterialKeys::HasSkinningNormal, true);
						if (meshInfo->TotalClusterCount > 0)
							nodeMeshData->Parameters->Set(MaterialKeys::SkinningBones, meshInfo->TotalClusterCount);
					}
					modelData->Meshes->Add(nodeMeshData);
				}
			}
		}

		// delete the vectors in the map
		for (std::map<int, std::vector<int>*>::iterator it = meshIndexToNodeIndex.begin(); it != meshIndexToNodeIndex.end(); ++it)
		{
			delete it->second;
			it->second = 0;
		}

		// embedded texture - only to log the warning for now
		for (unsigned int i = 0; i < scene->mNumTextures; ++i)
            ExtractEmbededTexture(scene->mTextures[i]);

		return modelData;
	}

	void GetNodes(aiNode* node, int depth, std::map<aiNode*, std::string>& nodeNames, List<NodeInfo^>^ allNodes)
	{
		auto currentIndex = allNodes->Count;
		auto newNodeInfo = gcnew NodeInfo();
		newNodeInfo->Name = gcnew String(nodeNames[node].c_str());
		newNodeInfo->Depth = depth;
		newNodeInfo->Preserve = false;

		allNodes->Add(newNodeInfo);
		for (uint32_t i = 0; i < node->mNumChildren; ++i)
			GetNodes(node->mChildren[i], depth + 1, nodeNames, allNodes);
	}

	Vector3 GetUpAxis(aiNode* rootNode)
	{
		if (rootNode != 0)
		{
			// get node transformation
			aiVector3t<float> aiTranslation;
			aiVector3t<float> aiScaling;
			aiQuaterniont<float> aiOrientation;
			rootNode->mTransformation.Decompose(aiScaling, aiOrientation, aiTranslation);
			if (aiOrientation.x != 0)
				return Vector3::UnitZ;
			if (aiOrientation.z != 0)
				return Vector3::UnitX;
			return Vector3::UnitY;
		}
		return Vector3::UnitZ;
	}

	List<NodeInfo^>^ ExtractNodeHierarchy(const aiScene *scene, std::map<aiNode*, std::string>& nodeNames)
	{
		auto allNodes = gcnew List<NodeInfo^>();
		GetNodes(scene->mRootNode, 0, nodeNames, allNodes);
		return allNodes;
	}

public:
	EntityInfo^ ExtractEntity(String^ inputFilename, String^ outputFilename)
	{
		try
		{
			// the importer is kept here since it owns the scene object.
			//TODO: check the options
			Assimp::Importer importer;
			auto scene = Initialize(inputFilename, outputFilename, &importer,
				aiProcess_SortByPType
					| aiProcess_RemoveRedundantMaterials);

			std::map<aiMaterial*, std::string> materialNames;
			std::map<aiMesh*, std::string> meshNames;
			std::map<aiAnimation*, std::string> animationNames;
			std::map<aiNode*, std::string> nodeNames;
		
			GenerateNodeNames(scene, nodeNames);

			auto entityInfo = gcnew EntityInfo();
			entityInfo->TextureDependencies = ExtractTextureDependencies(scene);
			entityInfo->Materials = ExtractMaterials(scene, materialNames);
			entityInfo->Models = ExtractModel(scene, meshNames, materialNames, nodeNames);
			entityInfo->Nodes = ExtractNodeHierarchy(scene, nodeNames);
			entityInfo->AnimationNodes = ExtractAnimations(scene, animationNames);
			
			return entityInfo;
		}
		catch (Exception^)
		{
			return nullptr;
		}
	}

	Model^ Convert(String^ inputFilename, String^ outputFilename)
	{
		// the importer is kept here since it owns the scene object.
		Assimp::Importer importer;
		auto scene = Initialize(inputFilename, outputFilename, &importer, 
			aiProcess_CalcTangentSpace
				| aiProcess_RemoveRedundantMaterials
                | aiProcess_Triangulate
				| aiProcess_GenNormals 
                | aiProcess_JoinIdenticalVertices
                | aiProcess_LimitBoneWeights
                | aiProcess_SortByPType
				| aiProcess_FlipWindingOrder
				| aiProcess_FlipUVs );
		return ConvertAssimpScene(scene);
	}

	AnimationClip^ ConvertAnimation(String^ inputFilename, String^ outputFilename)
	{
		// the importer is kept here since it owns the scene object.
		Assimp::Importer importer;
		auto scene = Initialize(inputFilename, outputFilename, &importer, 0);
		return ProcessAnimation(scene);
	}
};

}}}}
