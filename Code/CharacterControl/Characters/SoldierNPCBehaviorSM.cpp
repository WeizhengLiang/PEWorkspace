#include "PrimeEngine/APIAbstraction/APIAbstractionDefines.h"

#include "PrimeEngine/Lua/LuaEnvironment.h"
#include "PrimeEngine/Scene/DebugRenderer.h"
#include "../ClientGameObjectManagerAddon.h"
#include "../CharacterControlContext.h"
#include "SoldierNPCMovementSM.h"
#include "SoldierNPCAnimationSM.h"
#include "SoldierNPCBehaviorSM.h"
#include "SoldierNPC.h"
#include "PrimeEngine/Scene/SceneNode.h"
#include "PrimeEngine/Render/IRenderer.h"
using namespace PE::Components;
using namespace PE::Events;
using namespace CharacterControl::Events;

namespace CharacterControl{

namespace Components{

PE_IMPLEMENT_CLASS1(SoldierNPCBehaviorSM, Component);

SoldierNPCBehaviorSM::SoldierNPCBehaviorSM(PE::GameContext &context, PE::MemoryArena arena, PE::Handle hMyself, PE::Handle hMovementSM) 
: Component(context, arena, hMyself)
, m_hMovementSM(hMovementSM)
, m_hasFallbackCorner(false)
, m_navPath(context, arena)
, m_currentNavWaypoint(0)
, m_hasNavPath(false)
{

}

void SoldierNPCBehaviorSM::start()
{
	if (m_havePatrolWayPoint)
	{
		m_state = WAITING_FOR_WAYPOINT; // will update on next do_UPDATE()
	}
	else
	{
		ClientGameObjectManagerAddon *pAddon = (ClientGameObjectManagerAddon *)(m_pContext->get<CharacterControlContext>()->getGameObjectManagerAddon());
		Vector3 fallbackPos;
		if (pAddon && pAddon->getRandomNavmeshCorner(fallbackPos) && requestPathToTarget(fallbackPos))
		{
			m_state = MOVING_TO_CORNER;
			m_fallbackCornerTarget = fallbackPos;
			m_hasFallbackCorner = true;
		}
		else
		{
			m_state = IDLE; // stand in place

			PE::Handle h("SoldierNPCMovementSM_Event_STOP", sizeof(SoldierNPCMovementSM_Event_STOP));
			SoldierNPCMovementSM_Event_STOP *pEvt = new(h) SoldierNPCMovementSM_Event_STOP();

			m_hMovementSM.getObject<Component>()->handleEvent(pEvt);
			// release memory now that event is processed
			h.release();
		}
	}	
}

void SoldierNPCBehaviorSM::addDefaultComponents()
{
	Component::addDefaultComponents();

	PE_REGISTER_EVENT_HANDLER(SoldierNPCMovementSM_Event_TARGET_REACHED, SoldierNPCBehaviorSM::do_SoldierNPCMovementSM_Event_TARGET_REACHED);
	PE_REGISTER_EVENT_HANDLER(Event_UPDATE, SoldierNPCBehaviorSM::do_UPDATE);

	PE_REGISTER_EVENT_HANDLER(Event_PRE_RENDER_needsRC, SoldierNPCBehaviorSM::do_PRE_RENDER_needsRC);
}

void SoldierNPCBehaviorSM::do_SoldierNPCMovementSM_Event_TARGET_REACHED(PE::Events::Event *pEvt)
{
	PEINFO("SoldierNPCBehaviorSM::do_SoldierNPCMovementSM_Event_TARGET_REACHED\n");

	if (m_state == PATROLLING_WAYPOINTS)
	{
		ClientGameObjectManagerAddon *pGameObjectManagerAddon = (ClientGameObjectManagerAddon *)(m_pContext->get<CharacterControlContext>()->getGameObjectManagerAddon());
		if (pGameObjectManagerAddon)
		{
			// search for waypoint object
			WayPoint *pWP = pGameObjectManagerAddon->getWayPoint(m_curPatrolWayPoint);
			if (pWP && StringOps::length(pWP->m_nextWayPointName) > 0)
			{
				
				// assignment 2 task 1: randomly choose next waypoint 
				// Initialize random seed only once
				static bool srandInitialized = false;
				if (!srandInitialized) {
					srand((unsigned)time(nullptr));
					srandInitialized = true;
				}

				auto pickRandomFromList = [&](const char* inputList, char* output) {
					// Copy input string because strtok modifies it
					char buffer[32];
					strncpy(buffer, inputList, sizeof(buffer));
					buffer[sizeof(buffer) - 1] = '\0';

					// Split by commas
					char* tokens[16]; // up to 16 options
					int count = 0;
					char* token = strtok(buffer, ",");
					while (token && count < 16) {
						// Trim whitespace from token
						while (*token == ' ' || *token == '\t') token++;
						char* end = token + strlen(token) - 1;
						while (end > token && (*end == ' ' || *end == '\t')) {
							*end = '\0';
							end--;
						}
						tokens[count++] = token;
						token = strtok(nullptr, ",");
					}

					if (count > 0) {
						// Randomly select one
						int idx = rand() % count;

						// Copy selected token to output
						strncpy(output, tokens[idx], 31);
						output[31] = '\0';
					}
					else {
						// Nothing to choose from �� clear string
						output[0] = '\0';
					}
				};

				char selectedWaypoint[32];
				pickRandomFromList(pWP->m_nextWayPointName, selectedWaypoint);

				char debugMsg[64];
				sprintf(debugMsg, "Next waypoint: %s\n", selectedWaypoint);
				OutputDebugStringA(debugMsg);
				
				// have next waypoint to go to
				pWP = pGameObjectManagerAddon->getWayPoint(selectedWaypoint);

				if (pWP)
				{
					StringOps::writeToString(pWP->m_name, m_curPatrolWayPoint, 32);

					m_state = PATROLLING_WAYPOINTS;
					PE::Handle h("SoldierNPCMovementSM_Event_MOVE_TO", sizeof(SoldierNPCMovementSM_Event_MOVE_TO));
					Events::SoldierNPCMovementSM_Event_MOVE_TO *pEvt = new(h) SoldierNPCMovementSM_Event_MOVE_TO(pWP->m_base.getPos());

					pEvt->m_running = pWP->m_needToRunToThisWayPoint > 0;

					m_hMovementSM.getObject<Component>()->handleEvent(pEvt);
					// release memory now that event is processed
					h.release();
				}
			}
			else
			{
				m_state = IDLE;
				// no need to send the event. movement state machine will automatically send event to animation state machine to play idle animation
				if (m_shouldLookForTargetAndShoot) {

					SoldierNPC *targetPtr = pGameObjectManagerAddon->getFirstTargetableSoldierObject();

					PE::Handle h("SoldierNPCMovementSM_Event_STOP", sizeof(SoldierNPCMovementSM_Event_STOP));
					SoldierNPCMovementSM_Event_STOP* pEvt = new(h) SoldierNPCMovementSM_Event_STOP();

					pEvt->m_targetPtr = targetPtr;
					pEvt->m_standShoot = m_shouldLookForTargetAndShoot;

					m_hMovementSM.getObject<Component>()->handleEvent(pEvt);
					// release memory now that event is processed
					h.release();
					
				}
			}
		}
	}
	else if (m_state == MOVING_TO_CORNER)
	{
		if (m_hasNavPath && m_currentNavWaypoint + 1 < m_navPath.m_size)
		{
			++m_currentNavWaypoint;
			issueNextPathWaypoint();
			return;
		}

		m_hasNavPath = false;

		ClientGameObjectManagerAddon *pAddon = (ClientGameObjectManagerAddon *)(m_pContext->get<CharacterControlContext>()->getGameObjectManagerAddon());
		Vector3 nextCorner;
		bool foundCorner = false;

		if (pAddon)
		{
			const int maxAttempts = 8;
			for (int attempt = 0; attempt < maxAttempts; ++attempt)
			{
				if (!pAddon->getRandomNavmeshCorner(nextCorner))
					break;

				if (!m_hasFallbackCorner)
				{
					foundCorner = true;
					break;
				}

				Vector3 diff = nextCorner - m_fallbackCornerTarget;
				if (diff.length() > 0.1f)
				{
					foundCorner = true;
					break;
				}
			}
		}

		if (foundCorner && requestPathToTarget(nextCorner))
		{
			m_state = MOVING_TO_CORNER;
			m_fallbackCornerTarget = nextCorner;
			m_hasFallbackCorner = true;
		}
		else
		{
			m_state = IDLE;
			m_hasFallbackCorner = false;
		}
	}
}

bool SoldierNPCBehaviorSM::getSoldierWorldPosition(Vector3 &outPos)
{
	SoldierNPC *pSol = getFirstParentByTypePtr<SoldierNPC>();
	if (!pSol)
		return false;

	PE::Handle hSoldierSceneNode = pSol->getFirstComponentHandle<PE::Components::SceneNode>();
	if (!hSoldierSceneNode.isValid())
		return false;

	SceneNode *pSN = hSoldierSceneNode.getObject<SceneNode>();
	if (!pSN)
		return false;

	outPos = pSN->m_base.getPos();
	return true;
}

bool SoldierNPCBehaviorSM::requestPathToTarget(const Vector3 &target)
{
	ClientGameObjectManagerAddon *pAddon = (ClientGameObjectManagerAddon *)(m_pContext->get<CharacterControlContext>()->getGameObjectManagerAddon());
	if (!pAddon)
		return false;

	Vector3 startPos;
	if (!getSoldierWorldPosition(startPos))
		return false;

	if (!pAddon->computeNavmeshPath(startPos, target, m_navPath, true))
	{
		m_hasNavPath = false;
		return false;
	}

	if (m_navPath.m_size == 0)
	{
		m_hasNavPath = false;
		return false;
	}

	m_currentNavWaypoint = 0;
	m_hasNavPath = true;
	return issueNextPathWaypoint();
}

bool SoldierNPCBehaviorSM::issueNextPathWaypoint()
{
	if (!m_hasNavPath || m_navPath.m_size == 0 || m_currentNavWaypoint >= m_navPath.m_size)
		return false;

	Vector3 waypoint = m_navPath[m_currentNavWaypoint];

	PE::Handle h("SoldierNPCMovementSM_Event_MOVE_TO", sizeof(SoldierNPCMovementSM_Event_MOVE_TO));
	SoldierNPCMovementSM_Event_MOVE_TO *pEvt = new(h) SoldierNPCMovementSM_Event_MOVE_TO(waypoint);
	pEvt->m_running = false;
	m_hMovementSM.getObject<Component>()->handleEvent(pEvt);
	h.release();

	return true;
}

// this event is executed when thread has RC
void SoldierNPCBehaviorSM::do_PRE_RENDER_needsRC(PE::Events::Event *pEvt)
{
	Event_PRE_RENDER_needsRC *pRealEvent = (Event_PRE_RENDER_needsRC *)(pEvt);
	if (m_havePatrolWayPoint)
	{
		char buf[80];
		sprintf(buf, "Patrol Waypoint: %s",m_curPatrolWayPoint);
		SoldierNPC *pSol = getFirstParentByTypePtr<SoldierNPC>();
		PE::Handle hSoldierSceneNode = pSol->getFirstComponentHandle<PE::Components::SceneNode>();
		Matrix4x4 base = hSoldierSceneNode.getObject<PE::Components::SceneNode>()->m_worldTransform;
		
		DebugRenderer::Instance()->createTextMesh(
			buf, false, false, true, false, 0,
			base.getPos(), 0.01f, pRealEvent->m_threadOwnershipMask);
		
		{
			//we can also construct points ourself
			bool sent = false;
			ClientGameObjectManagerAddon *pGameObjectManagerAddon = (ClientGameObjectManagerAddon *)(m_pContext->get<CharacterControlContext>()->getGameObjectManagerAddon());
			if (pGameObjectManagerAddon)
			{
				WayPoint *pWP = pGameObjectManagerAddon->getWayPoint(m_curPatrolWayPoint);
				if (pWP)
				{
					Vector3 target = pWP->m_base.getPos();
					Vector3 pos = base.getPos();
					Vector3 color(1.0f, 1.0f, 0);
					Vector3 linepts[] = {pos, color, target, color};
					
					DebugRenderer::Instance()->createLineMesh(true, base,  &linepts[0].m_x, 2, 0);// send event while the array is on the stack
					sent = true;
				}
			}
			if (!sent) // if for whatever reason we didnt retrieve waypoint info, send the event with transform only
				DebugRenderer::Instance()->createLineMesh(true, base, NULL, 0, 0);// send event while the array is on the stack
		}
	}
}

void SoldierNPCBehaviorSM::do_UPDATE(PE::Events::Event *pEvt)
{
	if (m_state == WAITING_FOR_WAYPOINT)
	{
		if (m_havePatrolWayPoint)
		{
			ClientGameObjectManagerAddon *pGameObjectManagerAddon = (ClientGameObjectManagerAddon *)(m_pContext->get<CharacterControlContext>()->getGameObjectManagerAddon());
			if (pGameObjectManagerAddon)
			{
				// search for waypoint object
				WayPoint *pWP = pGameObjectManagerAddon->getWayPoint(m_curPatrolWayPoint);
				if (pWP)
				{
					m_state = PATROLLING_WAYPOINTS;
					PE::Handle h("SoldierNPCMovementSM_Event_MOVE_TO", sizeof(SoldierNPCMovementSM_Event_MOVE_TO));
					Events::SoldierNPCMovementSM_Event_MOVE_TO *pEvt = new(h) SoldierNPCMovementSM_Event_MOVE_TO(pWP->m_base.getPos());

					m_hMovementSM.getObject<Component>()->handleEvent(pEvt);
					// release memory now that event is processed
					h.release();
				}
			}
		}
		else
		{
			// should not happen, but in any case, set state to idle
			m_state = IDLE;
			if (m_shouldLookForTargetAndShoot) {
				ClientGameObjectManagerAddon* pGameObjectManagerAddon = (ClientGameObjectManagerAddon*)(m_pContext->get<CharacterControlContext>()->getGameObjectManagerAddon());
				if (pGameObjectManagerAddon) {

					SoldierNPC *targetPtr= pGameObjectManagerAddon->getFirstTargetableSoldierObject();

					PE::Handle h("SoldierNPCMovementSM_Event_STOP", sizeof(SoldierNPCMovementSM_Event_STOP));
					SoldierNPCMovementSM_Event_STOP* pEvt = new(h) SoldierNPCMovementSM_Event_STOP();

					pEvt->m_targetPtr = targetPtr;
					pEvt->m_standShoot = m_shouldLookForTargetAndShoot;

					m_hMovementSM.getObject<Component>()->handleEvent(pEvt);
					// release memory now that event is processed
					h.release();
				}
			}
			else 
			{
				PE::Handle h("SoldierNPCMovementSM_Event_STOP", sizeof(SoldierNPCMovementSM_Event_STOP));
				SoldierNPCMovementSM_Event_STOP* pEvt = new(h) SoldierNPCMovementSM_Event_STOP();

				m_hMovementSM.getObject<Component>()->handleEvent(pEvt);
				// release memory now that event is processed
				h.release();
			}

		}
	}
}


}}




