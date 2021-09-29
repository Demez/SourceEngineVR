#include "cbase.h"
#include "vr_util.h"
#include "vr_tracker.h"
#include "vr_controller.h"
#include "hl2_shareddefs.h"  // TODO: move LINK_ENTITY_TO_CLASS_DUMB out of here
#include "vr_gamemovement.h"
#include "in_buttons.h"
#include "vr_player_shared.h"
#include "vphysics_interface.h"
#include "debugoverlay_shared.h"
#include "collisionutils.h"
#include "tier2/renderutils.h"

#ifdef CLIENT_DLL
#include "beamdraw.h"
#include "iviewrender_beams.h"
extern ConVar vr_dbg_point;
#endif


ConVar vr_dbg_pickup("vr_dbg_pickup", "1", FCVAR_CHEAT | FCVAR_REPLICATED);

#define VR_PICKUP_PARTITION 1

static ConVar vr_pickup_damp("vr_pickup_damp", "0.5", FCVAR_REPLICATED);
static ConVar vr_pickup_speed("vr_pickup_speed", "250", FCVAR_REPLICATED);
static ConVar vr_pickup_angular("vr_pickup_angular", "3600", FCVAR_REPLICATED);
static ConVar vr_pickup_damp_speed("vr_pickup_damp_speed", "500", FCVAR_REPLICATED);
static ConVar vr_pickup_damp_angular("vr_pickup_damp_angular", "3600", FCVAR_REPLICATED);

ConVar vr_pointer_lerp("vr_pointer_lerp", "0.2", FCVAR_REPLICATED);

extern CMoveData* g_pMoveData;

BEGIN_SIMPLE_DATADESC( vr_shadowcontrol_params_t )
	DEFINE_FIELD( targetPosition,   FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( targetRotation,   FIELD_VECTOR ),
	DEFINE_FIELD( maxAngular,       FIELD_FLOAT ),
	DEFINE_FIELD( maxDampAngular,   FIELD_FLOAT ),
	DEFINE_FIELD( maxSpeed,         FIELD_FLOAT ),
	DEFINE_FIELD( maxDampSpeed,     FIELD_FLOAT ),
	DEFINE_FIELD( dampFactor,       FIELD_FLOAT ),
	DEFINE_FIELD( teleportDistance, FIELD_FLOAT ),
END_DATADESC()


static const float DEFAULT_MAX_ANGULAR = 360.0f * 10.0f;
static const float REDUCED_CARRY_MASS = 1.0f;
static const int   PALM_DIR_MULT = 4;


CVRController::CVRController(): CVRTracker()
{
	m_pLastUseEntity = NULL;
	m_pGrabbedObject = NULL;
	m_lerpedPointDir.Init();
	m_pointerEnabled = true;
}

CVRController::~CVRController()
{
}

// blech
#ifdef PORTAL_DLL
#define physenv physenv_main
#endif


void CVRController::InitTracker( CmdVRTracker& cmdTracker, CVRBasePlayerShared* pPlayer )
{
	BaseClass::InitTracker( cmdTracker, pPlayer );

#ifdef CLIENT_DLL
	m_mdl_openxr_aim = LookupAttachment( "openxr_aim" );
#endif

	m_pController = physenv->CreateMotionController( this );
	m_pController->SetEventHandler( this );

	/*BeamInfo_t beamInfo;
	beamInfo.m_nType = TE_BEAMPOINTS;
	beamInfo.m_vecStart = GetPointPos();
	beamInfo.m_vecEnd = GetPointPos();
	beamInfo.m_pszModelName = "sprites/glow01.vmt";
	beamInfo.m_pszHaloName = "sprites/glow01.vmt";
	beamInfo.m_flHaloScale = 3.0;
	beamInfo.m_flWidth = 8.0f;
	beamInfo.m_flEndWidth = 35.0f;
	beamInfo.m_flFadeLength = 300.0f;
	beamInfo.m_flAmplitude = 0;
	beamInfo.m_flBrightness = 60.0;
	beamInfo.m_flSpeed = 0.0f;
	beamInfo.m_nStartFrame = 0.0;
	beamInfo.m_flFrameRate = 0.0;
	beamInfo.m_flRed = 255.0;
	beamInfo.m_flGreen = 255.0;
	beamInfo.m_flBlue = 255.0;
	beamInfo.m_nSegments = 8;
	beamInfo.m_bRenderable = true;
	beamInfo.m_flLife = 0.5;
	beamInfo.m_nFlags = FBEAM_FOREVER | FBEAM_ONLYNOISEONCE | FBEAM_NOTILE | FBEAM_HALOBEAM;

	m_pBeam = beams->CreateBeamPoints( beamInfo );*/
}


void CVRController::UpdateTracker( CmdVRTracker& cmdTracker )
{
	BaseClass::UpdateTracker( cmdTracker );

	DropObjectIfReleased();

	m_shadow.dampFactor = vr_pickup_damp.GetFloat();  // 1.0f
	m_shadow.teleportDistance = 0;
	// make this controller really stiff!
	m_shadow.maxSpeed = vr_pickup_speed.GetFloat();  // 1000
	m_shadow.maxAngular = vr_pickup_angular.GetFloat();  // DEFAULT_MAX_ANGULAR;
	m_shadow.maxDampSpeed = vr_pickup_damp_speed.GetFloat();  // m_shadow.maxSpeed*2;
	m_shadow.maxDampAngular = vr_pickup_damp_angular.GetFloat();  // m_shadow.maxAngular;

	if ( GetGrabbedObject() != NULL )
	{
		UpdateObject();
	}

	// update pointer lerping
	// DEMEZ TODO: add a trace check for the end point up to 32x away and scale the pointer lerping accordingly
	Vector pointDir = GetPointDir();

	if ( m_lerpedPointDir.IsZero() )
		m_lerpedPointDir = pointDir;

	Vector lerpedPointDir(pointDir * 32);

	lerpedPointDir = Lerp( vr_pointer_lerp.GetFloat(), m_lerpedPointDir, lerpedPointDir );

	m_lerpedPointDir = lerpedPointDir;

#ifdef CLIENT_DLL
	if ( vr_dbg_point.GetBool() )
	{
		g_VRRenderer.DrawControllerPointer( this );
	}
#endif
}


#ifdef CLIENT_DLL
int CVRController::DrawModel( int flags RENDER_INSTANCE_INPUT )
{
	int ret = BaseClass::DrawModel( flags RENDER_INSTANCE );

	/*if ( vr_dbg_pickup.GetBool() && m_drawGrabDebug && m_pPlayer->IsLocalPlayer() )
	{
		RenderSphere( GetAbsOrigin(), 4, 32, 32, Color(255, 255, 255, 8), false);
		// RenderSphere( GetAbsOrigin(), 4, 32, 32, Color(255, 255, 255, 8), true);

		debugoverlay->AddSphereOverlay( GetAbsOrigin(), 4, 32, 32, 255, 255, 255, 64, 0 );
		ret = 1;
	}*/

	// this could be used to see when the model is being drawn, just with something else probably though
	if ( ShouldDraw() && !vr_dbg_point.GetBool() )
	{
		g_VRRenderer.DrawControllerPointer( this );
	}

	return ret;
}
#endif


void CVRController::DropObjectIfReleased()
{
	if ( !CanGrabEntities() && GetGrabbedObject() != NULL )
	{
		DropObject();
		m_pLastUseEntity = NULL;
	}
}


void CVRController::DropObject()
{
	if ( GetGrabbedObject() == NULL )
		return;

	DetachEntity( false );

	CBaseAnimating *pAnimating = GetGrabbedObject()->GetMoveParent() ? GetGrabbedObject()->GetMoveParent()->GetBaseAnimating() : GetGrabbedObject()->GetBaseAnimating();
	if (!pAnimating)
		return;

	IPhysicsObject *ppList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	int nCount = pAnimating->VPhysicsGetObjectList( ppList, ARRAYSIZE(ppList) );
	if ( nCount > 0 )
	{
		for ( int i = 0; i < nCount; ++i )
		{
			m_pController->DetachObject( ppList[i] );
			PhysClearGameFlags( ppList[i], FVPHYSICS_PLAYER_HELD );
		}
	}

	// VR TODO: if you're holding the object close to you and let go
	// it can go flying away, and be annoying with switching what hand you want to grab it with
	m_pGrabbedObject->SetOwnerEntity( NULL );
	m_pGrabbedObject = NULL;
}


#if 1 // def GAME_DLL

#if VR_PICKUP_PARTITION
class CControllerCollideList : public IPartitionEnumerator
{
public:
	CControllerCollideList( Ray_t *pRay, CBasePlayer* pPlayer, int flagMask ) : 
		m_Entities( 0, 0 ), m_pPlayer( pPlayer ),
		m_nContentsMask( flagMask ), m_pRay( pRay )
	{
	}

	IterationRetval_t EnumElement( IHandleEntity *pHandleEntity )
	{
		// Don't bother with the player
		if ( pHandleEntity == m_pPlayer )
			return ITERATION_CONTINUE;

#if defined( CLIENT_DLL )
		IClientEntity *pClientEntity = cl_entitylist->GetClientEntityFromHandle( pHandleEntity->GetRefEHandle() );
		C_BaseEntity *pEntity = pClientEntity ? pClientEntity->GetBaseEntity() : NULL;
#else
		CBaseEntity *pEntity = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );
#endif

		if ( pEntity )
		{
			// needed to check if this is actually collides with the vphysics model or not
			trace_t tr;
			// enginetrace->ClipRayToEntity( *m_pRay, m_nContentsMask, pHandleEntity, &tr );
			enginetrace->ClipRayToCollideable( *m_pRay, m_nContentsMask, pEntity->GetCollideable(), &tr );
			if (( tr.fraction < 1.0f ) || (tr.startsolid) || (tr.allsolid))
			{
				if ( m_pPlayer->IsUseableEntity(pEntity, 0) )
				{
					m_Entities.AddToTail( pEntity );
				}
			}

			if ( m_Entities.Count() == 32 )
				return ITERATION_STOP;
		}

		return ITERATION_CONTINUE;
	}

	CUtlVector<CBaseEntity*>	m_Entities;

private:
	CBasePlayer		*m_pPlayer;
	int				m_nContentsMask;
	Ray_t			*m_pRay;
};

#else

class CControllerCollideList : public IEntityEnumerator
{
public:
	CControllerCollideList( Ray_t *pRay, CBasePlayer* pPlayer, int nContentsMask ) : 
		m_Entities( 0, 32 ), m_pPlayer( pPlayer ),
		m_nContentsMask( nContentsMask ), m_pRay(pRay) {}

	virtual bool EnumEntity( IHandleEntity *pHandleEntity )
	{
		// Don't bother with the ignore entity.
		if ( pHandleEntity == m_pPlayer )
			return true;

		Assert( pHandleEntity );

#if defined( CLIENT_DLL )
		IClientEntity *pClientEntity = cl_entitylist->GetClientEntityFromHandle( pHandleEntity->GetRefEHandle() );
		C_BaseEntity *pEntity = pClientEntity ? pClientEntity->GetBaseEntity() : NULL;
#else
		CBaseEntity *pEntity = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );
#endif

		if ( pEntity )
		{
			trace_t tr;
			enginetrace->ClipRayToEntity( *m_pRay, m_nContentsMask, pHandleEntity, &tr );
			if (( tr.fraction < 1.0f ) || (tr.startsolid) || (tr.allsolid))
			{
				if ( m_pPlayer->IsUseableEntity(pEntity, 0) )
				{
					m_Entities.AddToTail( pEntity );
				}
			}
		}

		return true;
	}

	CUtlVector<CBaseEntity*>	m_Entities;

private:
	CBasePlayer		*m_pPlayer;
	int				m_nContentsMask;
	Ray_t			*m_pRay;
};
#endif

#endif


// right now we always can if use is held down, will setup later for when holding weapons in a new class
bool CVRController::CanGrabEntities()
{
	return IsLeftHand() ? (g_pMoveData->m_nButtons & IN_PICKUP_LEFT) : (g_pMoveData->m_nButtons & IN_PICKUP_RIGHT);
}


// find entity your pointing to
CBaseEntity* CVRController::FindUseEntity()
{
	return FindEntityBase( false );
}

// useless function
CBaseEntity* CVRController::FindGrabEntity()
{
	return FindEntityBase( true );
}


CBaseEntity* CVRController::FindEntityBase( bool mustBeInPalm )
{
#if 1 // def GAME_DLL
	if ( GetGrabbedObject() != NULL )
		return GetGrabbedObject();

	// NOTE: Some debris objects are useable too, so hit those as well
	// A button, etc. can be made out of clip brushes, make sure it's +useable via a traceline, too.
	int useableContents = MASK_SOLID | CONTENTS_DEBRIS | CONTENTS_PLAYERCLIP;
	Vector searchCenter = GetAbsOrigin();

	CBaseEntity* pObject = NULL;

	// VR IDEA: could try using ICollidable for this in the future if i ever get tracker collisions setup
	// and then maybe use SweepCollideable()? or could just do some other check when a collision occurs

#if VR_PICKUP_PARTITION

	float distToNearest = 99999999.0f;
	// const float radius = 2.0f;
	Vector palmDir = GetPalmDir();

	int boxSize = 4;
	Vector boxMax(boxSize, boxSize, boxSize);
	Vector boxMin(-boxSize, -boxSize, -boxSize);

	Ray_t ray;
	ray.Init( searchCenter, searchCenter, boxMin, boxMax );
	CTraceFilterSimple traceFilter( m_pPlayer, COLLISION_GROUP_NONE );

	CControllerCollideList pEntities( &ray, m_pPlayer, useableContents );

	// partition->EnumerateElementsInSphere( PARTITION_ENGINE_NON_STATIC_EDICTS, searchCenter, radius, false, &pEntities );
	// partition->EnumerateElementsInBox( PARTITION_ENGINE_NON_STATIC_EDICTS, searchCenter, boxMin, boxMax, false, &pEntities );
	partition->EnumerateElementsInBox( PARTITION_ENGINE_SOLID_EDICTS, searchCenter + boxMin, searchCenter + boxMax, false, &pEntities );

	if ( vr_dbg_pickup.GetBool() )
	{
		NDebugOverlay::Box( searchCenter, boxMin, boxMax, 255, 255, 255, 1, 0);
		NDebugOverlay::Line( searchCenter, searchCenter + (palmDir * PALM_DIR_MULT), 0, 255, 0, 1, 0);
	}

	for( int iEntity = 0; iEntity < pEntities.m_Entities.Count(); iEntity++ )
	{
		CBaseEntity *pEntity = pEntities.m_Entities[iEntity];

		if ( mustBeInPalm )
		{
			Vector handToEnt( pEntity->GetAbsOrigin() - GetAbsOrigin() );
			VectorNormalize( handToEnt );

			float palmToEnt = palmDir.Dot( handToEnt );
			if ( palmToEnt < -0.1 )
				continue;
		}

		float flDist = (pEntity->GetAbsOrigin() - GetAbsOrigin()).LengthSqr();
		if (flDist < distToNearest)
		{
			pObject = pEntity;
			distToNearest = flDist;
		}
	}

#else

	int boxSize = 4;
	const int NUM_TANGENTS = 8;
	Vector boxMax(boxSize, boxSize, boxSize);
	Vector boxMin(-boxSize, -boxSize, -boxSize);
	trace_t tr;

	Ray_t ray;
	ray.Init( searchCenter, searchCenter + boxMax, boxMin, boxMax );
	CTraceFilterSimple traceFilter( m_pPlayer, COLLISION_GROUP_NONE );

	CControllerCollideList pEntities(&ray, m_pPlayer, useableContents);

	enginetrace->EnumerateEntities( searchCenter + boxMin, searchCenter + boxMax, &pEntities );

	Vector palmDir = GetPalmDir();

	if ( vr_dbg_pickup.GetBool() )
	{
		NDebugOverlay::Box( searchCenter, boxMin, boxMax, 255, 255, 255, 1, 0);
		NDebugOverlay::Line( searchCenter, searchCenter + (palmDir * PALM_DIR_MULT), 0, 255, 0, 1, 0);
	}

	float distToNearest = 99999999.0f;

	for( int iEntity = pEntities.m_Entities.Count(); --iEntity >= 0; )
	{
		CBaseEntity *pEntity = pEntities.m_Entities[iEntity];
		if ( pEntity == NULL )
			break;

		if ( pEntity == m_pPlayer || !m_pPlayer->IsUseableEntity(pEntity, 0) )
			continue;

		if ( mustBeInPalm )
		{
			Vector handToEnt( pEntity->GetAbsOrigin() - GetAbsOrigin() );
			VectorNormalize( handToEnt );

			float palmToEnt = palmDir.Dot( handToEnt );
			if ( palmToEnt < -0.1 )
				continue;
		}

		float flDist = (pEntity->GetAbsOrigin() - GetAbsOrigin()).LengthSqr();
		if (flDist < distToNearest)
		{
			pObject = pEntity;
			distToNearest = flDist;
		}
	}
#endif

	CVRController* otherHand = IsRightHand() ? m_pPlayer->GetLeftHand() : m_pPlayer->GetRightHand();

	// if we were holding this object, and the other hand took it, then we shouldn't try to grab this object again
	if ( pObject && otherHand && otherHand->GetGrabbedObject() == pObject && GetLastUseEntity() == pObject )
	{
		return NULL;
	}

	m_pLastUseEntity = pObject;
	return pObject;

#else
	return NULL;
#endif
}

#ifdef GAME_DLL
// unused, but is this needed?
bool CVRController::Intersects( CBaseEntity *pOther )
{
	if ( !pOther->edict() )
		return false;

	// CCollisionProperty *pMyProp = CollisionProp();
	CCollisionProperty *pOtherProp = pOther->CollisionProp();

	const int s = 4;

	return IsOBBIntersectingOBB( 
		GetAbsOrigin(), GetAbsAngles(), Vector(-s, -s, -s), Vector(s, s, s),
		pOtherProp->GetCollisionOrigin(), pOtherProp->GetCollisionAngles(), pOtherProp->OBBMins(), pOtherProp->OBBMaxs() );
}
#endif


void CVRController::GrabObject( CBaseEntity* pEntity )
{
	DropObjectIfReleased();

	// not holding use
	if ( !CanGrabEntities() )
		return;

	if ( !pEntity )
		return;

	if ( pEntity == GetGrabbedObject() )
		return;

	CVRController* otherHand = IsRightHand() ? m_pPlayer->GetLeftHand() : m_pPlayer->GetRightHand();
	if ( otherHand && otherHand->GetGrabbedObject() == pEntity )
	{
		otherHand->DropObject();
	}

	// TODO: check if we are holding this object with the other controller, or if anyone else is holding this object
	// right now we can't hold the object with two hands

	Vector palmDir = GetPalmDir();

	if ( vr_dbg_pickup.GetBool() )
		NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + (palmDir * PALM_DIR_MULT), 0, 255, 0, 1, 0);

	Vector handToEnt( pEntity->GetAbsOrigin() - GetAbsOrigin() );
	VectorNormalize( handToEnt );

	float palmToEnt = palmDir.Dot( handToEnt );

	// is the entity in the palm of our hand?
	if ( palmToEnt < -0.1 )
		return;

	// we can pickup this entity
	m_pGrabbedObject = pEntity;
	m_pGrabbedObject->SetOwnerEntity( m_pPlayer );

	CBaseAnimating *pAnimating = pEntity->GetMoveParent() ? pEntity->GetMoveParent()->GetBaseAnimating() : pEntity->GetBaseAnimating();
	if (!pAnimating)
		return;

	IPhysicsObject *ppList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	int nCount = pAnimating->VPhysicsGetObjectList( ppList, ARRAYSIZE(ppList) );
	if ( nCount > 0 )
	{
		for ( int i = 0; i < nCount; ++i )
		{
			m_pController->AttachObject( ppList[i], true );
			PhysSetGameFlags( ppList[i], FVPHYSICS_PLAYER_HELD );
		}
	}

	WorldToLocal( GetAbsOrigin(), GetAbsAngles(), pEntity->GetLocalOrigin(), pEntity->GetLocalAngles(), m_entLocalPos, m_entLocalAng );
}


Vector CVRController::GetPalmDir()
{
	Vector palmDir;
	AngleVectors( GetAbsAngles(), NULL, &palmDir, NULL );
	VectorNormalize( palmDir );

	if ( IsRightHand() )
		palmDir *= -1;  // flip it so it's actually where your palm is facing on the left hand

	return palmDir;
}


#ifdef CLIENT_DLL
// VR TODO: disabled by default for now due to not having any way to get these values on the server as of right now
// if i can't figure out a way to load the model on the server live,
// then maybe i will have to use a json parser and read certain values from it
// though that would require the server to have steamvr installed, or at least just have the json files
// or actually use that device type idea i was doing earlier
ConVar vr_point_mdl_pos("vr_point_mdl_pos", "0", 0, "use openxr_aim attachment position (ONLY ON CLIENT, NOT SERVER)");
#endif


Vector CVRController::GetPointPos()
{
#ifdef CLIENT_DLL
	if ( !vr_point_mdl_pos.GetBool() )
		return GetAbsOrigin();

	matrix3x4_t mat_openxr_aim;
	GetAttachment( m_mdl_openxr_aim, mat_openxr_aim );

	Vector rawPos;
	MatrixGetColumn( mat_openxr_aim, 3, rawPos ); 

	Vector pos( rawPos.z, rawPos.x, rawPos.y );

	// VR TODO: network the scale i guess
#ifdef CLIENT_DLL
	pos *= g_VR.GetScale();
#else
	pos *= 42.5;
#endif

	Vector finalOrigin = pos;

	matrix3x4_t matEntityToParent, matEntityCoord, matTrackerCoord;
	AngleMatrix( GetPointAng(), matEntityToParent );
	MatrixSetColumn( pos, 3, matEntityToParent );

	AngleMatrix( m_absAng, vec3_origin, matTrackerCoord );
	ConcatTransforms( matTrackerCoord, matEntityToParent, matEntityCoord );

	MatrixGetColumn( matEntityCoord, 3, finalOrigin ); 

	return GetAbsOrigin() - finalOrigin;
#else
	// uhhh
	return GetAbsOrigin();
#endif
}


Vector CVRController::GetPointDir()
{
	Vector point;
	AngleVectors( GetPointAng(), &point );

	return point;
}


// VR TODO: also read the angle from the controller json file
QAngle CVRController::GetPointAng()
{
	QAngle angles = m_absAng;

#ifdef CLIENT_DLL
	matrix3x4_t mat_openxr_aim;
	GetAttachment( m_mdl_openxr_aim, mat_openxr_aim );

	QAngle openxr_aim_ang;
	MatrixAngles( mat_openxr_aim, openxr_aim_ang ); 

	float rotation = openxr_aim_ang.y;
#else
	// hardcoded for cv1 controllers right now
	float rotation = 39.4;
#endif

	VMatrix mat, rotateMat, outMat;
	MatrixFromAngles( angles, mat );
	MatrixBuildRotationAboutAxis( rotateMat, Vector( 0, 1, 0 ), rotation );
	MatrixMultiply( mat, rotateMat, outMat );
	MatrixToAngles( outMat, angles );

	return angles;
}


void CVRController::SetDrawPointBeam( bool draw )
{
	m_pointerEnabled = draw;
}


bool CVRController::UpdateObject()
{
	CBaseEntity *pEntity = GetGrabbedObject();
	if ( !pEntity )
		return false;

	// if ( ComputeError() > flError )
	// 	return false;

	// if (!pEntity->VPhysicsGetObject() )
	// 	return false;

	//Adrian: Oops, our object became motion disabled, let go!
	IPhysicsObject *pPhys = pEntity->VPhysicsGetObject();
	if ( pPhys && pPhys->IsMoveable() == false )
	{
		return false;
	}

	if ( m_frameCount == gpGlobals->framecount )
	{
		return true;
	}

	m_frameCount = gpGlobals->framecount;

	Vector finalOrigin;
	QAngle finalAngles;

	matrix3x4_t matEntityToParent, matEntityCoord;
	AngleMatrix( m_entLocalAng, matEntityToParent );
	MatrixSetColumn( m_entLocalPos, 3, matEntityToParent );

	// slap the saved local entity origin and angles on top of the hands origin and angles
	ConcatTransforms( GetWorldCoordinate(), matEntityToParent, matEntityCoord );

	// pull our absolute position and angles out of the matrix
	MatrixGetColumn( matEntityCoord, 3, finalOrigin ); 
	MatrixAngles( matEntityCoord, finalAngles );

	if ( vr_dbg_pickup.GetBool() )
		NDebugOverlay::Line( GetAbsOrigin(), finalOrigin, 0, 255, 0, 1, 0);

	SetTargetPosition( finalOrigin, finalAngles );

	return true;
}


void CVRController::SetTargetPosition( const Vector &target, const QAngle &targetOrientation )
{
	m_shadow.targetPosition = target;
	m_shadow.targetRotation = targetOrientation;

	m_timeToArrive = gpGlobals->frametime;

	CBaseEntity *pAttached = GetGrabbedObject();
	if ( pAttached )
	{
		IPhysicsObject *pObj = pAttached->VPhysicsGetObject();

		if ( pObj != NULL )
		{
			pObj->Wake();
		}
		else
		{
			DropObject();
		}
	}
}


void CVRController::DetachEntity( bool bClearVelocity )
{
	CBaseEntity *pEntity = GetGrabbedObject();
	if ( pEntity )
	{
		// Restore the LS blocking state
		IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
		int count = pEntity->VPhysicsGetObjectList( pList, ARRAYSIZE(pList) );

		for ( int i = 0; i < count; i++ )
		{
			IPhysicsObject *pPhys = pList[i];
			if ( !pPhys )
				continue;

			// on the odd chance that it's gone to sleep while bring picked up
			pPhys->EnableDrag( true );
			pPhys->Wake();
			PhysClearGameFlags( pPhys, FVPHYSICS_PLAYER_HELD );

			if ( bClearVelocity )
			{
				PhysForceClearVelocity( pPhys );
			}
			else
			{
				#ifndef CLIENT_DLL
				// ClampPhysicsVelocity( pPhys, hl2_normspeed.GetFloat() * 1.5f, 2.0f * 360.0f );
				#endif
			}

		}
	}

	if ( physenv )
	{
		// physenv->DestroyMotionController( m_pController );
	}

	// m_pController = NULL;
}


IMotionEvent::simresult_e CVRController::Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular )
{
	// idfk, just copied from gravity gun

	vr_shadowcontrol_params_t shadowParams = m_shadow;
	/*if ( InContactWithHeavyObject( pObject, GetLoadWeight() ) )
	{
		m_contactAmount = Approach( 0.1f, m_contactAmount, deltaTime*2.0f );
	}
	else*/
	{
		m_contactAmount = Approach( 1.0f, m_contactAmount, deltaTime*2.0f );
	}

	shadowParams.maxAngular = m_shadow.maxAngular * m_contactAmount * m_contactAmount * m_contactAmount;

#ifndef CLIENT_DLL
	m_timeToArrive = pObject->ComputeShadowControl( shadowParams, m_timeToArrive, deltaTime );
#else
	m_timeToArrive = pObject->ComputeShadowControl( shadowParams, (TICK_INTERVAL*2), deltaTime );
#endif
	
	// Slide along the current contact points to fix bouncing problems
	Vector velocity;
	AngularImpulse angVel;
	pObject->GetVelocity( &velocity, &angVel );
	PhysComputeSlideDirection( pObject, velocity, angVel, &velocity, &angVel, GetLoadWeight() );
	pObject->SetVelocityInstantaneous( &velocity, NULL );

	linear.Init();
	angular.Init();
	// m_errorTime += deltaTime;

	return SIM_LOCAL_ACCELERATION;
}


// dumb function
void CVRController::GetFingerBoneNames( const char* fingerBoneNames[FINGER_BONE_COUNT] )
{
	if ( IsLeftHand() )
	{
	}
	else if ( IsRightHand() )
	{
	}

	fingerBoneNames = '\0';
}

