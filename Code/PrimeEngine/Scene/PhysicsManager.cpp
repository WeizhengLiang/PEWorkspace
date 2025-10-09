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

void PhysicsManager::update(float deltaTime)
{
    // Gravity constant (m/s^2) - negative Y is down
    const float GRAVITY = -10.0f;
    
    // Update all physics components
    for (PrimitiveTypes::UInt32 i = 0; i < m_physicsComponents.m_size; i++)
    {
        PhysicsComponent *pPhysics = m_physicsComponents[i].getObject<PhysicsComponent>();
        
        if (!pPhysics || pPhysics->isStatic)
            continue;  // Skip static objects
        
        // Apply gravity (F = ma, so a = F/m, but gravity is constant acceleration)
        pPhysics->acceleration.m_y = GRAVITY;
        
        // Update velocity: v = v0 + a*dt
        pPhysics->velocity.m_x += pPhysics->acceleration.m_x * deltaTime;
        pPhysics->velocity.m_y += pPhysics->acceleration.m_y * deltaTime;
        pPhysics->velocity.m_z += pPhysics->acceleration.m_z * deltaTime;
        
        // Update position: p = p0 + v*dt
        pPhysics->position.m_x += pPhysics->velocity.m_x * deltaTime;
        pPhysics->position.m_y += pPhysics->velocity.m_y * deltaTime;
        pPhysics->position.m_z += pPhysics->velocity.m_z * deltaTime;
        
        // Write position back to SceneNode
        if (pPhysics->m_linkedSceneNode)
        {
            pPhysics->m_linkedSceneNode->m_base.setPos(pPhysics->position);
        }
    }
    
    // TODO: Milestone 3 - implement collision detection
    // TODO: Milestone 4 - implement collision response
}

}
}