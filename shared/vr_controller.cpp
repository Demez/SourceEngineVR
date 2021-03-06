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

#ifdef CLIENT_DLL
#include "beamdraw.h"
#endif


ConVar vr_dbg_pickup("vr_dbg_pickup", "1", FCVAR_CHEAT | FCVAR_REPLICATED);

ConVar vr_pickup_damp("vr_pickup_damp", "0.5", FCVAR_REPLICATED);
ConVar vr_pickup_speed("vr_pickup_speed", "250", FCVAR_REPLICATED);
ConVar vr_pickup_angular("vr_pickup_angular", "3600", FCVAR_REPLICATED);
ConVar vr_pickup_damp_speed("vr_pickup_damp_speed", "500", FCVAR_REPLICATED);
ConVar vr_pickup_damp_angular("vr_pickup_damp_angular", "3600", FCVAR_REPLICATED);

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


const float DEFAULT_MAX_ANGULAR = 360.0f * 10.0f;
const float REDUCED_CARRY_MASS = 1.0f;

const int PALM_DIR_MULT = 4;


void CVRController::Spawn()
{
	BaseClass::Spawn();

	m_pLastUseEntity = NULL;
	m_pGrabbedObject = NULL;
	m_lerpedPointDir.Init();
	m_pointerEnabled = true;
}

// blech
#ifdef PORTAL_DLL
#define physenv physenv_main
#endif


void CVRController::InitTracker( CmdVRTracker& cmdTracker, CVRBasePlayerShared* pPlayer )
{
	BaseClass::InitTracker( cmdTracker, pPlayer );

	m_pController = physenv->CreateMotionController( this );
	m_pController->SetEventHandler( this );
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
}


void CVRController::DropObjectIfReleased()
{
	if ( !CanGrabEntities() && GetGrabbedObject() != NULL )
	{
		DropObject();
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

	m_pGrabbedObject->SetOwnerEntity( NULL );
	m_pGrabbedObject = NULL;
}


#ifdef GAME_DLL
class CControllerCollideList : public IEntityEnumerator
{
public:
	CControllerCollideList( Ray_t *pRay, CBaseEntity* pIgnoreEntity, int nContentsMask ) : 
		m_Entities( 0, 32 ), m_pIgnoreEntity( pIgnoreEntity ),
		m_nContentsMask( nContentsMask ), m_pRay(pRay) {}

	virtual bool EnumEntity( IHandleEntity *pHandleEntity )
	{
		// Don't bother with the ignore entity.
		if ( pHandleEntity == m_pIgnoreEntity )
			return true;

		Assert( pHandleEntity );

		trace_t tr;
		enginetrace->ClipRayToEntity( *m_pRay, m_nContentsMask, pHandleEntity, &tr );
		if (( tr.fraction < 1.0f ) || (tr.startsolid) || (tr.allsolid))
		{
			CBaseEntity *pEntity = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );
			m_Entities.AddToTail( pEntity );
		}

		return true;
	}

	CUtlVector<CBaseEntity*>	m_Entities;

private:
	CBaseEntity		*m_pIgnoreEntity;
	int				m_nContentsMask;
	Ray_t			*m_pRay;
};
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
#ifdef GAME_DLL
	if ( GetGrabbedObject() != NULL )
		return GetGrabbedObject();

	// NOTE: Some debris objects are useable too, so hit those as well
	// A button, etc. can be made out of clip brushes, make sure it's +useable via a traceline, too.
	int useableContents = MASK_SOLID | CONTENTS_DEBRIS | CONTENTS_PLAYERCLIP;

	int boxSize = 4;

	const int NUM_TANGENTS = 8;
	Vector searchCenter = GetAbsOrigin();
	Vector boxMax(boxSize, boxSize, boxSize);
	Vector boxMin(-boxSize, -boxSize, -boxSize);
	trace_t tr;
	CBaseEntity* pObject = NULL;

	Ray_t ray;
	ray.Init( searchCenter, searchCenter + boxMax, boxMin, boxMax );
	CTraceFilterSimple traceFilter( m_pPlayer, COLLISION_GROUP_NONE );

	enginetrace->TraceRay( ray, useableContents, &traceFilter, &tr );

	CControllerCollideList pEntities(&ray, m_pPlayer, useableContents);

	enginetrace->EnumerateEntities( searchCenter + boxMin, searchCenter + boxMax, &pEntities );

	NDebugOverlay::Axis( searchCenter, GetAbsAngles(), 6, false, 0.0f);

	if ( vr_dbg_pickup.GetBool() )
		NDebugOverlay::Box( searchCenter, boxMin, boxMax, 255, 255, 255, 1, 0);

	Vector palmDir = GetPalmDir();

	if ( vr_dbg_pickup.GetBool() )
		NDebugOverlay::Line( searchCenter, searchCenter + (palmDir * PALM_DIR_MULT), 0, 255, 0, 1, 0);

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

	m_pLastUseEntity = pObject;
	return pObject;

#else
	return NULL;
#endif
}


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



// TODO: have this offset from the center of the controller slightly
Vector CVRController::GetPointPos()
{
	// temp hack
	if ( IsLeftHand() )
		return GetAbsOrigin() + Vector(-0.006119, 0.025944, 0.032358);
	else
		return GetAbsOrigin() + Vector(0.006119, 0.025944, 0.032358);
}


Vector CVRController::GetPointDir()
{
	Vector point;
	AngleVectors( GetPointAng(), &point );

	return point;
}


// TODO: adjust this for other controller types
// (but then the server needs to know the hardware used, hmmm, can i get it with openvr?)
QAngle CVRController::GetPointAng()
{
	QAngle angles = m_absAng;

	VMatrix mat, rotateMat, outMat;
	MatrixFromAngles( angles, mat );
	MatrixBuildRotationAboutAxis( rotateMat, Vector( 0, 1, 0 ), 35 );
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

