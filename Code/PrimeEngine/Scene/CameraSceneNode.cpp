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
	// TEMPORARY: Use yellow frustum visualization as the actual culling boundary
	// This lets us see exactly how objects pop in/out at the yellow frustum edges
	
	// Create a custom view-projection matrix using the yellow frustum parameters
	float testNear = 1.5f;   // 1.5 units from camera
	float testFar = 4.0f;    // 4 units from camera (very small!)
	float actualFOV = 0.33f * PrimitiveTypes::Constants::c_Pi_F32;  // Same as yellow frustum
	
	// Create custom projection matrix for yellow frustum
	Matrix4x4 customProj = CameraOps::CreateProjectionMatrix(actualFOV, 
		(float)(m_pContext->getGPUScreen()->getWidth()) / (float)(m_pContext->getGPUScreen()->getHeight()),
		testNear, testFar);
	
	// Use custom projection instead of real camera projection
	Matrix4x4 viewProj = customProj * m_worldToViewTransform;
	
	printf("DEBUG: Using SMALL YELLOW FRUSTUM as actual culling boundary (near=%.2f, far=%.2f)\n", testNear, testFar);
	
	// Extract the 6 frustum planes from the view-projection matrix
	// Each plane is defined by: ax + by + cz + d = 0
	// where (a,b,c) is the normal and d is the distance
	
	// Left plane (column 3 + column 0)
	m_frustumPlanes[0].normal.m_x = (float)(viewProj.m[0][3] + viewProj.m[0][0]);
	m_frustumPlanes[0].normal.m_y = (float)(viewProj.m[1][3] + viewProj.m[1][0]);
	m_frustumPlanes[0].normal.m_z = (float)(viewProj.m[2][3] + viewProj.m[2][0]);
	m_frustumPlanes[0].distance = (float)(viewProj.m[3][3] + viewProj.m[3][0]);
	
	// Right plane (column 3 - column 0)
	m_frustumPlanes[1].normal.m_x = (float)(viewProj.m[0][3] - viewProj.m[0][0]);
	m_frustumPlanes[1].normal.m_y = (float)(viewProj.m[1][3] - viewProj.m[1][0]);
	m_frustumPlanes[1].normal.m_z = (float)(viewProj.m[2][3] - viewProj.m[2][0]);
	m_frustumPlanes[1].distance = (float)(viewProj.m[3][3] - viewProj.m[3][0]);
	
	// Bottom plane (column 3 + column 1)
	m_frustumPlanes[2].normal.m_x = (float)(viewProj.m[0][3] + viewProj.m[0][1]);
	m_frustumPlanes[2].normal.m_y = (float)(viewProj.m[1][3] + viewProj.m[1][1]);
	m_frustumPlanes[2].normal.m_z = (float)(viewProj.m[2][3] + viewProj.m[2][1]);
	m_frustumPlanes[2].distance = (float)(viewProj.m[3][3] + viewProj.m[3][1]);
	
	// Top plane (column 3 - column 1)
	m_frustumPlanes[3].normal.m_x = (float)(viewProj.m[0][3] - viewProj.m[0][1]);
	m_frustumPlanes[3].normal.m_y = (float)(viewProj.m[1][3] - viewProj.m[1][1]);
	m_frustumPlanes[3].normal.m_z = (float)(viewProj.m[2][3] - viewProj.m[2][1]);
	m_frustumPlanes[3].distance = (float)(viewProj.m[3][3] - viewProj.m[3][1]);
	
	// Near plane (column 3 + column 2)
	m_frustumPlanes[4].normal.m_x = (float)(viewProj.m[0][3] + viewProj.m[0][2]);
	m_frustumPlanes[4].normal.m_y = (float)(viewProj.m[1][3] + viewProj.m[1][2]);
	m_frustumPlanes[4].normal.m_z = (float)(viewProj.m[2][3] + viewProj.m[2][2]);
	m_frustumPlanes[4].distance = (float)(viewProj.m[3][3] + viewProj.m[3][2]);
	
	// Far plane (column 3 - column 2)
	m_frustumPlanes[5].normal.m_x = (float)(viewProj.m[0][3] - viewProj.m[0][2]);
	m_frustumPlanes[5].normal.m_y = (float)(viewProj.m[1][3] - viewProj.m[1][2]);
	m_frustumPlanes[5].normal.m_z = (float)(viewProj.m[2][3] - viewProj.m[2][2]);
	m_frustumPlanes[5].distance = (float)(viewProj.m[3][3] - viewProj.m[3][2]);
	
	// Normalize all planes and flip normals to point inward
	for (int i = 0; i < 6; i++)
	{
		float length = m_frustumPlanes[i].normal.length();
		if (length > 0.0f)
		{
			m_frustumPlanes[i].normal = m_frustumPlanes[i].normal / length;
			m_frustumPlanes[i].distance /= length;
			
			// Flip the normal to point inward toward the camera
			m_frustumPlanes[i].normal = m_frustumPlanes[i].normal * -1.0f;
			m_frustumPlanes[i].distance = -m_frustumPlanes[i].distance;
		}
	}
	
	// Debug output to verify plane normals are pointing inward
	printf("DEBUG: YELLOW FRUSTUM plane normals (using yellow frustum as culling boundary):\n");
	const char* planeNames[6] = {"Left", "Right", "Bottom", "Top", "Near", "Far"};
	for (int i = 0; i < 6; i++)
	{
		printf("  %s plane: normal(%.3f, %.3f, %.3f), distance=%.3f\n", 
			planeNames[i], 
			m_frustumPlanes[i].normal.m_x, 
			m_frustumPlanes[i].normal.m_y, 
			m_frustumPlanes[i].normal.m_z,
			m_frustumPlanes[i].distance);
	}
	
	// Test if camera position is inside frustum (it should be!)
	Vector3 cameraPos = m_worldTransform.getPos();
	printf("DEBUG: Testing if camera position (%.3f, %.3f, %.3f) is inside frustum:\n", 
		cameraPos.m_x, cameraPos.m_y, cameraPos.m_z);
	bool cameraInside = true;
	for (int i = 0; i < 6; i++)
	{
		bool inside = m_frustumPlanes[i].isPointInside(cameraPos);
		printf("  %s plane: %s\n", planeNames[i], inside ? "INSIDE" : "OUTSIDE");
		if (!inside) cameraInside = false;
	}
	printf("DEBUG: Camera is %s the frustum\n", cameraInside ? "INSIDE" : "OUTSIDE");
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
	printf("DEBUG: drawFrustumBoxDebug() called\n");
	
	// Get the DebugRenderer instance
	PE::Components::DebugRenderer* pDebugRenderer = PE::Components::DebugRenderer::Instance();
	if (!pDebugRenderer)
	{
		printf("DEBUG: DebugRenderer not available for frustum box visualization\n");
		return;
	}
	
	printf("DEBUG: DebugRenderer found, creating frustum test lines...\n");
	
	// Create a simple frustum box by sampling points at near and far planes
	Vector3 cameraPos = m_worldTransform.getPos();
	Vector3 cameraDir = m_worldTransform.getN();
	Vector3 cameraUp = m_worldTransform.getV();
	Vector3 cameraRight = m_worldTransform.getU();
	
	// Add test lines to make frustum visible from camera perspective
	// Test line 1: Red line extending forward from camera
	Vector3 forwardTest = cameraPos;
	Vector3 forwardOffset = cameraDir;
	forwardOffset *= 20.0f; // 10 units forward
	forwardTest = forwardTest + forwardOffset;
	
	Vector3 testLineData1[4] = {
		cameraPos, Vector3(1.0f, 0.0f, 0.0f), // Red color
		forwardTest, Vector3(1.0f, 0.0f, 0.0f)  // Red color
	};
	
	// Test line 2: Green line extending up from camera
	Vector3 upTest = cameraPos;
	Vector3 upOffset = cameraUp;
	upOffset *= 5.0f; // 5 units up
	upTest = upTest + upOffset;
	
	Vector3 testLineData2[4] = {
		cameraPos, Vector3(0.0f, 1.0f, 0.0f), // Green color
		upTest, Vector3(0.0f, 1.0f, 0.0f)     // Green color
	};
	
	// Test line 3: Blue line extending right from camera
	Vector3 rightTest = cameraPos;
	Vector3 rightOffset = cameraRight;
	rightOffset *= 5.0f; // 5 units right
	rightTest = rightTest + rightOffset;
	
	Vector3 testLineData3[4] = {
		cameraPos, Vector3(0.0f, 0.0f, 1.0f), // Blue color
		rightTest, Vector3(0.0f, 0.0f, 1.0f)  // Blue color
	};
	
	// Create the test lines
	Matrix4x4 identityMatrix;
	identityMatrix.loadIdentity();
	
	printf("DEBUG: Creating RED test line (forward)...\n");
	pDebugRenderer->createLineMesh(true, identityMatrix, &testLineData1[0].m_x, 2, 1.0f, 1.0f);
	printf("DEBUG: Creating GREEN test line (up)...\n");
	pDebugRenderer->createLineMesh(true, identityMatrix, &testLineData2[0].m_x, 2, 1.0f, 1.0f);
	printf("DEBUG: Creating BLUE test line (right)...\n");
	pDebugRenderer->createLineMesh(true, identityMatrix, &testLineData3[0].m_x, 2, 1.0f, 1.0f);
	
	printf("DEBUG: Added test lines - Red=forward, Green=up, Blue=right from camera at (%.2f, %.2f, %.2f)\n",
		cameraPos.m_x, cameraPos.m_y, cameraPos.m_z);
	
	// Calculate frustum dimensions - VERY SMALL FOR CLEAR CULLING BOUNDARIES
	// Match the culling boundary parameters exactly
	float testNear = 1.5f;   // 1.5 units from camera (matches culling)
	float testFar = 4.0f;    // 4 units from camera (matches culling)
	
	// Use actual FOV from camera
	float actualFOV = 0.33f * PrimitiveTypes::Constants::c_Pi_F32;  // Same as in do_CALCULATE_TRANSFORMATIONS
	
	float nearHeight = 2.0f * tan(actualFOV / 2.0f) * testNear;
	float nearWidth = nearHeight * ((float)(m_pContext->getGPUScreen()->getWidth()) / (float)(m_pContext->getGPUScreen()->getHeight()));
	float farHeight = 2.0f * tan(actualFOV / 2.0f) * testFar;
	float farWidth = farHeight * ((float)(m_pContext->getGPUScreen()->getWidth()) / (float)(m_pContext->getGPUScreen()->getHeight()));
	
	// Calculate near and far plane centers - position them IN FRONT of camera so they're visible
	Vector3 nearCenter = cameraPos;
	Vector3 nearOffset = cameraDir;
	nearOffset *= testNear;
	nearCenter = nearCenter + nearOffset;
	
	Vector3 farCenter = cameraPos;
	Vector3 farOffset = cameraDir;
	farOffset *= testFar;
	farCenter = farCenter + farOffset;
	
	// Move the frustum forward so it's visible from camera perspective
	float frustumForwardDistance = 5.0f; // Move frustum 5 units forward from camera
	Vector3 forwardMove = cameraDir;
	forwardMove *= frustumForwardDistance;
	nearCenter = nearCenter + forwardMove;
	farCenter = farCenter + forwardMove;
	
	// Add a test line from near to far center to make frustum visible
	Vector3 frustumTestLine[4] = {
		nearCenter, Vector3(1.0f, 1.0f, 0.0f), // Yellow color
		farCenter, Vector3(1.0f, 1.0f, 0.0f)  // Yellow color
	};
	printf("DEBUG: Creating YELLOW frustum test line...\n");
	pDebugRenderer->createLineMesh(true, identityMatrix, &frustumTestLine[0].m_x, 2, 5.0f, 1.0f);
	
	printf("DEBUG: Added frustum test line from near (%.2f) to far (%.2f) [LARGE VISIBLE VERSION]\n", testNear, testFar);
	
	// Calculate 8 corners of the frustum
	Vector3 rightNear = cameraRight;
	rightNear *= nearWidth;
	Vector3 upNear = cameraUp;
	upNear *= nearHeight;
	Vector3 rightFar = cameraRight;
	rightFar *= farWidth;
	Vector3 upFar = cameraUp;
	upFar *= farHeight;
	
	Vector3 corners[8] = {
		// Near plane corners
		nearCenter + rightNear + upNear,    // near top-right
		nearCenter - rightNear + upNear,    // near top-left
		nearCenter - rightNear - upNear,    // near bottom-left
		nearCenter + rightNear - upNear,    // near bottom-right
		
		// Far plane corners
		farCenter + rightFar + upFar,       // far top-right
		farCenter - rightFar + upFar,       // far top-left
		farCenter - rightFar - upFar,       // far bottom-left
		farCenter + rightFar - upFar        // far bottom-right
	};
	
	// Debug output to show frustum corner positions
	printf("DEBUG: YELLOW FRUSTUM BOX corners:\n");
	printf("  Camera pos: (%.2f, %.2f, %.2f)\n", cameraPos.m_x, cameraPos.m_y, cameraPos.m_z);
	printf("  Near center: (%.2f, %.2f, %.2f)\n", nearCenter.m_x, nearCenter.m_y, nearCenter.m_z);
	printf("  Far center: (%.2f, %.2f, %.2f)\n", farCenter.m_x, farCenter.m_y, farCenter.m_z);
	printf("  Near plane size: %.4f x %.4f\n", nearWidth, nearHeight);
	printf("  Far plane size: %.4f x %.4f\n", farWidth, farHeight);
	for (int i = 0; i < 8; i++) {
		printf("  Corner %d: (%.2f, %.2f, %.2f)\n", i, corners[i].m_x, corners[i].m_y, corners[i].m_z);
	}
	
	// Define the 12 lines for the frustum wireframe
	int lineIndices[12][2] = {
		{0,1}, {1,2}, {2,3}, {3,0}, // near face
		{4,5}, {5,6}, {6,7}, {7,4}, // far face
		{0,4}, {1,5}, {2,6}, {3,7}  // connecting edges
	};
	
	// Create line data
	Vector3 lineData[48]; // 12 lines * 4 values per line
	Vector3 color(1.0f, 1.0f, 0.0f); // Yellow color for frustum
	
	int pointIndex = 0;
	for (int i = 0; i < 12; i++) {
		Vector3 start = corners[lineIndices[i][0]];
		Vector3 end = corners[lineIndices[i][1]];
		
		lineData[pointIndex++] = start;  // position
		lineData[pointIndex++] = color;  // color
		lineData[pointIndex++] = end;    // position
		lineData[pointIndex++] = color;  // color
	}
	
	printf("DEBUG: Creating YELLOW frustum box with %d points (should be 48)\n", pointIndex);
	pDebugRenderer->createLineMesh(
		true, 
		identityMatrix, 
		&lineData[0].m_x, 
		pointIndex, 
		1.0f,  // 1 second lifetime (longer for visibility)
		1.0f
	);
	
	printf("DEBUG: Drew YELLOW frustum box from near (%.2f) to far (%.2f) - lifetime: 5.0s\n", testNear, testFar);
	
	// Add a simple test box at a known location to verify debug rendering works
	Vector3 testBoxCorners[8] = {
		Vector3(0.0f, 0.0f, 0.0f),   // 0: origin
		Vector3(1.0f, 0.0f, 0.0f),   // 1: +X
		Vector3(1.0f, 1.0f, 0.0f),   // 2: +X+Y
		Vector3(0.0f, 1.0f, 0.0f),   // 3: +Y
		Vector3(0.0f, 0.0f, 1.0f),   // 4: +Z
		Vector3(1.0f, 0.0f, 1.0f),   // 5: +X+Z
		Vector3(1.0f, 1.0f, 1.0f),   // 6: +X+Y+Z
		Vector3(0.0f, 1.0f, 1.0f)    // 7: +Y+Z
	};
	
	// Create a simple 1x1x1 box at origin
	Vector3 testBoxLines[48]; // 12 lines * 4 values per line
	Vector3 testBoxColor(0.0f, 1.0f, 1.0f); // Cyan color
	
	int testBoxIndex = 0;
	int testBoxLineIndices[12][2] = {
		{0,1}, {1,2}, {2,3}, {3,0}, // bottom face
		{4,5}, {5,6}, {6,7}, {7,4}, // top face
		{0,4}, {1,5}, {2,6}, {3,7}  // vertical edges
	};
	
	for (int i = 0; i < 12; i++) {
		Vector3 start = testBoxCorners[testBoxLineIndices[i][0]];
		Vector3 end = testBoxCorners[testBoxLineIndices[i][1]];
		
		testBoxLines[testBoxIndex++] = start;  // position
		testBoxLines[testBoxIndex++] = testBoxColor;  // color
		testBoxLines[testBoxIndex++] = end;    // position
		testBoxLines[testBoxIndex++] = testBoxColor;  // color
	}
	
	printf("DEBUG: Creating simple test box at origin (0,0,0) to (1,1,1) - CYAN color\n");
	pDebugRenderer->createLineMesh(true, identityMatrix, &testBoxLines[0].m_x, testBoxIndex, 10.0f, 1.0f);
	
	// Add a simple test frustum at a fixed location to verify it's visible
	Vector3 fixedFrustumCorners[8] = {
		// Near plane (small square)
		Vector3(0.0f, 0.0f, 0.0f),   // 0: near bottom-left
		Vector3(0.2f, 0.0f, 0.0f),   // 1: near bottom-right
		Vector3(0.2f, 0.2f, 0.0f),   // 2: near top-right
		Vector3(0.0f, 0.2f, 0.0f),   // 3: near top-left
		
		// Far plane (larger square)
		Vector3(0.0f, 0.0f, 1.0f),   // 4: far bottom-left
		Vector3(0.4f, 0.0f, 1.0f),   // 5: far bottom-right
		Vector3(0.4f, 0.4f, 1.0f),   // 6: far top-right
		Vector3(0.0f, 0.4f, 1.0f)    // 7: far top-left
	};
	
	// Create a simple fixed frustum
	Vector3 fixedFrustumLines[48]; // 12 lines * 4 values per line
	Vector3 fixedFrustumColor(1.0f, 0.0f, 1.0f); // Magenta color
	
	int fixedFrustumIndex = 0;
	int fixedFrustumLineIndices[12][2] = {
		{0,1}, {1,2}, {2,3}, {3,0}, // near face
		{4,5}, {5,6}, {6,7}, {7,4}, // far face
		{0,4}, {1,5}, {2,6}, {3,7}  // connecting edges
	};
	
	for (int i = 0; i < 12; i++) {
		Vector3 start = fixedFrustumCorners[fixedFrustumLineIndices[i][0]];
		Vector3 end = fixedFrustumCorners[fixedFrustumLineIndices[i][1]];
		
		fixedFrustumLines[fixedFrustumIndex++] = start;  // position
		fixedFrustumLines[fixedFrustumIndex++] = fixedFrustumColor;  // color
		fixedFrustumLines[fixedFrustumIndex++] = end;    // position
		fixedFrustumLines[fixedFrustumIndex++] = fixedFrustumColor;  // color
	}
	
	printf("DEBUG: Creating fixed test frustum at origin - MAGENTA color\n");
	pDebugRenderer->createLineMesh(true, identityMatrix, &fixedFrustumLines[0].m_x, fixedFrustumIndex, 10.0f, 1.0f);
	
#endif
}

}; // namespace Components
}; // namespace PE
