#include "PrimeEngine/APIAbstraction/APIAbstractionDefines.h"

#include "PrimeEngine/Lua/LuaEnvironment.h"

#include "SoldierNPCMovementSM.h"
#include "SoldierNPCAnimationSM.h"
#include "SoldierNPC.h"
using namespace PE::Components;
using namespace PE::Events;
using namespace CharacterControl::Events;

namespace CharacterControl{

// Events sent by behavior state machine (or other high level state machines)
// these are events that specify where a soldier should move
namespace Events{

PE_IMPLEMENT_CLASS1(SoldierNPCMovementSM_Event_MOVE_TO, Event);

SoldierNPCMovementSM_Event_MOVE_TO::SoldierNPCMovementSM_Event_MOVE_TO(Vector3 targetPos /* = Vector3 */)
: m_targetPosition(targetPos)
, m_running(false)
{ }

PE_IMPLEMENT_CLASS1(SoldierNPCMovementSM_Event_STOP, Event);

PE_IMPLEMENT_CLASS1(SoldierNPCMovementSM_Event_TARGET_REACHED, Event);
}

namespace Components{

PE_IMPLEMENT_CLASS1(SoldierNPCMovementSM, Component);


SoldierNPCMovementSM::SoldierNPCMovementSM(PE::GameContext &context, PE::MemoryArena arena, PE::Handle hMyself) 
: Component(context, arena, hMyself)
, m_state(STANDING)
{}

SceneNode *SoldierNPCMovementSM::getParentsSceneNode()
{
	PE::Handle hParent = getFirstParentByType<Component>();
	if (hParent.isValid())
	{
		// see if parent has scene node component
		return hParent.getObject<Component>()->getFirstComponent<SceneNode>();
		
	}
	return NULL;
}

void SoldierNPCMovementSM::addDefaultComponents()
{
	Component::addDefaultComponents();

	PE_REGISTER_EVENT_HANDLER(SoldierNPCMovementSM_Event_MOVE_TO, SoldierNPCMovementSM::do_SoldierNPCMovementSM_Event_MOVE_TO);
	PE_REGISTER_EVENT_HANDLER(SoldierNPCMovementSM_Event_STOP, SoldierNPCMovementSM::do_SoldierNPCMovementSM_Event_STOP);
	
	PE_REGISTER_EVENT_HANDLER(Event_UPDATE, SoldierNPCMovementSM::do_UPDATE);
}

void SoldierNPCMovementSM::do_SoldierNPCMovementSM_Event_MOVE_TO(PE::Events::Event *pEvt)
{
	SoldierNPCMovementSM_Event_MOVE_TO *pRealEvt = (SoldierNPCMovementSM_Event_MOVE_TO *)(pEvt);
	
	// change state of this state machine
	m_targetPostion = pRealEvt->m_targetPosition;

	OutputDebugStringA("PE: PROGRESS: SoldierNPCMovementSM::do_SoldierNPCMovementSM_Event_MOVE_TO(): ");
	OutputDebugStringA(pRealEvt->m_running ? "running\n" : "walking\n");

	if (pRealEvt->m_running) {
		m_state = RUNNING_TO_TARGET;

		// make sure the animations are playing

		PE::Handle h("SoldierNPCAnimSM_Event_RUN", sizeof(SoldierNPCAnimSM_Event_RUN));
		Events::SoldierNPCAnimSM_Event_RUN* pOutEvt = new(h) SoldierNPCAnimSM_Event_RUN();

		SoldierNPC* pSol = getFirstParentByTypePtr<SoldierNPC>();
		pSol->getFirstComponent<PE::Components::SceneNode>()->handleEvent(pOutEvt);

		// release memory now that event is processed
		h.release();
	}
	else {
		m_state = WALKING_TO_TARGET;

		// make sure the animations are playing

		PE::Handle h("SoldierNPCAnimSM_Event_WALK", sizeof(SoldierNPCAnimSM_Event_WALK));
		Events::SoldierNPCAnimSM_Event_WALK* pOutEvt = new(h) SoldierNPCAnimSM_Event_WALK();

		SoldierNPC* pSol = getFirstParentByTypePtr<SoldierNPC>();
		pSol->getFirstComponent<PE::Components::SceneNode>()->handleEvent(pOutEvt);

		// release memory now that event is processed
		h.release();
	}
}

void SoldierNPCMovementSM::do_SoldierNPCMovementSM_Event_STOP(PE::Events::Event *pEvt)
{
	SoldierNPCMovementSM_Event_STOP* pRealEvt = (SoldierNPCMovementSM_Event_STOP*)(pEvt);

	if (!pRealEvt->m_standShoot) {
		m_state = STANDING; // can add another state if needed
		Events::SoldierNPCAnimSM_Event_STOP Evt;

		SoldierNPC* pSol = getFirstParentByTypePtr<SoldierNPC>();
		pSol->getFirstComponent<PE::Components::SceneNode>()->handleEvent(&Evt);
	}
	else {
		m_state = STANDING;
		Events::SoldierNPCAnimSM_Event_STAND_SHOOT Evt;

		m_shootTargetPtr = pRealEvt->m_targetPtr;

		SoldierNPC* pSol = getFirstParentByTypePtr<SoldierNPC>();
		pSol->getFirstComponent<PE::Components::SceneNode>()->handleEvent(&Evt);
	}
}

void SoldierNPCMovementSM::do_UPDATE(PE::Events::Event *pEvt)
{
	if (m_state == WALKING_TO_TARGET || m_state == RUNNING_TO_TARGET)
	{
		// see if parent has scene node component
		SceneNode* pSN = getParentsSceneNode();
		if (pSN)
		{
		Vector3 curPos = pSN->m_base.getPos();
		float dsqr = (m_targetPostion - curPos).lengthSqr();

		bool reached = true;
		// Increased threshold from 0.01 (0.1 units) to 0.25 (0.5 units) for smoother stop
		// This prevents "walking in place" when physics micro-adjusts position
		if (dsqr > 0.25f)  // Stop within 0.5 units of waypoint
		{
			// not at the spot yet
			Event_UPDATE* pRealEvt = (Event_UPDATE*)(pEvt);
			float speed = (m_state == WALKING_TO_TARGET) ? 1.4f : 3.0f;
			float allowedDisp = speed * pRealEvt->m_frameTime;

				Vector3 dir = (m_targetPostion - curPos);
				dir.normalize();
				float dist = sqrt(dsqr);
				if (dist > allowedDisp)
				{
					dist = allowedDisp; // can move up to allowedDisp
					reached = false; // not reaching destination yet
				}

				// instantaneous turn
				pSN->m_base.turnInDirection(dir, 3.1415f);
				pSN->m_base.setPos(curPos + dir * dist);
			}

			if (reached)
			{
				m_state = STANDING;

				// target has been reached. need to notify all same level state machines (components of parent)
				{
					PE::Handle h("SoldierNPCMovementSM_Event_TARGET_REACHED", sizeof(SoldierNPCMovementSM_Event_TARGET_REACHED));
					Events::SoldierNPCMovementSM_Event_TARGET_REACHED* pOutEvt = new(h) SoldierNPCMovementSM_Event_TARGET_REACHED();

					PE::Handle hParent = getFirstParentByType<Component>();
					if (hParent.isValid())
					{
						hParent.getObject<Component>()->handleEvent(pOutEvt);
					}

					// release memory now that event is processed
					h.release();
				}

				if (m_state == STANDING)
				{
					// no one has modified our state based on TARGET_REACHED callback
					// this means we are not going anywhere right now
					// so can send event to animation state machine to stop
					{
						Events::SoldierNPCAnimSM_Event_STOP evt;

						SoldierNPC* pSol = getFirstParentByTypePtr<SoldierNPC>();
						pSol->getFirstComponent<PE::Components::SceneNode>()->handleEvent(&evt);
					}
				}
			}
		}
	}else if(m_state == STANDING){

		if (SceneNode* pSN = getParentsSceneNode())
		{
			const Vector3 curPos = pSN->m_base.getPos();
			Vector3 m_shootTargetPostion = Vector3();
			
			// Check if we have a valid target pointer before accessing it
			if (m_shootTargetPtr)
			{
				SceneNode* pTargetSN = m_shootTargetPtr->getFirstComponent<SceneNode>();
				if (pTargetSN)
				{
					m_shootTargetPostion = pTargetSN->m_base.getPos();
					
					Vector3 toTarget = m_shootTargetPostion - curPos;

					// Only turn if the target isnt right on top of us
					constexpr float faceEpsilon = 0.001f; // ~1mm in engine units
					if (toTarget.lengthSqr() > faceEpsilon)
					{
						toTarget.normalize();

						// Rotate toward the target direction smoothly
						// Assume were inside Event_UPDATE with frameTime available
						auto* pRealEvt = static_cast<Event_UPDATE*>(pEvt);
						//PE_ASSERT(pRealEvt && "Event_UPDATE required here");

						constexpr float turnRate = 3.14f; // rad/sec = 180 per second, tuneable
						pSN->m_base.turnInDirection(toTarget, turnRate * pRealEvt->m_frameTime);
					}
					else
					{
						// Target coincides with position do nothing
					}
					
					// Continuously trigger shooting animation while we have a valid target
					Events::SoldierNPCAnimSM_Event_STAND_SHOOT shootEvt;
					SoldierNPC* pSol = getFirstParentByTypePtr<SoldierNPC>();
					pSol->getFirstComponent<PE::Components::SceneNode>()->handleEvent(&shootEvt);
				}
			}
		}
		

	}
}


}}
