#ifndef __PRIMEENGINE_NAVMESH_COMPONENT_H__
#define __PRIMEENGINE_NAVMESH_COMPONENT_H__

// API Abstraction
#include "PrimeEngine/APIAbstraction/APIAbstractionDefines.h"

// Inter-Engine includes
#include "PrimeEngine/MemoryManagement/Handle.h"
#include "PrimeEngine/PrimitiveTypes/PrimitiveTypes.h"
#include "PrimeEngine/Math/Vector3.h"
#include "PrimeEngine/Utils/Array/Array.h"
#include "../Events/Component.h"

namespace PE {
namespace Components {

// Forward declaration
struct SceneNode;

// ============================================================================
// NavmeshTriangle - Single triangle in the navigation mesh
// ============================================================================
struct NavmeshTriangle
{
	// Vertex indices into NavmeshComponent::m_vertices array
	// Using "opposite vertex" edge convention:
	//   Edge 0 (opposite v0): connects vertexIndices[1] to vertexIndices[2]
	//   Edge 1 (opposite v1): connects vertexIndices[2] to vertexIndices[0]
	//   Edge 2 (opposite v2): connects vertexIndices[0] to vertexIndices[1]
	PrimitiveTypes::UInt32 vertexIndices[3];

	// Adjacency information - indices into NavmeshComponent::m_triangles array
	// neighbors[i] = triangle index sharing edge i, or -1 if no neighbor (boundary)
	PrimitiveTypes::Int32 neighbors[3];

	// Metadata for pathfinding
	PrimitiveTypes::Float32 cost;        // Traversal cost multiplier (1.0 = normal)
	PrimitiveTypes::UInt32 flags;        // Special properties (WATER, STAIRS, etc.)

	// Cached data for pathfinding (computed at load time)
	Vector3 center;                      // Triangle centroid (average of 3 vertices)
	PrimitiveTypes::Float32 area;        // Triangle area (for heuristics)

	// Constructor
	NavmeshTriangle()
		: cost(1.0f)
		, flags(0)
		, area(0.0f)
	{
		vertexIndices[0] = vertexIndices[1] = vertexIndices[2] = 0;
		neighbors[0] = neighbors[1] = neighbors[2] = -1;
	}

	// Helper: Check if this triangle uses a given vertex
	bool hasVertex(PrimitiveTypes::UInt32 vertexIndex) const
	{
		return vertexIndices[0] == vertexIndex
			|| vertexIndices[1] == vertexIndex
			|| vertexIndices[2] == vertexIndex;
	}

	// Helper: Check if this triangle has an edge with given vertices
	// (in any order - used for computing adjacency)
	bool hasEdge(PrimitiveTypes::UInt32 v0, PrimitiveTypes::UInt32 v1) const
	{
		// Check all 3 edges
		for (int i = 0; i < 3; i++)
		{
			PrimitiveTypes::UInt32 edgeV0 = vertexIndices[i];
			PrimitiveTypes::UInt32 edgeV1 = vertexIndices[(i + 1) % 3];

			// Check both orderings (edge can be v0→v1 or v1→v0)
			if ((edgeV0 == v0 && edgeV1 == v1) || (edgeV0 == v1 && edgeV1 == v0))
			{
				return true;
			}
		}
		return false;
	}

	// Helper: Get which edge connects to a specific vertex pair
	// Returns edge index (0-2) or -1 if not found
	PrimitiveTypes::Int32 getEdgeForVertices(PrimitiveTypes::UInt32 v0, PrimitiveTypes::UInt32 v1) const
	{
		for (int i = 0; i < 3; i++)
		{
			PrimitiveTypes::UInt32 edgeV0 = vertexIndices[i];
			PrimitiveTypes::UInt32 edgeV1 = vertexIndices[(i + 1) % 3];

			if ((edgeV0 == v0 && edgeV1 == v1) || (edgeV0 == v1 && edgeV1 == v0))
			{
				return i;
			}
		}
		return -1;
	}
};

// ============================================================================
// NavmeshComponent - Stores and manages navigation mesh data
// ============================================================================
struct NavmeshComponent : public Component
{
	PE_DECLARE_CLASS(NavmeshComponent);

	// Constructor
	NavmeshComponent(PE::GameContext &context, PE::MemoryArena arena, Handle hMyself);
	virtual ~NavmeshComponent() {}

	// Component interface
	virtual void addDefaultComponents();

	// ========================================================================
	// Loading & Initialization
	// ========================================================================

	// Load navmesh from .navmesh file
	// Returns true on success, false on failure
	bool loadFromFile(const char* filename, const char* package);

	// Compute derived data (triangle centers, areas, etc.)
	// Called automatically after loading
	void computeDerivedData();

	// Build adjacency graph if not provided in file
	// Called automatically if ADJACENCY section is missing
	void computeAdjacency();

	// ========================================================================
	// Spatial Queries (for pathfinding)
	// ========================================================================

	// Find which triangle contains the given world position
	// Returns triangle index or -1 if position not on navmesh
	PrimitiveTypes::Int32 findTriangleContainingPoint(const Vector3& position) const;

	// Find nearest triangle to a position (even if position is off-navmesh)
	// Useful for snapping AI agents to navmesh
	PrimitiveTypes::Int32 findNearestTriangle(const Vector3& position) const;

	// Point-in-triangle test (2D XZ plane)
	// Uses barycentric coordinates
	bool isPointInTriangle(const Vector3& point, PrimitiveTypes::UInt32 triangleIndex) const;

	// Get distance from point to triangle (for nearest triangle search)
	PrimitiveTypes::Float32 getDistanceToTriangle(const Vector3& point, PrimitiveTypes::UInt32 triangleIndex) const;

	// ========================================================================
	// Accessors
	// ========================================================================

	const Array<Vector3>& getVertices() const { return m_vertices; }
	const Array<NavmeshTriangle>& getTriangles() const { return m_triangles; }

	PrimitiveTypes::UInt32 getVertexCount() const { return m_vertices.m_size; }
	PrimitiveTypes::UInt32 getTriangleCount() const { return m_triangles.m_size; }

	const Vector3& getVertex(PrimitiveTypes::UInt32 index) const
	{
		// Array doesn't have const operator[], so we cast away const
		// This is safe because we're only reading
		return const_cast<Array<Vector3>&>(m_vertices)[index];
	}

	const NavmeshTriangle& getTriangle(PrimitiveTypes::UInt32 index) const
	{
		// Array doesn't have const operator[], so we cast away const
		// This is safe because we're only reading
		return const_cast<Array<NavmeshTriangle>&>(m_triangles)[index];
	}

	// Get triangle vertices (for rendering, collision, etc.)
	void getTriangleVertices(PrimitiveTypes::UInt32 triangleIndex, Vector3 outVerts[3]) const;

	const char* getName() const { return m_name; }

	// ========================================================================
	// Debug & Visualization
	// ========================================================================

	// Event handlers for debug rendering
	PE_DECLARE_IMPLEMENT_EVENT_HANDLER_WRAPPER(do_GATHER_DRAWCALLS);
	virtual void do_GATHER_DRAWCALLS(Events::Event *pEvt);

	void setDebugRenderEnabled(bool enabled) { m_debugRenderEnabled = enabled; }
	bool isDebugRenderEnabled() const { return m_debugRenderEnabled; }

	// ========================================================================
	// Data Members
	// ========================================================================

	// Navmesh geometry
	Array<Vector3> m_vertices;              // Shared vertex pool
	Array<NavmeshTriangle> m_triangles;     // Triangle soup with adjacency

	// Metadata
	char m_name[128];                       // Navmesh name (e.g., "ccontrollvl0")
	PrimitiveTypes::Float32 m_version;      // File format version

	// Debug rendering
	bool m_debugRenderEnabled;              // Show navmesh in game?
	Handle m_hDebugMesh;                    // LineMesh for visualization (optional)

	// Spatial acceleration (optional - for future optimization)
	// Could add a grid or BVH here for faster point queries
	// For now, we'll just brute-force search (fine for <1000 triangles)
};

}; // namespace Components
}; // namespace PE

#endif // __PRIMEENGINE_NAVMESH_COMPONENT_H__
