#ifndef _PE_SOLDIER_NPC_BEHAVIOR_SM_H_
#define _PE_SOLDIER_NPC_BEHAVIOR_SM_H_


#include "PrimeEngine/Events/Component.h"
#include "PrimeEngine/Math/Vector3.h"
#include "PrimeEngine/Utils/Array/Array.h"
#include "PrimeEngine/PrimitiveTypes/PrimitiveTypes.h"

#include "../Events/Events.h"

namespace CharacterControl{

namespace Components {

// movement state machine talks to associated animation state machine
struct SoldierNPCBehaviorSM : public PE::Components::Component
{
	PE_DECLARE_CLASS(SoldierNPCBehaviorSM);
	
	enum States
	{
		IDLE, // stand in place
		WAITING_FOR_WAYPOINT, // have a name of waypoint to go to, but it has not been loaded yet
		PATROLLING_WAYPOINTS,
		//LOOKING_FOR_TARGET,
		SHOOTING_AT_TARGET,
		MOVING_TO_CORNER,
	};


	SoldierNPCBehaviorSM(PE::GameContext &context, PE::MemoryArena arena, PE::Handle hMyself, PE::Handle hMovementSM);

	void start();

	//////////////////////////////////////////////////////////////////////////
	// Component API and Event handlers
	//////////////////////////////////////////////////////////////////////////
	//
	virtual void addDefaultComponents();
	//
	PE_DECLARE_IMPLEMENT_EVENT_HANDLER_WRAPPER(do_SoldierNPCMovementSM_Event_TARGET_REACHED)
	virtual void do_SoldierNPCMovementSM_Event_TARGET_REACHED(PE::Events::Event *pEvt);
	//
	PE_DECLARE_IMPLEMENT_EVENT_HANDLER_WRAPPER(do_UPDATE)
	virtual void do_UPDATE(PE::Events::Event *pEvt);

	PE_DECLARE_IMPLEMENT_EVENT_HANDLER_WRAPPER(do_PRE_RENDER_needsRC)
	void do_PRE_RENDER_needsRC(PE::Events::Event *pEvt);

	PE::Handle m_hMovementSM;

	bool m_havePatrolWayPoint;
	char m_curPatrolWayPoint[32];
	bool m_shouldLookForTargetAndShoot;
	int m_lookForTargetAndShoot;
	States m_state;
	Vector3 m_fallbackCornerTarget;
	bool m_hasFallbackCorner;
	Array<Vector3> m_navPath;
	PrimitiveTypes::UInt32 m_currentNavWaypoint;
	bool m_hasNavPath;

	bool getSoldierWorldPosition(Vector3 &outPos);
	bool requestPathToTarget(const Vector3 &target);
	bool issueNextPathWaypoint();
};

};
};


#endif


