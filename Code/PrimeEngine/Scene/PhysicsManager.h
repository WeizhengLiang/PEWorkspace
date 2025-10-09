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

struct PhysicsManager : public Component
{
	PE_DECLARE_CLASS(PhysicsManager);
	PhysicsManager(PE::GameContext &context, PE::MemoryArena arena, Handle hMyself);
	virtual ~PhysicsManager() {}
	
	virtual void addDefaultComponents();

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