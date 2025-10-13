#ifndef __PYENGINE_2_0_MESH_CPU__
#define __PYENGINE_2_0_MESH_CPU__

#define NOMINMAX
// API Abstraction
#include "PrimeEngine/APIAbstraction/APIAbstractionDefines.h"

// Outer-Engine includes

// Inter-Engine includes
#include "PrimeEngine/MemoryManagement/Handle.h"
#include "PrimeEngine/PrimitiveTypes/PrimitiveTypes.h"
#include "PrimeEngine/FileSystem/FileReader.h"
#include "PrimeEngine/Utils/StringOps.h"
#include "../../Utils/Array/Array.h"
#include "PrimeEngine/MainFunction/MainFunctionArgs.h"
#include "PrimeEngine/APIAbstraction/Texture/SamplerState.h"

#include "../PositionBufferCPU/PositionBufferCPU.h"
#include "../TexCoordBufferCPU/TexCoordBufferCPU.h"
#include "../IndexBufferCPU/IndexBufferCPU.h"
#include "../NormalBufferCPU/NormalBufferCPU.h"

// Sibling/Children includes

namespace PE {

	struct AABB {
		Vector3 center;
		Vector3 extents;
		Vector3 min() const { return center - extents; }
		Vector3 max() const { return center + extents; }
	};

// This class is a simple POD struct that holds all the CPU information about the mesh before it is given to DX to be put in GPU memory
struct MeshCPU : PE::PEAllocatableAndDefragmentable
{
	MeshCPU(PE::GameContext &context, PE::MemoryArena arena) 
	: m_manualBufferManagement(false)
	, m_hAdditionalVertexBuffersCPU(context, arena)
	, m_hAdditionalTexCoordBuffersCPU(context, arena)
	, m_hAdditionalNormalBuffersCPU(context, arena)
	, m_hAdditionalTangentBuffersCPU(context, arena)
	{
		m_arena = arena; m_pContext = &context;
	}

	// Reads the specified buffer from file
	void ReadMesh(const char *filename, const char *package, const char *tag);
	
	void createEmptyMesh();
	void createBillboardMesh();
	void createBillboardMeshWithColorTexture(const char *textureFilename , const char *package, PrimitiveTypes::Float32 w = 1000.0f, PrimitiveTypes::Float32 h = 1000.0f, ESamplerState customSamplerState = SamplerState_Count);
	void createBillboardMeshWithColorGlowTextures(const char *colorTextureFilename, const char *glowTextureFilename , const char *package,
		PrimitiveTypes::Float32 w, PrimitiveTypes::Float32 h);

	Handle m_hPositionBufferCPU;
	Array<Handle> m_hAdditionalVertexBuffersCPU;
	
	Handle m_hIndexBufferCPU;

	Handle m_hTexCoordBufferCPU;
	Array<Handle> m_hAdditionalTexCoordBuffersCPU;
	
	Handle m_hNormalBufferCPU;
	Array<Handle> m_hAdditionalNormalBuffersCPU;
	
	Handle m_hTangentBufferCPU;
	Array<Handle> m_hAdditionalTangentBuffersCPU;
	

	Handle m_hColorBufferCPU;

	Handle m_hMaterialSetCPU;

	Handle m_hSkinWeightsCPU;

	PrimitiveTypes::Bool m_manualBufferManagement; // if true, this mesh wont be cached and reused

	PE::MemoryArena m_arena; PE::GameContext *m_pContext;

public:
	// read-only access + two builders for local AABB
	const AABB& localAABB() const { return m_localAABB; }
	bool hasAABB() const { return m_aabbValid; }

	// Generic AABB builder (row pointer + count)
	void buildLocalAABBFromPositions(const Vector3* positions, size_t count);

	// pull from MeshCPU's own position buffer
	void buildLocalAABBFromMeshBuffer();

	// Debug visualization
	void createAABBDebugLines(Vector3* lineData, int& lineCount) const;

private:
	AABB m_localAABB;
	bool m_aabbValid{ false };
};

}; // namespace PE {
#endif
