#define NOMINMAX
#include "NavmeshComponent.h"

// API Abstraction
#include "PrimeEngine/APIAbstraction/APIAbstractionDefines.h"

// Inter-Engine includes
#include "PrimeEngine/FileSystem/FileReader.h"
#include "PrimeEngine/Utils/StringOps.h"
#include "PrimeEngine/Utils/PEString.h"
#include "PrimeEngine/Lua/LuaEnvironment.h"
#include "PrimeEngine/Scene/SceneNode.h"
#include "PrimeEngine/Scene/DebugRenderer.h"

// For debug output and string functions
#include <stdio.h>
#include <string.h>

// Helper: Check if string contains substring (StringOps doesn't have this)
static bool stringContains(const char* str, const char* substr)
{
	return strstr(str, substr) != NULL;
}

namespace PE {
namespace Components {

PE_IMPLEMENT_CLASS1(NavmeshComponent, Component);

// ============================================================================
// Constructor
// ============================================================================
NavmeshComponent::NavmeshComponent(PE::GameContext &context, PE::MemoryArena arena, Handle hMyself)
	: Component(context, arena, hMyself)
	, m_vertices(context, arena)
	, m_triangles(context, arena)
	, m_debugPath(context, arena)
	, m_version(1.0f)
	, m_debugRenderEnabled(false)
	, m_debugPathEnabled(false)
{
	m_name[0] = '\0';
	m_transform.loadIdentity();  // Initialize transform to identity
}

// ============================================================================
// Component Interface
// ============================================================================
void NavmeshComponent::addDefaultComponents()
{
	Component::addDefaultComponents();

	// Register for debug rendering events
	PE_REGISTER_EVENT_HANDLER(Events::Event_GATHER_DRAWCALLS, NavmeshComponent::do_GATHER_DRAWCALLS);
}

// ============================================================================
// Loading & Initialization
// ============================================================================
bool NavmeshComponent::loadFromFile(const char* filename, const char* package)
{
	PEINFO("NavmeshComponent: Loading navmesh from file: %s (package: %s)\n", filename, package);

	// Generate full file path using Prime Engine's path system
	char fullPath[256];
	PEString::generatePathname(*m_pContext, filename, package, "Levels", fullPath, 256);

	PEINFO("NavmeshComponent: Full path: %s\n", fullPath);

	// Open file
	FileReader reader(fullPath);

	// Read file line by line
	char line[256];
	bool hasAdjacency = false;

	while (reader.nextNonEmptyLine(line, 256))
	{
		// Skip comments
		if (line[0] == '#')
			continue;

		// Trim whitespace
		char* token = line;
		while (*token == ' ' || *token == '\t')
			token++;

		// Parse keywords
		if (StringOps::startsswith(token, "NAVMESH"))
		{
			// Extract navmesh name
			sscanf(token, "NAVMESH %s", m_name);
			PEINFO("NavmeshComponent: Navmesh name: %s\n", m_name);
		}
		else if (StringOps::startsswith(token, "VERSION"))
		{
			sscanf(token, "VERSION %f", &m_version);
			PEINFO("NavmeshComponent: Version: %.1f\n", m_version);
		}
		else if (StringOps::startsswith(token, "TRANSFORM"))
		{
			// Read 4x4 transform matrix
			PEINFO("NavmeshComponent: Reading transform matrix...\n");
			m_transform.loadIdentity();  // Start with identity

			for (int row = 0; row < 4; row++)
			{
				if (!reader.nextNonEmptyLine(line, 256))
					break;

				float m00, m01, m02, m03;
				if (sscanf(line, "%f %f %f %f", &m00, &m01, &m02, &m03) == 4)
				{
					m_transform.setRow(Row4(m00, m01, m02, m03), row);
				}
			}
			PEINFO("NavmeshComponent: Transform matrix loaded\n");
		}
		else if (StringOps::startsswith(token, "VERTEX_COUNT"))
		{
			int vertexCount = 0;
			sscanf(token, "VERTEX_COUNT %d", &vertexCount);
			PEINFO("NavmeshComponent: Vertex count: %d\n", vertexCount);

			// Reserve space
			m_vertices.reset(vertexCount);
		}
		else if (StringOps::startsswith(token, "VERTICES"))
		{
			// Read vertex data
			PEINFO("NavmeshComponent: Reading vertices...\n");

			int count = 0;
			while (reader.nextNonEmptyLine(line, 256))
			{
				// Stop if we hit next section
				if (line[0] == '#' || stringContains(line, "TRIANGLE"))
					break;

				// Parse vertex position
				Vector3 v;
				if (sscanf(line, "%f %f %f", &v.m_x, &v.m_y, &v.m_z) == 3)
				{
					m_vertices.add(v);
					count++;
				}
			}

			PEINFO("NavmeshComponent: Loaded %d vertices\n", count);

			// We just read a line that might be TRIANGLE_COUNT, check it
			if (stringContains(line, "TRIANGLE_COUNT"))
			{
				int triangleCount = 0;
				sscanf(line, "TRIANGLE_COUNT %d", &triangleCount);
				PEINFO("NavmeshComponent: Triangle count: %d\n", triangleCount);

				// Reserve space
				m_triangles.reset(triangleCount);
			}
		}
		else if (StringOps::startsswith(token, "TRIANGLE_COUNT"))
		{
			int triangleCount = 0;
			sscanf(token, "TRIANGLE_COUNT %d", &triangleCount);
			PEINFO("NavmeshComponent: Triangle count: %d\n", triangleCount);

			// Reserve space
			m_triangles.reset(triangleCount);
		}
		else if (StringOps::startsswith(token, "TRIANGLES"))
		{
			// Read triangle indices
			PEINFO("NavmeshComponent: Reading triangles...\n");

			int count = 0;
			while (reader.nextNonEmptyLine(line, 256))
			{
				// Stop if we hit next section
				if (line[0] == '#' || stringContains(line, "ADJACENCY") || stringContains(line, "TRIANGLE_METADATA") || stringContains(line, "END"))
					break;

				// Parse triangle indices
				NavmeshTriangle tri;
				if (sscanf(line, "%u %u %u", &tri.vertexIndices[0], &tri.vertexIndices[1], &tri.vertexIndices[2]) == 3)
				{
					m_triangles.add(tri);
					count++;
				}
			}

			PEINFO("NavmeshComponent: Loaded %d triangles\n", count);

			// Check what section we just read
			if (stringContains(line, "ADJACENCY"))
			{
				hasAdjacency = true;

				// Read adjacency data
				PEINFO("NavmeshComponent: Reading adjacency...\n");

				int adjCount = 0;
				while (reader.nextNonEmptyLine(line, 256))
				{
					// Stop if we hit next section
					if (line[0] == '#' || stringContains(line, "TRIANGLE_METADATA") || stringContains(line, "END"))
						break;

					// Parse adjacency
					if (adjCount < m_triangles.m_size)
					{
						int triIndex, n0, n1, n2;
						// Try format: "triIndex n0 n1 n2"
						if (sscanf(line, "%d %d %d %d", &triIndex, &n0, &n1, &n2) == 4)
						{
							if (triIndex < (int)m_triangles.m_size)
							{
								m_triangles[triIndex].neighbors[0] = n0;
								m_triangles[triIndex].neighbors[1] = n1;
								m_triangles[triIndex].neighbors[2] = n2;
								adjCount++;
							}
						}
						// Try simpler format: "n0 n1 n2" (implicit triangle index)
						else if (sscanf(line, "%d %d %d", &n0, &n1, &n2) == 3)
						{
							m_triangles[adjCount].neighbors[0] = n0;
							m_triangles[adjCount].neighbors[1] = n1;
							m_triangles[adjCount].neighbors[2] = n2;
							adjCount++;
						}
					}
				}

				PEINFO("NavmeshComponent: Loaded adjacency for %d triangles\n", adjCount);
			}
		}
		else if (StringOps::startsswith(token, "END"))
		{
			// End of file
			break;
		}
	}

	// Vertices are exported in world space, so no transform needed
	// (The TRANSFORM section is still loaded for reference, but not applied)
	PEINFO("NavmeshComponent: Vertices already in world space, no transform applied\n");

	// Compute adjacency if not provided in file
	if (!hasAdjacency)
	{
		PEINFO("NavmeshComponent: No adjacency data in file, computing...\n");
		computeAdjacency();
	}

	// Compute derived data (centers, areas, etc.)
	computeDerivedData();

	PEINFO("NavmeshComponent: Loading complete! %d vertices, %d triangles\n",
		m_vertices.m_size, m_triangles.m_size);

	return true;
}

void NavmeshComponent::computeDerivedData()
{
	PEINFO("NavmeshComponent: Computing derived data...\n");

	for (PrimitiveTypes::UInt32 i = 0; i < m_triangles.m_size; i++)
	{
		NavmeshTriangle& tri = m_triangles[i];

		// Get triangle vertices
		const Vector3& v0 = m_vertices[tri.vertexIndices[0]];
		const Vector3& v1 = m_vertices[tri.vertexIndices[1]];
		const Vector3& v2 = m_vertices[tri.vertexIndices[2]];

		// Compute center (centroid)
		tri.center = (v0 + v1 + v2) * (1.0f / 3.0f);

		// Compute area using cross product
		// Area = 0.5 * ||AB x AC||
		Vector3 AB = v1 - v0;
		Vector3 AC = v2 - v0;
		Vector3 cross = AB.crossProduct(AC);
		tri.area = cross.length() * 0.5f;
	}

	PEINFO("NavmeshComponent: Derived data computed\n");
}

void NavmeshComponent::computeAdjacency()
{
	PEINFO("NavmeshComponent: Computing adjacency graph...\n");

	// For each triangle, find its neighbors
	for (PrimitiveTypes::UInt32 i = 0; i < m_triangles.m_size; i++)
	{
		NavmeshTriangle& tri = m_triangles[i];

		// Check each edge of this triangle
		// Edge convention: Edge N is opposite vertex N
		//   Edge 0 (opposite v0): connects v1 to v2
		//   Edge 1 (opposite v1): connects v2 to v0
		//   Edge 2 (opposite v2): connects v0 to v1
		for (int edge = 0; edge < 3; edge++)
		{
			// Get the two vertices that form this edge
			PrimitiveTypes::UInt32 edgeV0, edgeV1;

			if (edge == 0)
			{
				// Edge 0: v1 to v2
				edgeV0 = tri.vertexIndices[1];
				edgeV1 = tri.vertexIndices[2];
			}
			else if (edge == 1)
			{
				// Edge 1: v2 to v0
				edgeV0 = tri.vertexIndices[2];
				edgeV1 = tri.vertexIndices[0];
			}
			else // edge == 2
			{
				// Edge 2: v0 to v1
				edgeV0 = tri.vertexIndices[0];
				edgeV1 = tri.vertexIndices[1];
			}

			// Search for another triangle that shares these 2 vertices
			bool foundNeighbor = false;
			for (PrimitiveTypes::UInt32 j = 0; j < m_triangles.m_size; j++)
			{
				if (i == j)
					continue; // Don't compare triangle to itself

				const NavmeshTriangle& other = m_triangles[j];

				// Check if 'other' contains both edgeV0 and edgeV1
				if (other.hasEdge(edgeV0, edgeV1))
				{
					tri.neighbors[edge] = j; // Found neighbor!
					foundNeighbor = true;
					break;
				}
			}

			// If no neighbor found, mark as boundary (-1)
			if (!foundNeighbor)
			{
				tri.neighbors[edge] = -1;
			}
		}
	}

	PEINFO("NavmeshComponent: Adjacency computed\n");
}

// ============================================================================
// Spatial Queries
// ============================================================================
PrimitiveTypes::Int32 NavmeshComponent::findTriangleContainingPoint(const Vector3& position) const
{
	// Brute-force search (fine for small navmeshes < 1000 triangles)
	// For larger navmeshes, use spatial acceleration (grid, BVH)
	for (PrimitiveTypes::UInt32 i = 0; i < m_triangles.m_size; i++)
	{
		if (isPointInTriangle(position, i))
		{
			return i;
		}
	}

	return -1; // Not found
}

PrimitiveTypes::Int32 NavmeshComponent::findNearestTriangle(const Vector3& position) const
{
	PrimitiveTypes::Int32 nearestIndex = -1;
	PrimitiveTypes::Float32 nearestDist = FLT_MAX;

	for (PrimitiveTypes::UInt32 i = 0; i < m_triangles.m_size; i++)
	{
		PrimitiveTypes::Float32 dist = getDistanceToTriangle(position, i);
		if (dist < nearestDist)
		{
			nearestDist = dist;
			nearestIndex = i;
		}
	}

	return nearestIndex;
}

bool NavmeshComponent::isPointInTriangle(const Vector3& point, PrimitiveTypes::UInt32 triangleIndex) const
{
	if (triangleIndex >= m_triangles.m_size)
		return false;

	// Array doesn't have const operator[], cast away const (safe for reading)
	const NavmeshTriangle& tri = const_cast<Array<NavmeshTriangle>&>(m_triangles)[triangleIndex];

	// Get triangle vertices
	const Vector3& v0 = const_cast<Array<Vector3>&>(m_vertices)[tri.vertexIndices[0]];
	const Vector3& v1 = const_cast<Array<Vector3>&>(m_vertices)[tri.vertexIndices[1]];
	const Vector3& v2 = const_cast<Array<Vector3>&>(m_vertices)[tri.vertexIndices[2]];

	// Use barycentric coordinates (2D test on XZ plane)
	// Point P is inside triangle ABC if:
	//   Barycentric coords (u, v, w) all >= 0 and u + v + w = 1

	// Compute vectors
	Vector3 v0v1 = v1 - v0;
	Vector3 v0v2 = v2 - v0;
	Vector3 v0p = point - v0;

	// Compute dot products (using XZ plane, ignoring Y)
	float dot00 = v0v2.m_x * v0v2.m_x + v0v2.m_z * v0v2.m_z;
	float dot01 = v0v2.m_x * v0v1.m_x + v0v2.m_z * v0v1.m_z;
	float dot02 = v0v2.m_x * v0p.m_x + v0v2.m_z * v0p.m_z;
	float dot11 = v0v1.m_x * v0v1.m_x + v0v1.m_z * v0v1.m_z;
	float dot12 = v0v1.m_x * v0p.m_x + v0v1.m_z * v0p.m_z;

	// Compute barycentric coordinates
	float invDenom = 1.0f / (dot00 * dot11 - dot01 * dot01);
	float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
	float v = (dot00 * dot12 - dot01 * dot02) * invDenom;

	// Check if point is inside
	return (u >= 0.0f) && (v >= 0.0f) && (u + v <= 1.0f);
}

PrimitiveTypes::Float32 NavmeshComponent::getDistanceToTriangle(const Vector3& point, PrimitiveTypes::UInt32 triangleIndex) const
{
	if (triangleIndex >= m_triangles.m_size)
		return FLT_MAX;

	// Simple implementation: distance to triangle center
	// For better accuracy, compute distance to closest point on triangle
	const NavmeshTriangle& tri = const_cast<Array<NavmeshTriangle>&>(m_triangles)[triangleIndex];
	Vector3 diff = point - tri.center;
	return diff.length();
}

// ============================================================================
// Accessors
// ============================================================================
void NavmeshComponent::getTriangleVertices(PrimitiveTypes::UInt32 triangleIndex, Vector3 outVerts[3]) const
{
	if (triangleIndex >= m_triangles.m_size)
		return;

	const NavmeshTriangle& tri = const_cast<Array<NavmeshTriangle>&>(m_triangles)[triangleIndex];
	Array<Vector3>& verts = const_cast<Array<Vector3>&>(m_vertices);
	outVerts[0] = verts[tri.vertexIndices[0]];
	outVerts[1] = verts[tri.vertexIndices[1]];
	outVerts[2] = verts[tri.vertexIndices[2]];
}

// ============================================================================
// Debug Rendering
// ============================================================================
void NavmeshComponent::do_GATHER_DRAWCALLS(Events::Event *pEvt)
{
	if (!m_debugRenderEnabled)
		return;

	// Draw navmesh as wireframe using DebugRenderer
	Events::Event_GATHER_DRAWCALLS *pDrawEvent = NULL;
	pDrawEvent = (Events::Event_GATHER_DRAWCALLS *)(pEvt);

	// Get debug renderer
	Handle hDebugRenderer = DebugRenderer::Instance();
	if (!hDebugRenderer.isValid())
		return;

	DebugRenderer *pDebugRenderer = hDebugRenderer.getObject<DebugRenderer>();

	// Build line data for all triangles
	// Format: [v0, color, v1, color, v1, color, v2, color, v2, color, v0, color] per triangle
	Vector3 offset(0.0f, 0.05f, 0.0f);
	Vector3 color(0.0f, 1.0f, 0.0f); // Green

	// Each triangle = 3 edges = 6 points (3 lines * 2 points per line)
	int numPoints = m_triangles.m_size * 6;
	Vector3* lineData = (Vector3*)alloca(sizeof(Vector3) * numPoints * 2); // *2 for position + color per point

	int pointIndex = 0;
	for (PrimitiveTypes::UInt32 i = 0; i < m_triangles.m_size; i++)
	{
		const NavmeshTriangle& tri = const_cast<Array<NavmeshTriangle>&>(m_triangles)[i];

		// Get triangle vertices
		const Vector3& v0 = const_cast<Array<Vector3>&>(m_vertices)[tri.vertexIndices[0]];
		const Vector3& v1 = const_cast<Array<Vector3>&>(m_vertices)[tri.vertexIndices[1]];
		const Vector3& v2 = const_cast<Array<Vector3>&>(m_vertices)[tri.vertexIndices[2]];

		// Edge 0: v0 -> v1
		lineData[pointIndex++] = v0 + offset;
		lineData[pointIndex++] = color;
		lineData[pointIndex++] = v1 + offset;
		lineData[pointIndex++] = color;

		// Edge 1: v1 -> v2
		lineData[pointIndex++] = v1 + offset;
		lineData[pointIndex++] = color;
		lineData[pointIndex++] = v2 + offset;
		lineData[pointIndex++] = color;

		// Edge 2: v2 -> v0
		lineData[pointIndex++] = v2 + offset;
		lineData[pointIndex++] = color;
		lineData[pointIndex++] = v0 + offset;
		lineData[pointIndex++] = color;
	}

	// Create line mesh with identity transform (vertices already in world space)
	Matrix4x4 identityMatrix;
	identityMatrix.loadIdentity();

	pDebugRenderer->createLineMesh(
		false,  // No transform (vertices already in world space)
		identityMatrix,
		&lineData[0].m_x,
		numPoints,
		0.0f,  // 0 = persistent (draw every frame)
		1.0f   // Scale
	);

	// Draw path visualization if enabled
	if (m_debugPathEnabled && m_debugPath.m_size > 1)
	{
		// Draw path as connected line segments
		// Color: Yellow for visibility
		Vector3 pathOffset(0.0f, 0.1f, 0.0f); // Slightly higher than navmesh
		Vector3 pathColor(1.0f, 1.0f, 0.0f); // Yellow

		// Each segment = 2 points (start + end)
		// We need m_debugPath.m_size - 1 segments
		int numSegments = m_debugPath.m_size - 1;
		int numPathPoints = numSegments * 2;
		Vector3* pathLineData = (Vector3*)alloca(sizeof(Vector3) * numPathPoints * 2); // *2 for position + color

		int pathPointIndex = 0;
		for (PrimitiveTypes::UInt32 i = 0; i < m_debugPath.m_size - 1; i++)
		{
			const Vector3& start = const_cast<Array<Vector3>&>(m_debugPath)[i];
			const Vector3& end = const_cast<Array<Vector3>&>(m_debugPath)[i + 1];

			// Line segment from start to end
			pathLineData[pathPointIndex++] = start + pathOffset;
			pathLineData[pathPointIndex++] = pathColor;
			pathLineData[pathPointIndex++] = end + pathOffset;
			pathLineData[pathPointIndex++] = pathColor;
		}

		pDebugRenderer->createLineMesh(
			false,  // No transform
			identityMatrix,
			&pathLineData[0].m_x,
			numPathPoints,
			0.0f,  // Persistent
			1.0f   // Scale
		);
	}
}

// ============================================================================
// Pathfinding - A* Algorithm
// ============================================================================

// Helper struct for A* algorithm
struct AStarNode
{
	PrimitiveTypes::UInt32 triangleIndex;
	PrimitiveTypes::Float32 gCost;  // Cost from start
	PrimitiveTypes::Float32 hCost;  // Heuristic cost to goal
	PrimitiveTypes::Float32 fCost;  // gCost + hCost
	PrimitiveTypes::Int32 parentIndex;  // Index in closed list of parent node (-1 if none)

	AStarNode()
		: triangleIndex(0), gCost(0.0f), hCost(0.0f), fCost(0.0f), parentIndex(-1)
	{}

	AStarNode(PrimitiveTypes::UInt32 tri, PrimitiveTypes::Float32 g, PrimitiveTypes::Float32 h, PrimitiveTypes::Int32 parent = -1)
		: triangleIndex(tri), gCost(g), hCost(h), fCost(g + h), parentIndex(parent)
	{}
};

bool NavmeshComponent::findPath(const Vector3& startPos, const Vector3& endPos, Array<Vector3>& outPath)
{
	// Find triangles containing start and end positions
	PrimitiveTypes::Int32 startTri = findTriangleContainingPoint(startPos);
	PrimitiveTypes::Int32 endTri = findTriangleContainingPoint(endPos);

	// If either position is not on navmesh, try finding nearest triangle
	if (startTri == -1)
	{
		startTri = findNearestTriangle(startPos);
		PEINFO("NavmeshComponent::findPath: Start position not on navmesh, using nearest triangle %d\n", startTri);
	}

	if (endTri == -1)
	{
		endTri = findNearestTriangle(endPos);
		PEINFO("NavmeshComponent::findPath: End position not on navmesh, using nearest triangle %d\n", endTri);
	}

	// If we still can't find valid triangles, fail
	if (startTri == -1 || endTri == -1)
	{
		PEINFO("NavmeshComponent::findPath: Failed to find valid start/end triangles\n");
		return false;
	}

	// Find triangle path using A*
	// Allocate space for worst-case path
	Array<PrimitiveTypes::UInt32> trianglePath(*m_pContext, m_arena, m_triangles.m_size);
	if (!findTrianglePath(startTri, endTri, trianglePath))
	{
		PEINFO("NavmeshComponent::findPath: A* failed to find path\n");
		return false;
	}

	// Convert triangle path to waypoints
	trianglePathToWaypoints(trianglePath, outPath);

	// Add start and end positions as first and last waypoints
	if (outPath.m_size > 0)
	{
		// Replace first waypoint with actual start position
		outPath[0] = startPos;

		// Replace last waypoint with actual end position
		outPath[outPath.m_size - 1] = endPos;
	}

	PEINFO("NavmeshComponent::findPath: Found path with %d waypoints\n", outPath.m_size);
	return true;
}

bool NavmeshComponent::findTrianglePath(PrimitiveTypes::Int32 startTriIndex, PrimitiveTypes::Int32 endTriIndex,
                                         Array<PrimitiveTypes::UInt32>& outTrianglePath)
{
	// Early exit if start == end
	if (startTriIndex == endTriIndex)
	{
		outTrianglePath.reset(1);
		outTrianglePath.add((PrimitiveTypes::UInt32)startTriIndex);
		return true;
	}

	// A* data structures
	// Pre-allocate arrays to avoid running out of space during search
	// Worst case: we explore all triangles, so allocate space for all
	PrimitiveTypes::UInt32 maxNodes = m_triangles.m_size;

	// Use constructor with capacity to properly allocate memory
	Array<AStarNode> openList(*m_pContext, m_arena, maxNodes);  // Nodes to explore
	Array<AStarNode> closedList(*m_pContext, m_arena, maxNodes);  // Nodes already explored

	// Initialize closed list tracker (one bool per triangle, initialized to false)
	Array<bool> inClosed(*m_pContext, m_arena, m_triangles.m_size, false);

	// Get goal triangle center for heuristic
	const NavmeshTriangle& endTri = m_triangles[endTriIndex];
	const Vector3& goalPos = endTri.center;

	// Add start node to open list
	const NavmeshTriangle& startTri = m_triangles[startTriIndex];
	float heuristic = (startTri.center - goalPos).length();
	openList.add(AStarNode(startTriIndex, 0.0f, heuristic, -1));

	// A* main loop
	int iterations = 0;
	while (openList.m_size > 0)
	{
		iterations++;
		if (iterations > 10000)
		{
			PEINFO("ERROR: A* exceeded 10000 iterations, aborting\n");
			return false;
		}

		// Find node with lowest fCost in open list
		PrimitiveTypes::UInt32 bestIndex = 0;
		float bestFCost = openList[0].fCost;
		for (PrimitiveTypes::UInt32 i = 1; i < openList.m_size; i++)
		{
			if (openList[i].fCost < bestFCost)
			{
				bestIndex = i;
				bestFCost = openList[i].fCost;
			}
		}

		// Move best node from open to closed list
		AStarNode current = openList[bestIndex];
		openList.remove(bestIndex);

		PrimitiveTypes::Int32 currentClosedIndex = (PrimitiveTypes::Int32)closedList.m_size;
		closedList.add(current);

		// Bounds check before accessing inClosed
		if (current.triangleIndex >= m_triangles.m_size)
		{
			PEINFO("ERROR: Triangle index %d out of bounds (max: %d)\n",
				current.triangleIndex, m_triangles.m_size - 1);
			return false;
		}

		inClosed[current.triangleIndex] = true;

		// Check if we reached the goal
		if ((PrimitiveTypes::Int32)current.triangleIndex == endTriIndex)
		{
			// Reconstruct path by following parent indices backwards
			// Allocate enough space for worst-case path (all triangles)
			Array<PrimitiveTypes::UInt32> reversePath(*m_pContext, m_arena, maxNodes);
			PrimitiveTypes::Int32 nodeIndex = currentClosedIndex;

			while (nodeIndex != -1)
			{
				const AStarNode& node = closedList[nodeIndex];
				reversePath.add(node.triangleIndex);
				nodeIndex = node.parentIndex;
			}

			// Reverse the path (we built it backwards)
			// Clear output array and reserve space
			outTrianglePath.reset(reversePath.m_size);
			for (PrimitiveTypes::Int32 i = (PrimitiveTypes::Int32)reversePath.m_size - 1; i >= 0; i--)
			{
				outTrianglePath.add(reversePath[i]);
			}

			return true;
		}

		// Explore neighbors
		const NavmeshTriangle& currentTri = m_triangles[current.triangleIndex];

		for (int i = 0; i < 3; i++)
		{
			PrimitiveTypes::Int32 neighborIndex = currentTri.neighbors[i];

			// Skip if no neighbor on this edge
			if (neighborIndex == -1)
				continue;

			// Bounds check for neighbor index
			if (neighborIndex < 0 || (PrimitiveTypes::UInt32)neighborIndex >= m_triangles.m_size)
			{
				PEINFO("WARNING: Invalid neighbor index %d for triangle %d (max: %d)\n",
					neighborIndex, current.triangleIndex, m_triangles.m_size - 1);
				continue;
			}

			// Skip if already in closed list
			if (inClosed[neighborIndex])
				continue;

			// Calculate cost to reach this neighbor
			const NavmeshTriangle& neighborTri = m_triangles[neighborIndex];
			float edgeCost = (neighborTri.center - currentTri.center).length();
			float newGCost = current.gCost + edgeCost;

			// Check if neighbor is already in open list
			bool inOpen = false;
			PrimitiveTypes::UInt32 openIndex = 0;
			for (PrimitiveTypes::UInt32 j = 0; j < openList.m_size; j++)
			{
				if ((PrimitiveTypes::Int32)openList[j].triangleIndex == neighborIndex)
				{
					inOpen = true;
					openIndex = j;
					break;
				}
			}

			if (inOpen)
			{
				// If we found a better path to this neighbor, update it
				if (newGCost < openList[openIndex].gCost)
				{
					openList[openIndex].gCost = newGCost;
					openList[openIndex].fCost = newGCost + openList[openIndex].hCost;
					openList[openIndex].parentIndex = currentClosedIndex;
				}
			}
			else
			{
				// Add neighbor to open list
				float heuristic = (neighborTri.center - goalPos).length();
				openList.add(AStarNode(neighborIndex, newGCost, heuristic, currentClosedIndex));
			}
		}
	}

	// No path found
	return false;
}

void NavmeshComponent::trianglePathToWaypoints(const Array<PrimitiveTypes::UInt32>& trianglePath, Array<Vector3>& outWaypoints)
{
	outWaypoints.reset(trianglePath.m_size);

	for (PrimitiveTypes::UInt32 i = 0; i < trianglePath.m_size; i++)
	{
		// Cast away const since Array doesn't have const operator[]
		PrimitiveTypes::UInt32 triIndex = const_cast<Array<PrimitiveTypes::UInt32>&>(trianglePath)[i];
		const NavmeshTriangle& tri = m_triangles[triIndex];
		outWaypoints.add(tri.center);
	}
}

// ============================================================================
// Debug Path Visualization
// ============================================================================

void NavmeshComponent::setDebugPath(const Array<Vector3>& path)
{
	// Copy path to our internal debug path array
	m_debugPath.reset(path.m_size);
	for (PrimitiveTypes::UInt32 i = 0; i < path.m_size; i++)
	{
		m_debugPath.add(const_cast<Array<Vector3>&>(path)[i]);
	}

	// Auto-enable path visualization when path is set
	m_debugPathEnabled = true;

	PEINFO("NavmeshComponent: Debug path set (%d waypoints)\n", m_debugPath.m_size);
}

void NavmeshComponent::clearDebugPath()
{
	m_debugPath.reset(0);
	m_debugPathEnabled = false;
	PEINFO("NavmeshComponent: Debug path cleared\n");
}

}; // namespace Components
}; // namespace PE
