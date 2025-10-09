#ifndef __PYENGINE_2_0_PHYSICS_COMPONENT__
#define __PYENGINE_2_0_PHYSICS_COMPONENT__

#define NOMINMAX
// API Abstraction
#include "PrimeEngine/APIAbstraction/APIAbstractionDefines.h"

// Inter-Engine includes
#include "../Events/Component.h"
#include "../Math/Vector3.h"

// Forward declarations
namespace PE {
namespace Components {
    struct SceneNode;
}
}

namespace PE {
namespace Components {

struct PhysicsComponent : public Component
{
	PE_DECLARE_CLASS(PhysicsComponent);

	enum ShapeType {
		SPHERE,
		AABB
	};

	// Constructor
	PhysicsComponent(PE::GameContext &context, PE::MemoryArena arena, PE::Handle hMyself);
	virtual ~PhysicsComponent(){}
	
	virtual void addDefaultComponents();

	// Physics properties
	Vector3 position;
	Vector3 velocity;
	Vector3 acceleration;
	
	// Shape data
	ShapeType shapeType;
	float sphereRadius;      // Used if shapeType == SPHERE
	Vector3 aabbExtents;     // Used if shapeType == AABB (half-widths)
	
	// Physical properties
	bool isStatic;           // Static objects don't move (buildings, ground)
	float mass;              // Mass in kg
	
	// Link to scene graph
	SceneNode* m_linkedSceneNode;  // The SceneNode this physics component controls
};

}; // namespace Components
}; // namespace PE
#endif
