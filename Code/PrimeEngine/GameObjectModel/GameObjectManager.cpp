// APIAbstraction
#include "PrimeEngine/APIAbstraction/APIAbstractionDefines.h"

// Outer-Engine includes

// Inter-Engine includes
#include "../Lua/LuaEnvironment.h"

// Sibling/Children includes
#include "GameObjectManager.h"
#include "../Sound/SoundManager.h"

#include "PrimeEngine/Scene/Skeleton.h"
#include "PrimeEngine/Scene/MeshInstance.h"
#include "PrimeEngine/Scene/SkeletonInstance.h"
#include "PrimeEngine/Scene/PhysicsManager.h"
#include "PrimeEngine/Scene/Mesh.h"

namespace PE {
namespace Components {

using namespace PE::Events;

PE_IMPLEMENT_CLASS1(GameObjectManager, Component);

// Singleton ------------------------------------------------------------------

// Constructor -------------------------------------------------------------
GameObjectManager::GameObjectManager(PE::GameContext &context, PE::MemoryArena arena, Handle hMyself)
: Component(context, arena, hMyself), m_luaGameObjectTableRef(LUA_NOREF)
, Networkable(context, this, Networkable::s_NetworkId_GameObjectManager) // pre-assigned network id
{
}

	// Methods      ------------------------------------------------------------
void GameObjectManager::addDefaultComponents()
{
	Component::addDefaultComponents();

	addComponent(m_pContext->getLuaEnvironment()->getHandle());

	PE_REGISTER_EVENT_HANDLER(Event_SET_DEBUG_TARGET_HANDLE, GameObjectManager::do_SET_DEBUG_TARGET_HANDLE);
	PE_REGISTER_EVENT_HANDLER(Event_CONSTRUCT_SOUND, GameObjectManager::do_CONSTRUCT_SOUND);

	PE_REGISTER_EVENT_HANDLER(Event_CREATE_LIGHT, GameObjectManager::do_CREATE_LIGHT);
	PE_REGISTER_EVENT_HANDLER(Event_CREATE_MESH, GameObjectManager::do_CREATE_MESH);
	PE_REGISTER_EVENT_HANDLER(Event_CREATE_SKELETON, GameObjectManager::do_CREATE_SKELETON);
	PE_REGISTER_EVENT_HANDLER(Event_CREATE_ANIM_SET, GameObjectManager::do_CREATE_ANIM_SET);

	createGameObjectTableIfDoesntExist();
}

// Individual events -------------------------------------------------------
void GameObjectManager::do_SET_DEBUG_TARGET_HANDLE(Events::Event *pEvt)
{
	Event_SET_DEBUG_TARGET_HANDLE *pRealEvt = (Event_SET_DEBUG_TARGET_HANDLE *)(pEvt);

	Component::s_debuggedComponent = pRealEvt->m_hDebugTarget;
	Component::s_debuggedEvent = pRealEvt->m_debugEvent;
}

void GameObjectManager::do_CREATE_LIGHT(Events::Event *pEvt)
{
	Event_CREATE_LIGHT *pRealEvt = (Event_CREATE_LIGHT *)(pEvt);

	bool haveObject = false;
	Handle exisitngObject;

	putGameObjectTableIOnStack();

	if (!pRealEvt->m_peuuid.isZero())
	{
		// have a valid peeuid for the object. need to check if have one already

		haveObject = m_pContext->getLuaEnvironment()->checkTableValueByPEUUIDFieldExists(pRealEvt->m_peuuid);
		if (haveObject)
		{
			LuaEnvironment::popHandleFromTableOnStackAndPopTable(m_pContext->getLuaEnvironment()->L, exisitngObject);
			m_lastAddedObjHandle = exisitngObject;
		}
		else
		{
			// pop nil
			m_pContext->getLuaEnvironment()->pop();
		}
	}

	if (!haveObject)
	{
		Handle hLight("LIGHT", sizeof(Light));

		Light *pLight = new(hLight) Light(
			*m_pContext,
			m_arena,
			hLight,
			pRealEvt->m_pos, //Position
			pRealEvt->m_u, 
			pRealEvt->m_v, 
			pRealEvt->m_n, //Direction (z-axis)
			pRealEvt->m_ambient, //Ambient
			pRealEvt->m_diffuse, //Diffuse
			pRealEvt->m_spec, //Specular
			pRealEvt->m_att, //Attenuation (x, y, z)
			pRealEvt->m_spotPower, // Spot Power
			pRealEvt->m_range, //Range
			pRealEvt->m_isShadowCaster, //Whether or not it casts shadows
			(PrimitiveTypes::Int32)(pRealEvt->m_type) //0 = point, 1 = directional, 2 = spot
		);
		pLight->addDefaultComponents();

		RootSceneNode::Instance()->m_lights.add(hLight);
		RootSceneNode::Instance()->addComponent(hLight);

		m_pContext->getLuaEnvironment()->pushHandleAsFieldAndSet(pRealEvt->m_peuuid, hLight);
		m_lastAddedObjHandle = hLight;
	}
	else
	{
		// already have this object
		
		// need to reset the orientation
		// and light source settings
		Light *pLight = exisitngObject.getObject<Light>();
		
		pLight->m_base.setPos(pRealEvt->m_pos);
		pLight->m_base.setU(pRealEvt->m_u);
		pLight->m_base.setV(pRealEvt->m_v);
		pLight->m_base.setN(pRealEvt->m_n);
		

		pLight->m_cbuffer.pos = pLight->m_base.getPos();
		pLight->m_cbuffer.dir = pLight->m_base.getN();

		pLight->m_cbuffer.ambient = pRealEvt->m_ambient;
		pLight->m_cbuffer.diffuse = pRealEvt->m_diffuse;
		pLight->m_cbuffer.spec = pRealEvt->m_spec;
		pLight->m_cbuffer.att = pRealEvt->m_att;
		pLight->m_cbuffer.spotPower = pRealEvt->m_spotPower;
		pLight->m_cbuffer.range = pRealEvt->m_range;
		pLight->isTheShadowCaster = pRealEvt->m_isShadowCaster;
	}

	// pop the game object table
	m_pContext->getLuaEnvironment()->pop();
}

void GameObjectManager::do_CREATE_SKELETON(Events::Event *pEvt)
{
	Events::Event_CREATE_SKELETON *pRealEvent = (Events::Event_CREATE_SKELETON *)(pEvt);
	bool haveObject = false;
	Handle exisitngObject;

	putGameObjectTableIOnStack();

	if (!pRealEvent->m_peuuid.isZero())
	{
		// have a valid peeuid for the object. need to check if have one already

		haveObject = m_pContext->getLuaEnvironment()->checkTableValueByPEUUIDFieldExists(pRealEvent->m_peuuid);
		if (haveObject)
		{
			LuaEnvironment::popHandleFromTableOnStackAndPopTable(m_pContext->getLuaEnvironment()->L, exisitngObject);
			m_lastAddedObjHandle = exisitngObject;
		}
		else
		{
			// pop nil
			m_pContext->getLuaEnvironment()->pop();
		}

	}

	if (!haveObject)
	{
		// need to acquire redner context for this code to execute thread-safe
		m_pContext->getGPUScreen()->AcquireRenderContextOwnership(pRealEvent->m_threadOwnershipMask);

		PE::Handle hSkelInstance("SkeletonInstance", sizeof(SkeletonInstance));
		SkeletonInstance *pSkelInstance = new(hSkelInstance) SkeletonInstance(*m_pContext, m_arena, hSkelInstance, Handle());
		pSkelInstance->addDefaultComponents();

		pSkelInstance->initFromFiles(pRealEvent->m_skelFilename, pRealEvent->m_package, pRealEvent->m_threadOwnershipMask);

		m_pContext->getGPUScreen()->ReleaseRenderContextOwnership(pRealEvent->m_threadOwnershipMask);
		
		if (pRealEvent->hasCustomOrientation)
		{
			// need to create a scene node for this mesh
			Handle hSN("SCENE_NODE", sizeof(SceneNode));
			SceneNode *pSN = new(hSN) SceneNode(*m_pContext, m_arena, hSN);
			pSN->addDefaultComponents();

			pSN->m_base.setPos(pRealEvent->m_pos);
			pSN->m_base.setU(pRealEvent->m_u);
			pSN->m_base.setV(pRealEvent->m_v);
			pSN->m_base.setN(pRealEvent->m_n);

			pSN->addComponent(hSkelInstance);

			RootSceneNode::Instance()->addComponent(hSN);
		}
		else
		{
			RootSceneNode::Instance()->addComponent(hSkelInstance);
		}
		m_pContext->getLuaEnvironment()->pushHandleAsFieldAndSet(pRealEvent->m_peuuid, hSkelInstance);
		m_lastAddedObjHandle = hSkelInstance;
		m_lastAddedSkelInstanceHandle = hSkelInstance;
	}
	else
	{
		// already have this object
		// only care about orientation
		if (pRealEvent->hasCustomOrientation)
		{
			// need to reset the orientation
			// try finding scene node
			SkeletonInstance *pSkelInstance = exisitngObject.getObject<SkeletonInstance>();
			Handle hSN = pSkelInstance->getFirstParentByType<SceneNode>();
			if (hSN.isValid())
			{
				SceneNode *pSN = hSN.getObject<SceneNode>();
				pSN->m_base.setPos(pRealEvent->m_pos);
				pSN->m_base.setU(pRealEvent->m_u);
				pSN->m_base.setV(pRealEvent->m_v);
				pSN->m_base.setN(pRealEvent->m_n);
			}
		}
	}
	
	// pop the game object table
	m_pContext->getLuaEnvironment()->pop();
}

void GameObjectManager::do_CREATE_ANIM_SET(Events::Event *pEvt)
{
	Events::Event_CREATE_ANIM_SET *pRealEvent = (Events::Event_CREATE_ANIM_SET *)(pEvt);
	bool haveObject = false;
	Handle exisitngObject;

	putGameObjectTableIOnStack();

	if (!haveObject)
	{
		// need to acquire redner context for this code to execute thread-safe
		m_pContext->getGPUScreen()->AcquireRenderContextOwnership(pRealEvent->m_threadOwnershipMask);

		PEASSERT(m_lastAddedSkelInstanceHandle.isValid(), "Adding anim set, so we need a skeleton instance");
		m_lastAddedSkelInstanceHandle.getObject<SkeletonInstance>()->setAnimSet(pRealEvent->animSetFilename, pRealEvent->m_package);
		m_pContext->getGPUScreen()->ReleaseRenderContextOwnership(pRealEvent->m_threadOwnershipMask);
	}
	// pop the game object table
	m_pContext->getLuaEnvironment()->pop();
}

void GameObjectManager::do_CREATE_MESH(Events::Event *pEvt)
{
	Events::Event_CREATE_MESH *pRealEvent = (Events::Event_CREATE_MESH *)(pEvt);
	
	bool haveObject = false;
	bool haveOtherObject = false;
	Handle exisitngObject;

	putGameObjectTableIOnStack();

	if (!pRealEvent->m_peuuid.isZero())
	{
		// have a valid peeuid for the object. need to check if have one already
		
		haveObject = m_pContext->getLuaEnvironment()->checkTableValueByPEUUIDFieldExists(pRealEvent->m_peuuid);
		if (haveObject)
		{
			LuaEnvironment::popHandleFromTableOnStackAndPopTable(m_pContext->getLuaEnvironment()->L, exisitngObject);
		}
		else
		{
			// pop nil
			m_pContext->getLuaEnvironment()->pop();
		}

		if (haveObject)
		{
			Component *pExisiting = exisitngObject.getObject<Component>();

			if (!pExisiting->isInstanceOf<MeshInstance>())
			{
				haveObject = false; // objects can have same id if they are different types, like skeleton + mesh
				haveOtherObject = true;
			}
		}
	}

	if (!haveObject)
	{
	
		// need to acquire redner context for this code to execute thread-safe
		m_pContext->getGPUScreen()->AcquireRenderContextOwnership(pRealEvent->m_threadOwnershipMask);
		
			PE::Handle hMeshInstance("MeshInstance", sizeof(MeshInstance));
			MeshInstance *pMeshInstance = new(hMeshInstance) MeshInstance(*m_pContext, m_arena, hMeshInstance);
			pMeshInstance->addDefaultComponents();

			pMeshInstance->initFromFile(pRealEvent->m_meshFilename, pRealEvent->m_package, pRealEvent->m_threadOwnershipMask);
			
		
		m_pContext->getGPUScreen()->ReleaseRenderContextOwnership(pRealEvent->m_threadOwnershipMask);
		
		// we need to add this mesh to a scene node or to an existing skeleton
		if (pMeshInstance->hasSkinWeights())
		{
			// this mesh has skin weights, so it should belong to a skeleton. assume the last added skeleton is skeleton we need
			PEASSERT(m_lastAddedSkelInstanceHandle.isValid(), "Adding skinned mesh, so we need a skeleton instance");
			m_lastAddedSkelInstanceHandle.getObject<Component>()->addComponent(hMeshInstance);
		}
	else
	{
		// Declare parent SceneNode pointer that will be used for physics
		SceneNode *pParentSN = nullptr;
		
		if (pRealEvent->hasCustomOrientation)
		{
			// need to create a scene node for this mesh
			Handle hSN("SCENE_NODE", sizeof(SceneNode));
			SceneNode *pSN = new(hSN) SceneNode(*m_pContext, m_arena, hSN);
			pSN->addDefaultComponents();

			pSN->addComponent(hMeshInstance);

			RootSceneNode::Instance()->addComponent(hSN);
			pSN->m_base.setPos(pRealEvent->m_pos);
			pSN->m_base.setU(pRealEvent->m_u);
			pSN->m_base.setV(pRealEvent->m_v);
			pSN->m_base.setN(pRealEvent->m_n);
			
			pParentSN = pSN;  // Store for physics component creation
		}
		else
		{
			RootSceneNode::Instance()->addComponent(hMeshInstance);
			pParentSN = RootSceneNode::Instance();  // Use root as parent
		}
		
		// CREATE PHYSICS COMPONENT for static meshes that need collision
		Mesh *pMesh = pMeshInstance->m_hAsset.getObject<Mesh>();
		if (pMesh && pMesh->hasAABB() && pParentSN)
		{
			// ========== INDUSTRY SOLUTION: SELECTIVE PHYSICS ==========
			// Only add physics to meshes that actually need collision detection
			// This avoids unnecessary collision tests and improves performance
			
			// Check mesh name to determine if it needs physics
			bool needsPhysics = true;  // Default: add physics
			const char* meshName = pMesh->m_meshName;
			
			// WHITELIST approach: Only specific objects get physics
			// Uncomment this block and comment out BLACKLIST to use whitelist
			
			needsPhysics = false;  // Start with no physics
			
			// Add physics only to these objects (ground, buildings, walls, etc.)
			if (strstr(meshName, "cobbleplane.x_pplaneshape1_mesh.mesha") != nullptr ||      // Ground planes
			    strstr(meshName, "SoldierTransform.mesha") != nullptr ||                     // Soldier
			    strstr(meshName, "m98.x_m98main_mesh.mesha") != nullptr ||                   // Gun
			    strstr(meshName, "imrod.x_imrodmesh_mesh.mesha") != nullptr ||               // Imrod
			    strstr(meshName, "nazicar.x_carmesh_mesh.mesha") != nullptr ||               // nazi car
			    strstr(meshName, "imrod") != nullptr ||      // Buildings (imrod)
			    strstr(meshName, "obstacle") != nullptr)     // Obstacles
			{
				needsPhysics = true;
			}
			
			
			// BLACKLIST approach: Exclude decorative objects from physics
			// This is currently active - decorations don't need collision
			//if (strstr(meshName, "streetlight.x_main_mesh.mesha") != nullptr ||       // streetlight (decoration)
			//    strstr(meshName, "brickspire.x_main_mesh.mesha") != nullptr)      // background building (decoration)
			//{
			//	needsPhysics = false;
			//}
			
			if (needsPhysics)
			{
				PE::Handle hPhysics("PHYSICS_COMPONENT", sizeof(PhysicsComponent));
				PhysicsComponent *pPhysics = new(hPhysics) PhysicsComponent(*m_pContext, m_arena, hPhysics);
				pPhysics->addDefaultComponents();
				
				const PE::AABB& aabb = pMesh->getLocalAABB();
				
				// Initialize physics properties
				// Store initial position - PhysicsManager will handle world transform in update()
				pPhysics->position = pParentSN->m_base.getPos();
				pPhysics->localCenterOffset = aabb.center;  // Store in LOCAL space
				pPhysics->velocity = Vector3(0.0f, 0.0f, 0.0f);
				pPhysics->acceleration = Vector3(0.0f, 0.0f, 0.0f);
				
				// Static mesh uses AABB for collision
				pPhysics->shapeType = PhysicsComponent::AABB;
				pPhysics->aabbExtents = aabb.extents;
				
				// Static mesh doesn't move
				pPhysics->isStatic = true;
				pPhysics->mass = 0.0f;  // Infinite mass (static)
				
				// Link to SceneNode
				pPhysics->m_linkedSceneNode = pParentSN;
				
				// Add to PhysicsManager
				PhysicsManager::Instance()->addComponent(hPhysics);
				
				// Removed: Spammy during level load (creates log for every static mesh)
				// PEINFO("Created physics component for mesh: %s\n", pMesh->m_meshName);
			}
		}
	}

		if (!haveOtherObject)
			m_pContext->getLuaEnvironment()->pushHandleAsFieldAndSet(pRealEvent->m_peuuid, hMeshInstance);
	}
	else
	{
		// already have this object
		// only care about orientation
		if (pRealEvent->hasCustomOrientation)
		{
			// need to reset the orientation
			// try finding scene node
			MeshInstance *pMeshInstance = exisitngObject.getObject<MeshInstance>();
			Handle hSN = pMeshInstance->getFirstParentByType<SceneNode>();
			if (hSN.isValid())
			{
				SceneNode *pSN = hSN.getObject<SceneNode>();
				pSN->m_base.setPos(pRealEvent->m_pos);
				pSN->m_base.setU(pRealEvent->m_u);
				pSN->m_base.setV(pRealEvent->m_v);
				pSN->m_base.setN(pRealEvent->m_n);
			}
		}
	}

	// pop the game object table
	m_pContext->getLuaEnvironment()->pop();
}



void GameObjectManager::do_CONSTRUCT_SOUND(Events::Event *pEvt)
{
	Event_CONSTRUCT_SOUND *pRealEvent = (Event_CONSTRUCT_SOUND *)(pEvt);
	SoundManager::Construct(*m_pContext, m_arena, pRealEvent->m_waveBankFilename);
}

void GameObjectManager::createGameObjectTableIfDoesntExist()
{
	if (m_luaGameObjectTableRef == LUA_NOREF)
		m_luaGameObjectTableRef = LuaEnvironment::createTableOnTopOfStackAndStoreReference(m_pContext->getLuaEnvironment()->L);

}
void GameObjectManager::putGameObjectTableIOnStack()
{
	if (m_luaGameObjectTableRef == LUA_NOREF)
		createGameObjectTableIfDoesntExist();
	
	LuaEnvironment::putTableOnTopOfStackByReference(m_luaGameObjectTableRef, m_pContext->getLuaEnvironment()->L);
}

	
}; // namespace Components
}; //namespace PE
