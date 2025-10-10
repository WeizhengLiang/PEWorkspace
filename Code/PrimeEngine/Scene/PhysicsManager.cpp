#define NOMINMAX
#include "PhysicsManager.h"
#include "PrimeEngine/APIAbstraction/APIAbstractionDefines.h"
#include "PrimeEngine/Lua/LuaEnvironment.h"
#include "PrimeEngine/Events/StandardEvents.h"
#include "SceneNode.h"
#include "DebugRenderer.h"
#include <cmath>

namespace PE {
namespace Components {

PE_IMPLEMENT_CLASS1(PhysicsManager, Component);

// Singleton pattern
Handle PhysicsManager::s_hInstance;

//Constructor
PhysicsManager::PhysicsManager(PE::GameContext &context, PE::MemoryArena arena, Handle hMyself)
: Component(context, arena, hMyself)
, m_physicsComponents(context, arena, 256)  // Initial capacity of 256 physics components
{
}

void PhysicsManager::addDefaultComponents()
{
	Component::addDefaultComponents();
	
	// Register event handlers
	PE_REGISTER_EVENT_HANDLER(PE::Events::Event_PHYSICS_DEBUG_RENDER, PhysicsManager::do_PHYSICS_DEBUG_RENDER);
}

void PhysicsManager::Construct(PE::GameContext &context, PE::MemoryArena arena)
{
	Handle h("PHYSICS_MANAGER", sizeof(PhysicsManager));
	PhysicsManager *pPhysicsManager = new(h) PhysicsManager(context, arena, h);
	pPhysicsManager->addDefaultComponents();
	SetInstance(h);
}

void PhysicsManager::SetInstance(Handle h){s_hInstance = h;}
PhysicsManager *PhysicsManager::Instance(){return s_hInstance.getObject<PhysicsManager>();}

void PhysicsManager::addComponent(Handle hPhysicsComponent)
{
	m_physicsComponents.add(hPhysicsComponent);
	PEINFO("PhysicsManager: Added physics component. Total count: %d\n", m_physicsComponents.m_size);
}

// Helper function: Test collision between sphere and AABB
static bool testSphereAABB(const PhysicsComponent* sphere, const PhysicsComponent* aabb, CollisionInfo& outInfo)
{
	// Get sphere center and radius
	Vector3 sphereCenter = sphere->position;
	float sphereRadius = sphere->sphereRadius;
	
	// Get AABB min and max (already in world space, accounts for rotation!)
	Vector3 aabbMin = aabb->worldAABBMin;
	Vector3 aabbMax = aabb->worldAABBMax;
	
	// Debug: Check if AABB is uninitialized (all zeros)
	#ifdef _DEBUG
	static bool s_warnedAboutZeroAABB = false;
	if (!s_warnedAboutZeroAABB && 
	    aabbMin.m_x == 0.0f && aabbMin.m_y == 0.0f && aabbMin.m_z == 0.0f &&
	    aabbMax.m_x == 0.0f && aabbMax.m_y == 0.0f && aabbMax.m_z == 0.0f)
	{
		PEINFO("WARNING: AABB has zero bounds! Not initialized?\n");
		s_warnedAboutZeroAABB = true;
	}
	#endif
	
	// Find closest point on AABB to sphere center
	Vector3 closestPoint;
	closestPoint.m_x = sphereCenter.m_x < aabbMin.m_x ? aabbMin.m_x : (sphereCenter.m_x > aabbMax.m_x ? aabbMax.m_x : sphereCenter.m_x);
	closestPoint.m_y = sphereCenter.m_y < aabbMin.m_y ? aabbMin.m_y : (sphereCenter.m_y > aabbMax.m_y ? aabbMax.m_y : sphereCenter.m_y);
	closestPoint.m_z = sphereCenter.m_z < aabbMin.m_z ? aabbMin.m_z : (sphereCenter.m_z > aabbMax.m_z ? aabbMax.m_z : sphereCenter.m_z);
	
	// Calculate distance from sphere center to closest point
	Vector3 diff = Vector3(
		sphereCenter.m_x - closestPoint.m_x,
		sphereCenter.m_y - closestPoint.m_y,
		sphereCenter.m_z - closestPoint.m_z
	);
	float distanceSquared = diff.m_x * diff.m_x + diff.m_y * diff.m_y + diff.m_z * diff.m_z;
	float distance = sqrtf(distanceSquared);
	
	// Check if collision occurred
	if (distance < sphereRadius)
	{
		// Collision detected!
		outInfo.hasCollision = true;
		outInfo.object1 = const_cast<PhysicsComponent*>(sphere);
		outInfo.object2 = const_cast<PhysicsComponent*>(aabb);
		outInfo.penetrationDepth = sphereRadius - distance;
		
		// Calculate collision normal (from AABB to sphere)
		if (distance > 0.001f)  // Avoid division by zero
		{
			outInfo.normal = Vector3(
				diff.m_x / distance,
				diff.m_y / distance,
				diff.m_z / distance
			);
		}
		else
		{
			// Sphere center is inside AABB - use direction to closest face
			outInfo.normal = Vector3(0.0f, 1.0f, 0.0f);  // Default: push up
		}
		
		return true;
	}
	
	return false;
}

void PhysicsManager::update(float deltaTime)
{
    // Gravity constant (m/s^2) - negative Y is down
    const float GRAVITY = -10.0f;
    
    // AABB scale factor: reduce if AABBs are too conservative (1.0 = exact, 0.8 = 20% smaller)
    // WARNING: Values < 1.0 can cause tunneling (objects pass through walls)!
    const float AABB_SCALE_FACTOR = 1.0f;  // Set to 0.9 or 0.8 if AABBs too large
    
    // PHASE 0: Sync physics positions from SceneNode world transforms (for newly created/static objects)
    static bool s_firstFrame = true;
    for (PrimitiveTypes::UInt32 i = 0; i < m_physicsComponents.m_size; i++)
    {
        PhysicsComponent *pPhysics = m_physicsComponents[i].getObject<PhysicsComponent>();
        
        if (pPhysics && pPhysics->m_linkedSceneNode && pPhysics->isStatic)
        {
            // For static objects, always sync from SceneNode
            // Get the world transform from the SceneNode
            Matrix4x4& worldTransform = pPhysics->m_linkedSceneNode->m_worldTransform;
            
            // Extract world position from the transform
            Vector3 sceneNodeWorldPos = Vector3(
                worldTransform.m[0][3],
                worldTransform.m[1][3],
                worldTransform.m[2][3]
            );
            
            // Transform the local center offset by rotation only (not translation)
            // offset_world = rotation * offset_local
            Vector3 worldCenterOffset = Vector3(
                worldTransform.m[0][0] * pPhysics->localCenterOffset.m_x + 
                worldTransform.m[0][1] * pPhysics->localCenterOffset.m_y + 
                worldTransform.m[0][2] * pPhysics->localCenterOffset.m_z,
                
                worldTransform.m[1][0] * pPhysics->localCenterOffset.m_x + 
                worldTransform.m[1][1] * pPhysics->localCenterOffset.m_y + 
                worldTransform.m[1][2] * pPhysics->localCenterOffset.m_z,
                
                worldTransform.m[2][0] * pPhysics->localCenterOffset.m_x + 
                worldTransform.m[2][1] * pPhysics->localCenterOffset.m_y + 
                worldTransform.m[2][2] * pPhysics->localCenterOffset.m_z
            );
            
            // Physics position = SceneNode world position + rotated center offset
            pPhysics->position = sceneNodeWorldPos + worldCenterOffset;
            
            // For AABBs: Calculate world-space AABB from rotated local corners
            // (Industry standard: AABBs are axis-aligned in WORLD space, not local space)
            if (pPhysics->shapeType == PhysicsComponent::AABB)
            {
                // Get the 8 corners of the local AABB
                Vector3 localMin = pPhysics->localCenterOffset - pPhysics->aabbExtents;
                Vector3 localMax = pPhysics->localCenterOffset + pPhysics->aabbExtents;
                
                Vector3 localCorners[8] = {
                    Vector3(localMin.m_x, localMin.m_y, localMin.m_z),
                    Vector3(localMax.m_x, localMin.m_y, localMin.m_z),
                    Vector3(localMax.m_x, localMax.m_y, localMin.m_z),
                    Vector3(localMin.m_x, localMax.m_y, localMin.m_z),
                    Vector3(localMin.m_x, localMin.m_y, localMax.m_z),
                    Vector3(localMax.m_x, localMin.m_y, localMax.m_z),
                    Vector3(localMax.m_x, localMax.m_y, localMax.m_z),
                    Vector3(localMin.m_x, localMax.m_y, localMax.m_z)
                };
                
                // Transform all 8 corners to world space
                Vector3 worldCorners[8];
                for (int j = 0; j < 8; j++)
                {
                    worldCorners[j] = worldTransform * localCorners[j];
                }
                
                // Find axis-aligned min/max in world space
                pPhysics->worldAABBMin = worldCorners[0];
                pPhysics->worldAABBMax = worldCorners[0];
                for (int j = 1; j < 8; j++)
                {
                    pPhysics->worldAABBMin.m_x = (worldCorners[j].m_x < pPhysics->worldAABBMin.m_x) ? worldCorners[j].m_x : pPhysics->worldAABBMin.m_x;
                    pPhysics->worldAABBMin.m_y = (worldCorners[j].m_y < pPhysics->worldAABBMin.m_y) ? worldCorners[j].m_y : pPhysics->worldAABBMin.m_y;
                    pPhysics->worldAABBMin.m_z = (worldCorners[j].m_z < pPhysics->worldAABBMin.m_z) ? worldCorners[j].m_z : pPhysics->worldAABBMin.m_z;
                    
                    pPhysics->worldAABBMax.m_x = (worldCorners[j].m_x > pPhysics->worldAABBMax.m_x) ? worldCorners[j].m_x : pPhysics->worldAABBMax.m_x;
                    pPhysics->worldAABBMax.m_y = (worldCorners[j].m_y > pPhysics->worldAABBMax.m_y) ? worldCorners[j].m_y : pPhysics->worldAABBMax.m_y;
                    pPhysics->worldAABBMax.m_z = (worldCorners[j].m_z > pPhysics->worldAABBMax.m_z) ? worldCorners[j].m_z : pPhysics->worldAABBMax.m_z;
                }
                
                #ifdef _DEBUG
                if (s_firstFrame)
                {
                    PEINFO("Static AABB %d: min=(%.2f, %.2f, %.2f) max=(%.2f, %.2f, %.2f)\n",
                        i, pPhysics->worldAABBMin.m_x, pPhysics->worldAABBMin.m_y, pPhysics->worldAABBMin.m_z,
                        pPhysics->worldAABBMax.m_x, pPhysics->worldAABBMax.m_y, pPhysics->worldAABBMax.m_z);
                }
                #endif
                
                // Optional: Scale AABB if too conservative (shrink toward center)
                if (AABB_SCALE_FACTOR < 1.0f)
                {
                    Vector3 center = Vector3(
                        (pPhysics->worldAABBMin.m_x + pPhysics->worldAABBMax.m_x) * 0.5f,
                        (pPhysics->worldAABBMin.m_y + pPhysics->worldAABBMax.m_y) * 0.5f,
                        (pPhysics->worldAABBMin.m_z + pPhysics->worldAABBMax.m_z) * 0.5f
                    );
                    Vector3 halfExtents = Vector3(
                        (pPhysics->worldAABBMax.m_x - pPhysics->worldAABBMin.m_x) * 0.5f,
                        (pPhysics->worldAABBMax.m_y - pPhysics->worldAABBMin.m_y) * 0.5f,
                        (pPhysics->worldAABBMax.m_z - pPhysics->worldAABBMin.m_z) * 0.5f
                    );
                    Vector3 scaledExtents = Vector3(
                        halfExtents.m_x * AABB_SCALE_FACTOR,
                        halfExtents.m_y * AABB_SCALE_FACTOR,
                        halfExtents.m_z * AABB_SCALE_FACTOR
                    );
                    pPhysics->worldAABBMin = center - scaledExtents;
                    pPhysics->worldAABBMax = center + scaledExtents;
                }
            }
        }
    }
    
    // PHASE 1: Apply forces and integrate (update positions)
    for (PrimitiveTypes::UInt32 i = 0; i < m_physicsComponents.m_size; i++)
    {
        PhysicsComponent *pPhysics = m_physicsComponents[i].getObject<PhysicsComponent>();
        
        if (!pPhysics || pPhysics->isStatic)
            continue;  // Skip static objects
        
        // Apply gravity
        pPhysics->acceleration.m_y = GRAVITY;
        
        // Update velocity: v = v0 + a*dt
        pPhysics->velocity.m_x += pPhysics->acceleration.m_x * deltaTime;
        pPhysics->velocity.m_y += pPhysics->acceleration.m_y * deltaTime;
        pPhysics->velocity.m_z += pPhysics->acceleration.m_z * deltaTime;
        
        // Update position: p = p0 + v*dt
        pPhysics->position.m_x += pPhysics->velocity.m_x * deltaTime;
        pPhysics->position.m_y += pPhysics->velocity.m_y * deltaTime;
        pPhysics->position.m_z += pPhysics->velocity.m_z * deltaTime;
    }
    
    // PHASE 2: Detect collisions
    Array<CollisionInfo> collisions(*m_pContext, m_arena, 64);
    
    for (PrimitiveTypes::UInt32 i = 0; i < m_physicsComponents.m_size; i++)
    {
        PhysicsComponent *pDynamic = m_physicsComponents[i].getObject<PhysicsComponent>();
        if (!pDynamic || pDynamic->isStatic || pDynamic->shapeType != PhysicsComponent::SPHERE)
            continue;  // Only test dynamic spheres
        
        // Test against all static AABBs
        for (PrimitiveTypes::UInt32 j = 0; j < m_physicsComponents.m_size; j++)
        {
            if (i == j) continue;  // Don't test against self
            
            PhysicsComponent *pStatic = m_physicsComponents[j].getObject<PhysicsComponent>();
            if (!pStatic || !pStatic->isStatic || pStatic->shapeType != PhysicsComponent::AABB)
                continue;  // Only test against static AABBs
            
            // Test collision
            CollisionInfo info;
            if (testSphereAABB(pDynamic, pStatic, info))
            {
                collisions.add(info);
            }
        }
    }
    
    // PHASE 3: Resolve collisions (simple response for now)
    #ifdef _DEBUG
    static int frameCount = 0;
    if (collisions.m_size > 0 && (frameCount++ % 60 == 0))  // Log every 60 frames
    {
        PEINFO("Physics: %d collisions detected (frame %d)\n", collisions.m_size, frameCount);
        for (PrimitiveTypes::UInt32 i = 0; i < collisions.m_size && i < 3; i++)  // Show first 3
        {
            PEINFO("  Collision %d: sphere at (%.2f, %.2f, %.2f), AABB min=(%.2f, %.2f, %.2f) max=(%.2f, %.2f, %.2f)\n",
                i, collisions[i].object1->position.m_x, collisions[i].object1->position.m_y, collisions[i].object1->position.m_z,
                collisions[i].object2->worldAABBMin.m_x, collisions[i].object2->worldAABBMin.m_y, collisions[i].object2->worldAABBMin.m_z,
                collisions[i].object2->worldAABBMax.m_x, collisions[i].object2->worldAABBMax.m_y, collisions[i].object2->worldAABBMax.m_z);
        }
    }
    #endif
    
    for (PrimitiveTypes::UInt32 i = 0; i < collisions.m_size; i++)
    {
        CollisionInfo& collision = collisions[i];
        
        // Separate the objects
        collision.object1->position.m_x += collision.normal.m_x * collision.penetrationDepth;
        collision.object1->position.m_y += collision.normal.m_y * collision.penetrationDepth;
        collision.object1->position.m_z += collision.normal.m_z * collision.penetrationDepth;
        
        // Stop velocity in the direction of the normal (prevent sinking)
        float velocityAlongNormal = 
            collision.object1->velocity.m_x * collision.normal.m_x +
            collision.object1->velocity.m_y * collision.normal.m_y +
            collision.object1->velocity.m_z * collision.normal.m_z;
        
        if (velocityAlongNormal < 0.0f)  // Moving into the surface
        {
            collision.object1->velocity.m_x -= velocityAlongNormal * collision.normal.m_x;
            collision.object1->velocity.m_y -= velocityAlongNormal * collision.normal.m_y;
            collision.object1->velocity.m_z -= velocityAlongNormal * collision.normal.m_z;
        }
    }
    
    // PHASE 4: Write positions back to SceneNodes (dynamic objects only)
    for (PrimitiveTypes::UInt32 i = 0; i < m_physicsComponents.m_size; i++)
    {
        PhysicsComponent *pPhysics = m_physicsComponents[i].getObject<PhysicsComponent>();
        
        if (pPhysics && pPhysics->m_linkedSceneNode && !pPhysics->isStatic)
        {
            // Physics position is at the collision shape center (in world space)
            // We need to calculate what the SceneNode position should be
            
            // Get current world transform to extract rotation
            Matrix4x4& worldTransform = pPhysics->m_linkedSceneNode->m_worldTransform;
            
            // Transform the local center offset by rotation only
            // offset_world = rotation * offset_local
            Vector3 worldCenterOffset = Vector3(
                worldTransform.m[0][0] * pPhysics->localCenterOffset.m_x + 
                worldTransform.m[0][1] * pPhysics->localCenterOffset.m_y + 
                worldTransform.m[0][2] * pPhysics->localCenterOffset.m_z,
                
                worldTransform.m[1][0] * pPhysics->localCenterOffset.m_x + 
                worldTransform.m[1][1] * pPhysics->localCenterOffset.m_y + 
                worldTransform.m[1][2] * pPhysics->localCenterOffset.m_z,
                
                worldTransform.m[2][0] * pPhysics->localCenterOffset.m_x + 
                worldTransform.m[2][1] * pPhysics->localCenterOffset.m_y + 
                worldTransform.m[2][2] * pPhysics->localCenterOffset.m_z
            );
            
            // SceneNode world position = Physics position - rotated offset
            Vector3 sceneNodeWorldPos = Vector3(
                pPhysics->position.m_x - worldCenterOffset.m_x,
                pPhysics->position.m_y - worldCenterOffset.m_y,
                pPhysics->position.m_z - worldCenterOffset.m_z
            );
            
            // For root nodes (like pMainSN), local position = world position
            // TODO: For child nodes, would need to inverse-transform by parent
            pPhysics->m_linkedSceneNode->m_base.setPos(sceneNodeWorldPos);
        }
    }
    
    s_firstFrame = false;
}

// Helper function to draw a single line
static void drawLine(DebugRenderer* pDebugRenderer, const Vector3& p1, const Vector3& p2, const Vector3& color)
{
    float lineData[12]; // 2 points * (3 position + 3 color) = 12 floats
    lineData[0] = p1.m_x; lineData[1] = p1.m_y; lineData[2] = p1.m_z;
    lineData[3] = color.m_x; lineData[4] = color.m_y; lineData[5] = color.m_z;
    lineData[6] = p2.m_x; lineData[7] = p2.m_y; lineData[8] = p2.m_z;
    lineData[9] = color.m_x; lineData[10] = color.m_y; lineData[11] = color.m_z;
    
    Matrix4x4 identity;
    identity.loadIdentity();
    pDebugRenderer->createLineMesh(false, identity, lineData, 12, 0.0f);
}

// Event handler implementation
void PhysicsManager::do_PHYSICS_DEBUG_RENDER(PE::Events::Event *pEvt)
{
#ifndef _DEBUG
    // Debug rendering disabled in release builds
    return;
#else
    DebugRenderer* pDebugRenderer = DebugRenderer::Instance();
    if (!pDebugRenderer)
    {
        PEINFO("PhysicsManager: DebugRenderer is NULL!\n");
        return;
    }
    
    PEINFO("PhysicsManager: Debug rendering %d physics components\n", m_physicsComponents.m_size);
    
    // Iterate through all physics components and draw their shapes
    for (PrimitiveTypes::UInt32 i = 0; i < m_physicsComponents.m_size; i++)
    {
        PhysicsComponent *pPhysics = m_physicsComponents[i].getObject<PhysicsComponent>();
        if (!pPhysics)
            continue;
        
        if (pPhysics->shapeType == PhysicsComponent::SPHERE)
        {
            // Draw sphere wireframe (3 circles: XY, XZ, YZ planes)
            const int segments = 16;  // Number of line segments per circle
            const float radius = pPhysics->sphereRadius;
            const Vector3& center = pPhysics->position;
            
            PEINFO("  Drawing SPHERE at (%.2f, %.2f, %.2f) radius=%.2f\n", 
                center.m_x, center.m_y, center.m_z, radius);
            
            // Color: Cyan for dynamic, Yellow for static
            Vector3 color = pPhysics->isStatic ? Vector3(1.0f, 1.0f, 0.0f) : Vector3(0.0f, 1.0f, 1.0f);
            
            // Draw 3 orthogonal circles to represent the sphere
            // Circle 1: XY plane (around Z axis)
            for (int j = 0; j < segments; j++)
            {
                float angle1 = (float)j / (float)segments * 2.0f * 3.14159f;
                float angle2 = (float)(j + 1) / (float)segments * 2.0f * 3.14159f;
                
                Vector3 p1(
                    center.m_x + radius * cosf(angle1),
                    center.m_y + radius * sinf(angle1),
                    center.m_z
                );
                Vector3 p2(
                    center.m_x + radius * cosf(angle2),
                    center.m_y + radius * sinf(angle2),
                    center.m_z
                );
                
                drawLine(pDebugRenderer, p1, p2, color);
            }
            
            // Circle 2: XZ plane (around Y axis)
            for (int j = 0; j < segments; j++)
            {
                float angle1 = (float)j / (float)segments * 2.0f * 3.14159f;
                float angle2 = (float)(j + 1) / (float)segments * 2.0f * 3.14159f;
                
                Vector3 p1(
                    center.m_x + radius * cosf(angle1),
                    center.m_y,
                    center.m_z + radius * sinf(angle1)
                );
                Vector3 p2(
                    center.m_x + radius * cosf(angle2),
                    center.m_y,
                    center.m_z + radius * sinf(angle2)
                );
                
                drawLine(pDebugRenderer, p1, p2, color);
            }
            
            // Circle 3: YZ plane (around X axis)
            for (int j = 0; j < segments; j++)
            {
                float angle1 = (float)j / (float)segments * 2.0f * 3.14159f;
                float angle2 = (float)(j + 1) / (float)segments * 2.0f * 3.14159f;
                
                Vector3 p1(
                    center.m_x,
                    center.m_y + radius * cosf(angle1),
                    center.m_z + radius * sinf(angle1)
                );
                Vector3 p2(
                    center.m_x,
                    center.m_y + radius * cosf(angle2),
                    center.m_z + radius * sinf(angle2)
                );
                
                drawLine(pDebugRenderer, p1, p2, color);
            }
        }
        else if (pPhysics->shapeType == PhysicsComponent::AABB)
        {
            // Draw AABB wireframe box (using world-space AABB that accounts for rotation)
            const Vector3& min = pPhysics->worldAABBMin;
            const Vector3& max = pPhysics->worldAABBMax;
            
            PEINFO("  Drawing AABB min=(%.2f, %.2f, %.2f) max=(%.2f, %.2f, %.2f)\n", 
                min.m_x, min.m_y, min.m_z, max.m_x, max.m_y, max.m_z);
            
            // Color: Magenta for static AABBs
            Vector3 color(1.0f, 0.0f, 1.0f);
            
            // Calculate 8 corners from world-space min/max
            Vector3 corners[8];
            corners[0] = Vector3(min.m_x, min.m_y, min.m_z);
            corners[1] = Vector3(max.m_x, min.m_y, min.m_z);
            corners[2] = Vector3(max.m_x, max.m_y, min.m_z);
            corners[3] = Vector3(min.m_x, max.m_y, min.m_z);
            corners[4] = Vector3(min.m_x, min.m_y, max.m_z);
            corners[5] = Vector3(max.m_x, min.m_y, max.m_z);
            corners[6] = Vector3(max.m_x, max.m_y, max.m_z);
            corners[7] = Vector3(min.m_x, max.m_y, max.m_z);
            
            // Draw 12 edges of the box
            // Bottom face
            drawLine(pDebugRenderer, corners[0], corners[1], color);
            drawLine(pDebugRenderer, corners[1], corners[2], color);
            drawLine(pDebugRenderer, corners[2], corners[3], color);
            drawLine(pDebugRenderer, corners[3], corners[0], color);
            
            // Top face
            drawLine(pDebugRenderer, corners[4], corners[5], color);
            drawLine(pDebugRenderer, corners[5], corners[6], color);
            drawLine(pDebugRenderer, corners[6], corners[7], color);
            drawLine(pDebugRenderer, corners[7], corners[4], color);
            
            // Vertical edges
            drawLine(pDebugRenderer, corners[0], corners[4], color);
            drawLine(pDebugRenderer, corners[1], corners[5], color);
            drawLine(pDebugRenderer, corners[2], corners[6], color);
            drawLine(pDebugRenderer, corners[3], corners[7], color);
        }
    }
#endif
}

}
}