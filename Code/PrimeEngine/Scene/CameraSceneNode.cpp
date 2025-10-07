#include "CameraSceneNode.h"
#include "../Lua/LuaEnvironment.h"
#include "PrimeEngine/Events/StandardEvents.h"
#include "PrimeEngine/Scene/DebugRenderer.h"

#define Z_ONLY_CAM_BIAS 0.0f
namespace PE {
namespace Components {

PE_IMPLEMENT_CLASS1(CameraSceneNode, SceneNode);

CameraSceneNode::CameraSceneNode(PE::GameContext &context, PE::MemoryArena arena, Handle hMyself) : SceneNode(context, arena, hMyself)
{
	m_near = 0.05f;
	m_far = 2000.0f;
}
void CameraSceneNode::addDefaultComponents()
{
	Component::addDefaultComponents();
	PE_REGISTER_EVENT_HANDLER(Events::Event_CALCULATE_TRANSFORMATIONS, CameraSceneNode::do_CALCULATE_TRANSFORMATIONS);
}

void CameraSceneNode::do_CALCULATE_TRANSFORMATIONS(Events::Event *pEvt)
{
	Handle hParentSN = getFirstParentByType<SceneNode>();
	if (hParentSN.isValid())
	{
		Matrix4x4 parentTransform = hParentSN.getObject<PE::Components::SceneNode>()->m_worldTransform;
		m_worldTransform = parentTransform * m_base;
	}
	
	Matrix4x4 &mref_worldTransform = m_worldTransform;

	Vector3 pos = Vector3(mref_worldTransform.m[0][3], mref_worldTransform.m[1][3], mref_worldTransform.m[2][3]);
	Vector3 n = Vector3(mref_worldTransform.m[0][2], mref_worldTransform.m[1][2], mref_worldTransform.m[2][2]);
	Vector3 target = pos + n;
	Vector3 up = Vector3(mref_worldTransform.m[0][1], mref_worldTransform.m[1][1], mref_worldTransform.m[2][1]);

	m_worldToViewTransform = CameraOps::CreateViewMatrix(pos, target, up);

	m_worldTransform2 = mref_worldTransform;

	m_worldTransform2.moveForward(Z_ONLY_CAM_BIAS);

	Vector3 pos2 = Vector3(m_worldTransform2.m[0][3], m_worldTransform2.m[1][3], m_worldTransform2.m[2][3]);
	Vector3 n2 = Vector3(m_worldTransform2.m[0][2], m_worldTransform2.m[1][2], m_worldTransform2.m[2][2]);
	Vector3 target2 = pos2 + n2;
	Vector3 up2 = Vector3(m_worldTransform2.m[0][1], m_worldTransform2.m[1][1], m_worldTransform2.m[2][1]);

	m_worldToViewTransform2 = CameraOps::CreateViewMatrix(pos2, target2, up2);
    
    PrimitiveTypes::Float32 aspect = (PrimitiveTypes::Float32)(m_pContext->getGPUScreen()->getWidth()) / (PrimitiveTypes::Float32)(m_pContext->getGPUScreen()->getHeight());
    
    PrimitiveTypes::Float32 verticalFov = 0.33f * PrimitiveTypes::Constants::c_Pi_F32;
    if (aspect < 1.0f)
    {
        //ios portrait view
        static PrimitiveTypes::Float32 factor = 0.5f;
        verticalFov *= factor;
    }

	// Use real camera parameters for rendering
	m_viewToProjectedTransform = CameraOps::CreateProjectionMatrix(verticalFov, 
		aspect,
		m_near, m_far);
	
	// Compute frustum planes for culling
	computeFrustumPlanes();
	
	// Draw frustum for debugging
	drawFrustumBoxDebug();
	
	SceneNode::do_CALCULATE_TRANSFORMATIONS(pEvt);

}

void CameraSceneNode::computeFrustumPlanes()
{
	// Use REAL camera frustum for culling but with YELLOW FRUSTUM planes
	// This means we use the real camera's projection matrix but extract planes from yellow frustum
	
	// Create yellow frustum projection matrix for plane extraction
	float yellowNear = 1.0f;   // Yellow frustum near plane (close to camera)
	float yellowFar = 50.0f;   // Yellow frustum far plane (reasonable range)
	float actualFOV = 0.33f * PrimitiveTypes::Constants::c_Pi_F32;  // Same as real camera
	
	// Create yellow frustum projection matrix
	Matrix4x4 yellowProj = CameraOps::CreateProjectionMatrix(actualFOV, 
		(float)(m_pContext->getGPUScreen()->getWidth()) / (float)(m_pContext->getGPUScreen()->getHeight()),
		yellowNear, yellowFar);
	
	// Use yellow frustum projection for plane extraction
	// Standard matrix order: Projection * View
	Matrix4x4 viewProj = yellowProj * m_worldToViewTransform;
	
	// Extract the 6 frustum planes from the view-projection matrix
	// Each plane is defined by: ax + by + cz + d = 0
	// where (a,b,c) is the normal and d is the distance
	// Using row extraction (transposed for this engine's matrix layout)
	
	// Left plane (row 3 + row 0)
	m_frustumPlanes[0].normal.m_x = (float)(viewProj.m[3][0] + viewProj.m[0][0]);
	m_frustumPlanes[0].normal.m_y = (float)(viewProj.m[3][1] + viewProj.m[0][1]);
	m_frustumPlanes[0].normal.m_z = (float)(viewProj.m[3][2] + viewProj.m[0][2]);
	m_frustumPlanes[0].distance = (float)(viewProj.m[3][3] + viewProj.m[0][3]);
	
	// Right plane (row 3 - row 0)
	m_frustumPlanes[1].normal.m_x = (float)(viewProj.m[3][0] - viewProj.m[0][0]);
	m_frustumPlanes[1].normal.m_y = (float)(viewProj.m[3][1] - viewProj.m[0][1]);
	m_frustumPlanes[1].normal.m_z = (float)(viewProj.m[3][2] - viewProj.m[0][2]);
	m_frustumPlanes[1].distance = (float)(viewProj.m[3][3] - viewProj.m[0][3]);
	
	// Bottom plane (row 3 + row 1)
	m_frustumPlanes[2].normal.m_x = (float)(viewProj.m[3][0] + viewProj.m[1][0]);
	m_frustumPlanes[2].normal.m_y = (float)(viewProj.m[3][1] + viewProj.m[1][1]);
	m_frustumPlanes[2].normal.m_z = (float)(viewProj.m[3][2] + viewProj.m[1][2]);
	m_frustumPlanes[2].distance = (float)(viewProj.m[3][3] + viewProj.m[1][3]);
	
	// Top plane (row 3 - row 1)
	m_frustumPlanes[3].normal.m_x = (float)(viewProj.m[3][0] - viewProj.m[1][0]);
	m_frustumPlanes[3].normal.m_y = (float)(viewProj.m[3][1] - viewProj.m[1][1]);
	m_frustumPlanes[3].normal.m_z = (float)(viewProj.m[3][2] - viewProj.m[1][2]);
	m_frustumPlanes[3].distance = (float)(viewProj.m[3][3] - viewProj.m[1][3]);
	
	// Near plane (row 3 + row 2)
	m_frustumPlanes[4].normal.m_x = (float)(viewProj.m[3][0] + viewProj.m[2][0]);
	m_frustumPlanes[4].normal.m_y = (float)(viewProj.m[3][1] + viewProj.m[2][1]);
	m_frustumPlanes[4].normal.m_z = (float)(viewProj.m[3][2] + viewProj.m[2][2]);
	m_frustumPlanes[4].distance = (float)(viewProj.m[3][3] + viewProj.m[2][3]);
	
	// Far plane (row 3 - row 2)
	m_frustumPlanes[5].normal.m_x = (float)(viewProj.m[3][0] - viewProj.m[2][0]);
	m_frustumPlanes[5].normal.m_y = (float)(viewProj.m[3][1] - viewProj.m[2][1]);
	m_frustumPlanes[5].normal.m_z = (float)(viewProj.m[3][2] - viewProj.m[2][2]);
	m_frustumPlanes[5].distance = (float)(viewProj.m[3][3] - viewProj.m[2][3]);
	
	// Normalize all planes and flip to point inward
	for (int i = 0; i < 6; i++)
	{
		float length = m_frustumPlanes[i].normal.length();
		if (length > 0.0f)
		{
			m_frustumPlanes[i].normal = m_frustumPlanes[i].normal / length;
			m_frustumPlanes[i].distance /= length;
			
			// Flip normals to point inward
			m_frustumPlanes[i].normal = m_frustumPlanes[i].normal * -1.0f;
			m_frustumPlanes[i].distance = -m_frustumPlanes[i].distance;
		}
	}
}

bool CameraSceneNode::isAABBInsideFrustum(const Vector3& min, const Vector3& max)
{
	// Test AABB against all 6 frustum planes
	for (int i = 0; i < 6; i++)
	{
		// Find the positive vertex (the vertex that is most in the positive direction of the plane normal)
		Vector3 positiveVertex;
		positiveVertex.m_x = (m_frustumPlanes[i].normal.m_x >= 0.0f) ? max.m_x : min.m_x;
		positiveVertex.m_y = (m_frustumPlanes[i].normal.m_y >= 0.0f) ? max.m_y : min.m_y;
		positiveVertex.m_z = (m_frustumPlanes[i].normal.m_z >= 0.0f) ? max.m_z : min.m_z;
		
		// If the positive vertex is outside the plane, the entire AABB is outside
		if (!m_frustumPlanes[i].isPointInside(positiveVertex))
		{
			return false;
		}
	}
	
	return true;
}

bool CameraSceneNode::isSphereInsideFrustum(const Vector3& center, float radius)
{
	// Test sphere against all 6 frustum planes
	for (int i = 0; i < 6; i++)
	{
		if (!m_frustumPlanes[i].isSphereInside(center, radius))
		{
			return false;
		}
	}
	
	return true;
}

void CameraSceneNode::drawFrustumPlanesDebug()
{
#ifdef _DEBUG
	// Get the DebugRenderer instance
	DebugRenderer* pDebugRenderer = PE::Components::DebugRenderer::Instance();
	if (!pDebugRenderer)
	{
		printf("DEBUG: DebugRenderer not available for frustum plane visualization\n");
		return;
	}
	
	printf("DEBUG: Drawing frustum planes for camera at position (%.2f, %.2f, %.2f)\n",
		m_worldTransform.getPos().m_x, m_worldTransform.getPos().m_y, m_worldTransform.getPos().m_z);
	
	// Draw each frustum plane as a wireframe rectangle
	for (int planeIndex = 0; planeIndex < 6; planeIndex++)
	{
		const FrustumPlane& plane = m_frustumPlanes[planeIndex];
		
		// Create a rectangle on the plane for visualization
		// We'll create a 2x2 unit rectangle centered on the plane
		Vector3 planeCenter = plane.normal;
		planeCenter *= (-plane.distance);
		
		// Create two perpendicular vectors on the plane
		Vector3 u, v;
		if (fabs(plane.normal.m_x) < 0.9f)
		{
			u = Vector3(1.0f, 0.0f, 0.0f);
		}
		else
		{
			u = Vector3(0.0f, 1.0f, 0.0f);
		}
		
		// Make u perpendicular to the plane normal
		float dot = u.dotProduct(plane.normal);
		Vector3 projection = plane.normal;
		projection *= dot;
		u = u - projection;
		u.normalize();
		
		// v = normal x u (cross product)
		Vector3 normal = plane.normal;
		v = normal.crossProduct(u);
		v.normalize();
		
		// Create 4 corners of the rectangle
		float size = 5.0f; // Size of the rectangle
		Vector3 uScaled = u;
		uScaled *= size;
		Vector3 vScaled = v;
		vScaled *= size;
		
		Vector3 corners[4] = {
			planeCenter + uScaled + vScaled,  // top-right
			planeCenter - uScaled + vScaled,  // top-left
			planeCenter - uScaled - vScaled,  // bottom-left
			planeCenter + uScaled - vScaled   // bottom-right
		};
		
		// Define colors for each plane
		Vector3 colors[6] = {
			Vector3(1.0f, 0.0f, 0.0f), // Left - Red
			Vector3(0.0f, 1.0f, 0.0f), // Right - Green
			Vector3(0.0f, 0.0f, 1.0f), // Bottom - Blue
			Vector3(1.0f, 1.0f, 0.0f), // Top - Yellow
			Vector3(1.0f, 0.0f, 1.0f), // Near - Magenta
			Vector3(0.0f, 1.0f, 1.0f)  // Far - Cyan
		};
		
		Vector3 color = colors[planeIndex];
		
		// Create line data for the rectangle (4 lines)
		Vector3 lineData[16]; // 4 lines * 4 values per line (2 points * 2 values per point)
		
		// Line 1: corner[0] to corner[1]
		lineData[0] = corners[0];  // position
		lineData[1] = color;       // color
		lineData[2] = corners[1];  // position
		lineData[3] = color;       // color
		
		// Line 2: corner[1] to corner[2]
		lineData[4] = corners[1];  // position
		lineData[5] = color;       // color
		lineData[6] = corners[2];  // position
		lineData[7] = color;       // color
		
		// Line 3: corner[2] to corner[3]
		lineData[8] = corners[2];  // position
		lineData[9] = color;       // color
		lineData[10] = corners[3]; // position
		lineData[11] = color;      // color
		
		// Line 4: corner[3] to corner[0]
		lineData[12] = corners[3]; // position
		lineData[13] = color;      // color
		lineData[14] = corners[0]; // position
		lineData[15] = color;      // color
		
		// Create the debug lines
		Matrix4x4 identityMatrix;
		identityMatrix.loadIdentity();
		
		pDebugRenderer->createLineMesh(
			true, 
			identityMatrix, 
			&lineData[0].m_x, 
			8, // 4 lines * 2 points per line
			0.1f,  // 0.1 second lifetime (refreshes every frame)
			1.0f
		);
		
		printf("DEBUG: Drew plane %d at center (%.2f, %.2f, %.2f) with normal (%.2f, %.2f, %.2f)\n",
			planeIndex, planeCenter.m_x, planeCenter.m_y, planeCenter.m_z,
			plane.normal.m_x, plane.normal.m_y, plane.normal.m_z);
	}
	
	printf("DEBUG: Finished drawing all 6 frustum planes\n");
#endif
}

void CameraSceneNode::drawFrustumBoxDebug()
{
#ifdef _DEBUG
	// Get the DebugRenderer instance
	PE::Components::DebugRenderer* pDebugRenderer = PE::Components::DebugRenderer::Instance();
	if (!pDebugRenderer)
	{
		return;
	}
	
	// Draw frustum box matching the actual culling frustum
	Vector3 cameraPos = m_worldTransform.getPos();
	Vector3 cameraDir = m_worldTransform.getN();
	Vector3 cameraUp = m_worldTransform.getV();
	Vector3 cameraRight = m_worldTransform.getU();
	
	// Match culling frustum parameters (same as in computeFrustumPlanes)
	float frustumNear = 1.0f;
	float frustumFar = 50.0f;
	float actualFOV = 0.33f * PrimitiveTypes::Constants::c_Pi_F32;
	
	// Calculate frustum dimensions
	float aspectRatio = (float)(m_pContext->getGPUScreen()->getWidth()) / (float)(m_pContext->getGPUScreen()->getHeight());
	float nearHeight = 2.0f * tan(actualFOV / 2.0f) * frustumNear;
	float nearWidth = nearHeight * aspectRatio;
	float farHeight = 2.0f * tan(actualFOV / 2.0f) * frustumFar;
	float farWidth = farHeight * aspectRatio;
	
	// Calculate near and far plane centers
	Vector3 nearCenter = cameraPos + (cameraDir * frustumNear);
	Vector3 farCenter = cameraPos + (cameraDir * frustumFar);
	
	// Calculate 8 corners of the frustum
	Vector3 corners[8] = {
		// Near plane corners
		nearCenter + (cameraRight * nearWidth) + (cameraUp * nearHeight),
		nearCenter - (cameraRight * nearWidth) + (cameraUp * nearHeight),
		nearCenter - (cameraRight * nearWidth) - (cameraUp * nearHeight),
		nearCenter + (cameraRight * nearWidth) - (cameraUp * nearHeight),
		
		// Far plane corners
		farCenter + (cameraRight * farWidth) + (cameraUp * farHeight),
		farCenter - (cameraRight * farWidth) + (cameraUp * farHeight),
		farCenter - (cameraRight * farWidth) - (cameraUp * farHeight),
		farCenter + (cameraRight * farWidth) - (cameraUp * farHeight)
	};
	
	// Define the 12 lines for the frustum wireframe
	int lineIndices[12][2] = {
		{0,1}, {1,2}, {2,3}, {3,0}, // near face
		{4,5}, {5,6}, {6,7}, {7,4}, // far face
		{0,4}, {1,5}, {2,6}, {3,7}  // connecting edges
	};
	
	// Create line data
	Vector3 lineData[48];
	Vector3 color(1.0f, 1.0f, 0.0f); // Yellow color for frustum visualization
	
	int pointIndex = 0;
	for (int i = 0; i < 12; i++) {
		lineData[pointIndex++] = corners[lineIndices[i][0]];
		lineData[pointIndex++] = color;
		lineData[pointIndex++] = corners[lineIndices[i][1]];
		lineData[pointIndex++] = color;
	}
	
	Matrix4x4 identityMatrix;
	identityMatrix.loadIdentity();
	
	pDebugRenderer->createLineMesh(
		true, 
		identityMatrix, 
		&lineData[0].m_x, 
		pointIndex, 
		0.1f,  // Short lifetime, refreshes every frame
		1.0f
	);
	
#endif
}

}; // namespace Components
}; // namespace PE
