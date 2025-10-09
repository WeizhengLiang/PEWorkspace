#define NOMINMAX
#include "PhysicsManager.h"
#include "PrimeEngine/APIAbstraction/APIAbstractionDefines.h"
#include "PrimeEngine/Lua/LuaEnvironment.h"
#include "SceneNode.h"

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
}

// Helper function: Test collision between sphere and AABB
static bool testSphereAABB(const PhysicsComponent* sphere, const PhysicsComponent* aabb, CollisionInfo& outInfo)
{
	// Get sphere center and radius
	Vector3 sphereCenter = sphere->position;
	float sphereRadius = sphere->sphereRadius;
	
	// Get AABB center and extents
	Vector3 aabbCenter = aabb->position;
	Vector3 aabbExtents = aabb->aabbExtents;
	
	// Calculate AABB min and max
	Vector3 aabbMin = Vector3(
		aabbCenter.m_x - aabbExtents.m_x,
		aabbCenter.m_y - aabbExtents.m_y,
		aabbCenter.m_z - aabbExtents.m_z
	);
	Vector3 aabbMax = Vector3(
		aabbCenter.m_x + aabbExtents.m_x,
		aabbCenter.m_y + aabbExtents.m_y,
		aabbCenter.m_z + aabbExtents.m_z
	);
	
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
    if (collisions.m_size > 0)
    {
        PEINFO("Physics: %d collisions detected this frame\n", collisions.m_size);
    }
    
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
    
    // PHASE 4: Write positions back to SceneNodes
    for (PrimitiveTypes::UInt32 i = 0; i < m_physicsComponents.m_size; i++)
    {
        PhysicsComponent *pPhysics = m_physicsComponents[i].getObject<PhysicsComponent>();
        
        if (pPhysics && pPhysics->m_linkedSceneNode)
        {
            pPhysics->m_linkedSceneNode->m_base.setPos(pPhysics->position);
        }
    }
}

}
}