#ifndef __PYENGINE_2_0_MESH_H__
#define __PYENGINE_2_0_MESH_H__

#define NOMINMAX
// API Abstraction
#include "PrimeEngine/APIAbstraction/APIAbstractionDefines.h"

// Outer-Engine includes
#include <assert.h>

// Inter-Engine includes
#include "PrimeEngine/MemoryManagement/Handle.h"
#include "PrimeEngine/PrimitiveTypes/PrimitiveTypes.h"
#include "../Events/Component.h"
#include "../Utils/Array/Array.h"
#include "../Geometry/MeshCPU/MeshCPU.h"
#include "../Math/Matrix4x4.h"

#include "PrimeEngine/APIAbstraction/GPUBuffers/VertexBufferGPU.h"
#include "PrimeEngine/APIAbstraction/GPUBuffers/IndexBufferGPU.h"

#include "PrimeEngine/APIAbstraction/Effect/Effect.h"

// Sibling/Children includes

namespace PE {
struct MaterialSetCPU;
namespace Components {

struct Mesh : public Component
{
	PE_DECLARE_CLASS(Mesh);

	// Constructor -------------------------------------------------------------
	Mesh(PE::GameContext &context, PE::MemoryArena arena, Handle hMyself);

	virtual ~Mesh(){}

	virtual void addDefaultComponents();

	// need this to maintain m_instances
	virtual void addComponent(Handle hComponent, int *pAllowedEvents = NULL);
	virtual void removeComponent(int index);


	// Methods -----------------------------------------------------------------

	// Builds a Mesh from the data in system memory
	void loadFromMeshCPU_needsRC(MeshCPU &mcpu, int &threadOwnershipMask);
	EPEVertexFormat updateGeoFromMeshCPU_needsRC(MeshCPU &mcpu, int &threadOwnershipMask);

	// Component ------------------------------------------------------------

	// Individual events -------------------------------------------------------
	
	Handle &nextAdditionalShaderValue(int size)
	{
		m_additionalShaderValues.add(Handle("RAW_DATA", size));
		return m_additionalShaderValues[m_additionalShaderValues.m_size-1];
	}


	void overrideEffects(Handle newEffect);
	void overrideZOnlyEffects(Handle newEffect);
	

	void popEffects();

	PrimitiveTypes::Bool hasPushedEffects();
	
	// AABB access methods
	const PE::AABB& getLocalAABB() const { return m_localAABB; }
	PrimitiveTypes::Bool hasAABB() const { return m_aabbValid; }
	void setAABB(const PE::AABB& aabb) { m_localAABB = aabb; m_aabbValid = true; }
	
	// Mesh name for debugging
	const char* getMeshName() const { return m_meshName; }
	void setMeshName(const char* name) {
		if (name) {
			strncpy(m_meshName, name, sizeof(m_meshName) - 1);
			m_meshName[sizeof(m_meshName) - 1] = '\0';
		} else {
			m_meshName[0] = '\0';
		}
	}
	
	// Get AABB corners for frustum culling
	void getAABBCorners(Vector3* corners) const {
		if (!m_aabbValid) {
			// Return empty AABB if invalid
			for (int i = 0; i < 8; i++) {
				corners[i] = Vector3(0.0f, 0.0f, 0.0f);
			}
			return;
		}
		
		Vector3 min = m_localAABB.min();
		Vector3 max = m_localAABB.max();
		
		// Define the 8 corners of the AABB
		corners[0] = Vector3(min.m_x, min.m_y, min.m_z); // 0: min corner
		corners[1] = Vector3(max.m_x, min.m_y, min.m_z); // 1: min corner + x
		corners[2] = Vector3(max.m_x, max.m_y, min.m_z); // 2: min corner + x + y
		corners[3] = Vector3(min.m_x, max.m_y, min.m_z); // 3: min corner + y
		corners[4] = Vector3(min.m_x, min.m_y, max.m_z); // 4: min corner + z
		corners[5] = Vector3(max.m_x, min.m_y, max.m_z); // 5: min corner + x + z
		corners[6] = Vector3(max.m_x, max.m_y, max.m_z); // 6: max corner
		corners[7] = Vector3(min.m_x, max.m_y, max.m_z); // 7: min corner + y + z
	}
	// Member variables --------------------------------------------------------
	//Handle m_hVertexBufferGPU;
	Handle m_hTexCoordBufferCPU;
	
	Handle m_hIndexBufferGPU;
	
	Handle m_hMaterialSetGPU;

	PrimitiveTypes::Bool m_processShowEvt;

	Handle m_hPositionBufferCPU;
	Handle m_hNormalBufferCPU;
	Handle m_hTangentBufferCPU;

	Handle m_hSkinWeightsCPU;

	Array<Handle> m_additionalShaderValues;

	PEStaticVector<Handle, 4> m_vertexBuffersGPUHs;

	Array< PEStaticVector<Handle, 4> > m_effects;
	Array< PEStaticVector<Handle, 4> > m_zOnlyEffects;
	Array< PEStaticVector<Handle, 4> > m_instanceEffects;

	Array<Handle, 1> m_instances; // special cahce of instances
	Array<Handle> m_lods;
    int m_numVisibleInstances;
	
	Handle m_hAnimationSetGPU; // reference to animation stored in gpu buffer
	
	bool m_bDrawControl;
    
    bool m_performBoundingVolumeCulling;
    
    // AABB data for debug rendering
    PE::AABB m_localAABB;
    PrimitiveTypes::Bool m_aabbValid;
    
    // Mesh name for debugging
    char m_meshName[64];
};

}; // namespace Components
}; // namespace PE
#endif
