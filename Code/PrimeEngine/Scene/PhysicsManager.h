#ifndef __PYENGINE_2_0_PHYSICS_MANAGER__
#define __PYENGINE_2_0_PHYSICS_MANAGER__

#define NOMINMAX
// API Abstraction
#include "PrimeEngine/APIAbstraction/APIAbstractionDefines.h"

// Inter-Engine includes
#include "PrimeEngine/Utils/Array/Array.h"
#include "PrimeEngine/Events/Component.h"
#include "PhysicsComponent.h"

namespace PE {
namespace Components {

// Collision information structure
struct CollisionInfo
{
	PhysicsComponent* object1;  // Dynamic object (sphere)
	PhysicsComponent* object2;  // Static object (AABB)
	Vector3 normal;              // Collision normal (points from object2 to object1)
	float penetrationDepth;      // How far objects overlap
	bool hasCollision;           // Whether a collision occurred
	
	CollisionInfo() : object1(nullptr), object2(nullptr), penetrationDepth(0.0f), hasCollision(false) {}
};

struct PhysicsManager : public Component
{
	PE_DECLARE_CLASS(PhysicsManager);
	PhysicsManager(PE::GameContext &context, PE::MemoryArena arena, Handle hMyself);
	virtual ~PhysicsManager() {}
	
	virtual void addDefaultComponents();
	
	// Event handling
	PE_DECLARE_IMPLEMENT_EVENT_HANDLER_WRAPPER(do_PHYSICS_DEBUG_RENDER)
	virtual void do_PHYSICS_DEBUG_RENDER(PE::Events::Event *pEvt);

    // Singleton pattern
    static void Construct(PE::GameContext &context, PE::MemoryArena arena);
    static void SetInstance(Handle h);
    static PhysicsManager *Instance();

    // Singleton handle
    static Handle s_hInstance;

    // Physics operations
    void addComponent(Handle hPhysicsComponent);
    void update(float deltaTime);

    // List of all physics components
    Array<Handle> m_physicsComponents;
};

}; // namespace Components
}; // namespace PE

#endif