#define NOMINMAX
#include "PhysicsComponent.h"
#include "SceneNode.h"
#include "PrimeEngine/APIAbstraction/APIAbstractionDefines.h"

#include "PrimeEngine/Lua/LuaEnvironment.h"

namespace PE {
namespace Components {

PE_IMPLEMENT_CLASS1(PhysicsComponent, Component);

// Constructor
PhysicsComponent::PhysicsComponent(PE::GameContext &context, PE::MemoryArena arena, Handle hMyself)
	: Component(context, arena, hMyself)
	, position(0.0f, 0.0f, 0.0f)
	, velocity(0.0f, 0.0f, 0.0f)
	, acceleration(0.0f, 0.0f, 0.0f)
	, shapeType(SPHERE)
	, sphereRadius(1.0f)
	, aabbExtents(1.0f, 1.0f, 1.0f)
	, worldAABBMin(0.0f, 0.0f, 0.0f)
	, worldAABBMax(0.0f, 0.0f, 0.0f)
	, isStatic(false)
	, mass(1.0f)
	, isNavmeshObstacle(false)
	, m_linkedSceneNode(nullptr)
	, localCenterOffset(0.0f, 0.0f, 0.0f)
{
}

void PhysicsComponent::addDefaultComponents()
{
	Component::addDefaultComponents();
}

}; // namespace Components
}; // namespace PE

