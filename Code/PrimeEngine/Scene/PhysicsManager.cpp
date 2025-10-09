#define NOMINMAX
#include "PhysicsManager.h"
#include "PrimeEngine/APIAbstraction/APIAbstractionDefines.h"

#include "PrimeEngine/Lua/LuaEnvironment.h"

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
	PEINFO("PhysicsManager constructor completed\n");
}

void PhysicsManager::addDefaultComponents()
{
	PEINFO("PhysicsManager::addDefaultComponents() started\n");
	Component::addDefaultComponents();
	PEINFO("PhysicsManager::addDefaultComponents() completed\n");
}

void PhysicsManager::Construct(PE::GameContext &context, PE::MemoryArena arena)
{
	PEINFO("PhysicsManager::Construct() called\n");
	PEINFO("About to create Handle\n");
	Handle h("PHYSICS_MANAGER", sizeof(PhysicsManager));
	PEINFO("About to create PhysicsManager object\n");
	PhysicsManager *pPhysicsManager = new(h) PhysicsManager(context, arena, h);
	PEINFO("About to call addDefaultComponents\n");
	pPhysicsManager->addDefaultComponents();
	PEINFO("About to SetInstance\n");
	SetInstance(h);
	PEINFO("PhysicsManager::Construct() completed\n");
}

void PhysicsManager::SetInstance(Handle h){s_hInstance = h;}
PhysicsManager *PhysicsManager::Instance(){return s_hInstance.getObject<PhysicsManager>();}

void PhysicsManager::addComponent(Handle hPhysicsComponent)
{
	m_physicsComponents.add(hPhysicsComponent);
}

void PhysicsManager::update(float deltaTime)
{
    // For now, just return (empty update for Milestone 1)
    // TODO: Milestone 2 - implement gravity
    // TODO: Milestone 3 - implement collision detection
    // TODO: Milestone 4 - implement collision response
}

}
}