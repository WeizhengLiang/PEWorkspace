#include "PrimeEngine/APIAbstraction/APIAbstractionDefines.h"

#include "PrimeEngine/Lua/LuaEnvironment.h"
#include "PrimeEngine/Scene/SkeletonInstance.h"
#include "PrimeEngine/Scene/MeshInstance.h"
#include "PrimeEngine/Scene/Mesh.h"
#include "PrimeEngine/Scene/RootSceneNode.h"
#include "PrimeEngine/Scene/PhysicsManager.h"

#include "SoldierNPC.h"
#include "SoldierNPCAnimationSM.h"
#include "SoldierNPCMovementSM.h"
#include "SoldierNPCBehaviorSM.h"


using namespace PE;
using namespace PE::Components;
using namespace CharacterControl::Events;

namespace CharacterControl{
namespace Components {

PE_IMPLEMENT_CLASS1(SoldierNPC, Component);

SoldierNPC::SoldierNPC(PE::GameContext &context, PE::MemoryArena arena, PE::Handle hMyself, Event_CreateSoldierNPC *pEvt) : Component(context, arena, hMyself)
{

	// hierarchy of soldier and replated components and variables (note variables are just variables, they are not passed events to)
	// scene
	// +-components
	//   +-soldier scene node
	//   | +-components
	//   |   +-soldier skin
	//   |     +-components
	//   |       +-soldier animation state machine
	//   |       +-soldier weapon skin scene node
	//   |         +-components
	//   |           +-weapon mesh

	// game objects
	// +-components
	//   +-soldier npc
	//     +-variables
	//     | +-m_hMySN = soldier scene node
	//     | +-m_hMySkin = skin
	//     | +-m_hMyGunSN = soldier weapon skin scene node
	//     | +-m_hMyGunMesh = weapon mesh
	//     +-components
	//       +-soldier scene node (restricted to no events. this is for state machines to be able to locate the scene node)
	//       +-movement state machine
	//       +-behavior state machine

    
	// need to acquire redner context for this code to execute thread-safe
	m_pContext->getGPUScreen()->AcquireRenderContextOwnership(pEvt->m_threadOwnershipMask);
	
	PE::Handle hSN("SCENE_NODE", sizeof(SceneNode));
	SceneNode *pMainSN = new(hSN) SceneNode(*m_pContext, m_arena, hSN);
	pMainSN->addDefaultComponents();

	// offset the soldier position a bit for assignment 1 requirements
	//float kSpawnYOffset = 200.0f;
	//Vector3 temp_pos = pEvt->m_pos;
	//temp_pos.m_y += kSpawnYOffset;

	//pMainSN->m_base.setPos(temp_pos);
	pMainSN->m_base.setPos(pEvt->m_pos);
	pMainSN->m_base.setU(pEvt->m_u);
	pMainSN->m_base.setV(pEvt->m_v);
	pMainSN->m_base.setN(pEvt->m_n);

	m_lookForTargetAndShoot = pEvt->m_lookForTargetAndShoot > 0;
	m_npcType = pEvt->m_npcType;

	RootSceneNode::Instance()->addComponent(hSN);

	// add the scene node as component of soldier without any handlers. this is just data driven way to locate scnenode for soldier's components
	{
		static int allowedEvts[] = {0};
		addComponent(hSN, &allowedEvts[0]);
	}

	// Physics component will be created after mesh instance is loaded

	int numskins = 1; // 8
	for (int iSkin = 0; iSkin < numskins; ++iSkin)
	{
		float z = (iSkin / 4) * 1.5f;
		float x = (iSkin % 4) * 1.5f;
		PE::Handle hSN("SCENE_NODE", sizeof(SceneNode));
		SceneNode *pSN = new(hSN) SceneNode(*m_pContext, m_arena, hSN);
		pSN->addDefaultComponents();

		pSN->m_base.setPos(Vector3(x, 0, z));
		
		// rotation scene node to rotate soldier properly, since soldier from Maya is facing wrong direction
		PE::Handle hRotateSN("SCENE_NODE", sizeof(SceneNode));
		SceneNode *pRotateSN = new(hRotateSN) SceneNode(*m_pContext, m_arena, hRotateSN);
		pRotateSN->addDefaultComponents();

		pSN->addComponent(hRotateSN);

		pRotateSN->m_base.turnLeft(3.1415);

		PE::Handle hSoldierAnimSM("SoldierNPCAnimationSM", sizeof(SoldierNPCAnimationSM));
		SoldierNPCAnimationSM *pSoldierAnimSM = new(hSoldierAnimSM) SoldierNPCAnimationSM(*m_pContext, m_arena, hSoldierAnimSM);
		pSoldierAnimSM->addDefaultComponents();

		pSoldierAnimSM->m_debugAnimIdOffset = 0;// rand() % 3;

		PE::Handle hSkeletonInstance("SkeletonInstance", sizeof(SkeletonInstance));
		SkeletonInstance *pSkelInst = new(hSkeletonInstance) SkeletonInstance(*m_pContext, m_arena, hSkeletonInstance, 
			hSoldierAnimSM);
		pSkelInst->addDefaultComponents();

		pSkelInst->initFromFiles("soldier_Soldier_Skeleton.skela", "Soldier", pEvt->m_threadOwnershipMask);

		pSkelInst->setAnimSet("soldier_Soldier_Skeleton.animseta", "Soldier");

		PE::Handle hMeshInstance("MeshInstance", sizeof(MeshInstance));
		MeshInstance *pMeshInstance = new(hMeshInstance) MeshInstance(*m_pContext, m_arena, hMeshInstance);
		pMeshInstance->addDefaultComponents();
		
		pMeshInstance->initFromFile(pEvt->m_meshFilename, pEvt->m_package, pEvt->m_threadOwnershipMask);
		
		pSkelInst->addComponent(hMeshInstance);

		// CREATE PHYSICS COMPONENT for soldier (after mesh is loaded)
		// NOTE: Must be called AFTER scene graph transformations are calculated!
		// For now, we'll do a simple setup and rely on the first update to fix positioning
		{
			PE::Handle hPhysics("PHYSICS_COMPONENT", sizeof(PhysicsComponent));
			PhysicsComponent *pPhysics = new(hPhysics) PhysicsComponent(*m_pContext, m_arena, hPhysics);
			pPhysics->addDefaultComponents();
			
			// Calculate bounding sphere from mesh AABB (if available)
			float calculatedRadius = 1.0f;  // Default fallback
			Vector3 localAABBCenter(0.0f, 0.0f, 0.0f);
			
			// Try to get AABB from the soldier's mesh
			Mesh *pMesh = pMeshInstance->m_hAsset.getObject<Mesh>();
			if (pMesh && pMesh->hasAABB())
			{
				const PE::AABB& aabb = pMesh->getLocalAABB();
				localAABBCenter = aabb.center;
				
				// Calculate bounding sphere radius: distance from center to furthest corner
				Vector3 extents = aabb.extents;
				calculatedRadius = sqrtf(extents.m_x * extents.m_x + 
				                        extents.m_y * extents.m_y + 
				                        extents.m_z * extents.m_z);
				
				PEINFO("Soldier sphere: AABB center=(%.2f, %.2f, %.2f), radius=%.2f\n",
					localAABBCenter.m_x, localAABBCenter.m_y, localAABBCenter.m_z, calculatedRadius);
			}
			
			// Initialize physics properties
			// Use simple position for now - PhysicsManager will sync from world transform
			pPhysics->position = pMainSN->m_base.getPos();
			pPhysics->localCenterOffset = localAABBCenter;  // Store in LOCAL space
			pPhysics->velocity = Vector3(0.0f, 0.0f, 0.0f);
			pPhysics->acceleration = Vector3(0.0f, 0.0f, 0.0f);
			
			// Soldier uses a sphere for collision
			pPhysics->shapeType = PhysicsComponent::SPHERE;
			pPhysics->sphereRadius = calculatedRadius;
			
			// Soldier is dynamic (affected by physics)
			pPhysics->isStatic = false;
			pPhysics->mass = 70.0f;  // 70 kg
			
			// Link to SceneNode
			pPhysics->m_linkedSceneNode = pMainSN;
			
			// Add to PhysicsManager
			PhysicsManager::Instance()->addComponent(hPhysics);
		}

		// add skin to scene node
		pRotateSN->addComponent(hSkeletonInstance);

		#if !APIABSTRACTION_D3D11
		{
			PE::Handle hMyGunMesh = PE::Handle("MeshInstance", sizeof(MeshInstance));
			MeshInstance *pGunMeshInstance = new(hMyGunMesh) MeshInstance(*m_pContext, m_arena, hMyGunMesh);

			pGunMeshInstance->addDefaultComponents();
			pGunMeshInstance->initFromFile(pEvt->m_gunMeshName, pEvt->m_gunMeshPackage, pEvt->m_threadOwnershipMask);

			// create a scene node for gun attached to a joint

			PE::Handle hMyGunSN = PE::Handle("SCENE_NODE", sizeof(JointSceneNode));
			JointSceneNode *pGunSN = new(hMyGunSN) JointSceneNode(*m_pContext, m_arena, hMyGunSN, 38);
			pGunSN->addDefaultComponents();

			// add gun to joint
			pGunSN->addComponent(hMyGunMesh);

			// add gun scene node to the skin
			pSkelInst->addComponent(hMyGunSN);
		}
		#endif
				
		pMainSN->addComponent(hSN);
	}

	m_pContext->getGPUScreen()->ReleaseRenderContextOwnership(pEvt->m_threadOwnershipMask);
	
#if 1
	// add movement state machine to soldier npc
    PE::Handle hSoldierMovementSM("SoldierNPCMovementSM", sizeof(SoldierNPCMovementSM));
	SoldierNPCMovementSM *pSoldierMovementSM = new(hSoldierMovementSM) SoldierNPCMovementSM(*m_pContext, m_arena, hSoldierMovementSM);
	pSoldierMovementSM->addDefaultComponents();

	// add it to soldier NPC
	addComponent(hSoldierMovementSM);

	// add behavior state machine ot soldier npc
    PE::Handle hSoldierBheaviorSM("SoldierNPCBehaviorSM", sizeof(SoldierNPCBehaviorSM));
	SoldierNPCBehaviorSM *pSoldierBehaviorSM = new(hSoldierBheaviorSM) SoldierNPCBehaviorSM(*m_pContext, m_arena, hSoldierBheaviorSM, hSoldierMovementSM);
	pSoldierBehaviorSM->addDefaultComponents();

	// add it to soldier NPC
	addComponent(hSoldierBheaviorSM);

	StringOps::writeToString(pEvt->m_patrolWayPoint, pSoldierBehaviorSM->m_curPatrolWayPoint, 32);
	pSoldierBehaviorSM->m_havePatrolWayPoint = StringOps::length(pSoldierBehaviorSM->m_curPatrolWayPoint) > 0;

	pSoldierBehaviorSM->m_lookForTargetAndShoot = pEvt->m_lookForTargetAndShoot;
	pSoldierBehaviorSM->m_shouldLookForTargetAndShoot = pSoldierBehaviorSM->m_lookForTargetAndShoot > 0;

	// start the soldier
	pSoldierBehaviorSM->start();
#endif
}

void SoldierNPC::addDefaultComponents()
{
	Component::addDefaultComponents();

	// custom methods of this component
}

}; // namespace Components
}; // namespace CharacterControl
