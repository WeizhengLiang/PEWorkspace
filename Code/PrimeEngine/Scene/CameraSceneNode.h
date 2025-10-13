#ifndef __PYENGINE_2_0_CAMERA_SCENE_NODE_H__
#define __PYENGINE_2_0_CAMERA_SCENE_NODE_H__

// API Abstraction
#include "PrimeEngine/APIAbstraction/APIAbstractionDefines.h"

// Outer-Engine includes
#include <assert.h>

// Inter-Engine includes
#include "PrimeEngine/Render/IRenderer.h"
#include "PrimeEngine/MemoryManagement/Handle.h"
#include "PrimeEngine/PrimitiveTypes/PrimitiveTypes.h"
#include "../Events/Component.h"
#include "../Utils/Array/Array.h"
#include "PrimeEngine/Math/CameraOps.h"
#include "PrimeEngine/Math/Vector3.h"

#include "SceneNode.h"


// Sibling/Children includes

namespace PE {
namespace Components {

// Frustum plane structure for culling
struct FrustumPlane
{
	Vector3 normal;    // Plane normal (pointing inward)
	float distance;    // Distance from origin
	
	FrustumPlane() : normal(Vector3(0.0f, 0.0f, 0.0f)), distance(0.0f) {}
	FrustumPlane(const Vector3& n, float d) : normal(n), distance(d) {}
	
	// Test if a point is inside the plane (positive side)
	bool isPointInside(const Vector3& point)
	{
		float dotProduct = normal.dotProduct(point);
		return (dotProduct + distance) >= 0.0f;
	}
	
	// Test if a sphere is inside the plane
	bool isSphereInside(const Vector3& center, float radius)
	{
		float dotProduct = normal.dotProduct(center);
		return (dotProduct + distance) >= -radius;
	}
};

struct CameraSceneNode : public SceneNode
{

	PE_DECLARE_CLASS(CameraSceneNode);

	// Constructor -------------------------------------------------------------
	CameraSceneNode(PE::GameContext &context, PE::MemoryArena arena, Handle hMyself);

	virtual ~CameraSceneNode(){}

	// Component ------------------------------------------------------------
	virtual void addDefaultComponents();

	PE_DECLARE_IMPLEMENT_EVENT_HANDLER_WRAPPER(do_CALCULATE_TRANSFORMATIONS);
	virtual void do_CALCULATE_TRANSFORMATIONS(Events::Event *pEvt);

	// Frustum culling functions
	void computeFrustumPlanes();
	bool isAABBInsideFrustum(const Vector3& min, const Vector3& max);
	bool isSphereInsideFrustum(const Vector3& center, float radius);
	
	// Debug functions
	void drawFrustumPlanesDebug();
	void drawFrustumBoxDebug();

	// Individual events -------------------------------------------------------
	
	Matrix4x4 m_worldToViewTransform; // objects in world space are multiplied by this to get them into camera's coordinate system (view space)
	Matrix4x4 m_worldToViewTransform2;
	Matrix4x4 m_worldTransform2;
	Matrix4x4 m_viewToProjectedTransform; // objects in local (view) space are multiplied by this to get them to screen space
	float m_near, m_far;
	
	// Frustum planes for culling (6 planes: left, right, top, bottom, near, far)
	FrustumPlane m_frustumPlanes[6];
};
}; // namespace Components
}; // namespace PE
#endif
