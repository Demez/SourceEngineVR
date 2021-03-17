#include "cbase.h"
#include "vr_util.h"
#include "vr_tracker.h"
#include "vr_controller.h"
#include "hl2_shareddefs.h"  // TODO: move LINK_ENTITY_TO_CLASS_DUMB out of here
#include "vr_gamemovement.h"
#include "in_buttons.h"
#include "vr_player_shared.h"
#include "vphysics_interface.h"


LINK_ENTITY_TO_CLASS_DUMB( vr_controller, CVRController ); // TEMP: make a separate CVRController class that uses CVRTracker

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

void CVRController::Spawn()
{
	BaseClass::Spawn();

	SetSolid( SOLID_BBOX );	
	AddSolidFlags( FSOLID_NOT_SOLID );

	m_shadow.dampFactor = 1.0;
	m_shadow.teleportDistance = 0;
	// make this controller really stiff!
	m_shadow.maxSpeed = 1000;
	m_shadow.maxAngular = DEFAULT_MAX_ANGULAR;
	m_shadow.maxDampSpeed = m_shadow.maxSpeed*2;
	m_shadow.maxDampAngular = m_shadow.maxAngular;
}


void CVRController::InitTracker( CmdVRTracker& cmdTracker, CVRBasePlayerShared* pPlayer )
{
	BaseClass::InitTracker( cmdTracker, pPlayer );

	m_pController = physenv->CreateMotionController( this );
	m_pController->SetEventHandler( this );

	/*CBaseAnimating *pAnimating = GetMoveParent() ? GetMoveParent()->GetBaseAnimating() : NULL;
	if (!pAnimating)
		return;

	// NOTE: IsRagdoll means *client-side* ragdoll. We shouldn't be trying to fight
	// the server ragdoll (or any server physics) on the client
	if (( !m_pController ) && pAnimating->IsRagdoll())
	{
		IPhysicsObject *ppList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
		int nCount = pAnimating->VPhysicsGetObjectList( ppList, ARRAYSIZE(ppList) );
		if ( nCount > 0 )
		{
			m_pController = physenv->CreateMotionController( this );
			for ( int i = 0; i < nCount; ++i )
			{
				m_pController->AttachObject( ppList[i], true );
			}
		}
	}*/
}


void CVRController::UpdateTracker( CmdVRTracker& cmdTracker )
{
	BaseClass::UpdateTracker( cmdTracker );

	DropObjectIfReleased();

	if ( m_pGrabbedObject == NULL )
		return;

	UpdateObject();

	// do this better, so you have it offset from your hand a little bit somehow, or so it doesn't snap to your hand either, idk
	// m_pGrabbedEntity->SetAbsOrigin( GetAbsOrigin() );
	// m_pGrabbedEntity->SetAbsAngles( GetAbsAngles() );
}


void CVRController::DropObjectIfReleased()
{
	if ( !CanGrabEntities() && m_pGrabbedObject != NULL )
	{
		DropObject();
	}
}


void CVRController::DropObject()
{
	if ( m_pGrabbedObject == NULL )
		return;

	// Demez TODO: how can i have the entity keep its velocity when i let go of it?
	// do i have to calculate the velocity and add it to the entity im holding after i remove it's parent?

	// let go of entity
	// m_pGrabbedObject->SetParent( NULL );

	DetachEntity( false );

	CBaseAnimating *pAnimating = m_pGrabbedObject->GetMoveParent() ? m_pGrabbedObject->GetMoveParent()->GetBaseAnimating() : m_pGrabbedObject->GetBaseAnimating();
	if (!pAnimating)
		return;

	IPhysicsObject *ppList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	int nCount = pAnimating->VPhysicsGetObjectList( ppList, ARRAYSIZE(ppList) );
	if ( nCount > 0 )
	{
		for ( int i = 0; i < nCount; ++i )
		{
			m_pController->DetachObject( ppList[i] );
		}
	}

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
	return (g_pMoveData->m_nButtons & IN_USE);
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
	if ( m_pGrabbedObject != NULL )
		return m_pGrabbedObject;

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
	ray.Init( GetAbsOrigin(), GetAbsOrigin() + boxMax, boxMin, boxMax );
	CTraceFilterSimple traceFilter( this, COLLISION_GROUP_NONE );

	enginetrace->TraceRay( ray, useableContents, &traceFilter, &tr );

	CControllerCollideList pEntities(&ray, this, useableContents);
	enginetrace->EnumerateEntities( GetAbsOrigin() + boxMin, GetAbsOrigin() + boxMax, &pEntities );

	if ( r_visualizetraces.GetBool() )
		NDebugOverlay::Box( GetAbsOrigin(), boxMin, boxMax, 255, 255, 255, 1, 0);

	Vector forward, palmDir, up;
	AngleVectors(GetAbsAngles(), &forward, &palmDir, &up);
	VectorNormalize(forward);
	VectorNormalize(palmDir);
	VectorNormalize(up);
	palmDir *= -1;  // flip it so it's actually where your palm is facing

	if ( r_visualizetraces.GetBool() && mustBeInPalm )
		NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + (palmDir * 4), 0, 255, 0, 1, 0);

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

	if ( pEntity == m_pGrabbedObject )
		return;

	Vector palmDir;
	AngleVectors(GetAbsAngles(), NULL, &palmDir, NULL);
	VectorNormalize(palmDir);
	palmDir *= -1;  // flip it so it's actually where your palm is facing

	if ( r_visualizetraces.GetBool() )
		NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + (palmDir * 8), 0, 255, 0, 1, 0);

	Vector handToEnt( pEntity->GetAbsOrigin() - GetAbsOrigin() );
	VectorNormalize( handToEnt );

	float palmToEnt = palmDir.Dot( handToEnt );

	// is the entity in the palm of our hand?
	if ( palmToEnt < -0.1 )
		return;

	// we can pickup this entity
	m_pGrabbedObject = pEntity;

	CBaseAnimating *pAnimating = m_pGrabbedObject->GetMoveParent() ? m_pGrabbedObject->GetMoveParent()->GetBaseAnimating() : m_pGrabbedObject->GetBaseAnimating();
	if (!pAnimating)
		return;

	IPhysicsObject *ppList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	int nCount = pAnimating->VPhysicsGetObjectList( ppList, ARRAYSIZE(ppList) );
	if ( nCount > 0 )
	{
		for ( int i = 0; i < nCount; ++i )
		{
			m_pController->AttachObject( ppList[i], true );
		}
	}

	// m_grabbedObjectPos = GetAbsOrigin() - m_pGrabbedObject->GetAbsOrigin();
	// m_grabbedObjectAng = GetAbsAngles() - m_pGrabbedObject->GetAbsAngles();

	m_grabbedObjectPos = m_pGrabbedObject->GetAbsOrigin();
	m_grabbedObjectAng = m_pGrabbedObject->GetAbsAngles();

	m_grabbedObjectLocalPos = m_pGrabbedObject->GetLocalOrigin();
	m_grabbedObjectLocalAng = m_pGrabbedObject->GetLocalAngles();

	m_grabbedObjectPosDiff = GetAbsOrigin() - m_pGrabbedObject->GetAbsOrigin();
	m_grabbedObjectAngDiff = GetAbsAngles() - m_pGrabbedObject->GetAbsAngles();

	m_handPos = GetAbsOrigin();
	m_handAng = GetAbsAngles();

	// m_pGrabbedObject->SetParent( this );
}


bool CVRController::UpdateObject()
{
	CBaseEntity *pEntity = m_pGrabbedObject;
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

#if 0

	Vector forward, right, up;
	QAngle playerAngles = GetAbsAngles();

	float pitch = AngleDistance(playerAngles.x,0);
	playerAngles.x = clamp( pitch, -75, 75 );
	AngleVectors( playerAngles, &forward, &right, &up );

	// Now clamp a sphere of object radius at end to the player's bbox
	Vector radial = physcollision->CollideGetExtent( pPhys->GetCollide(), vec3_origin, pEntity->GetAbsAngles(), -forward );
	// Vector player2d = m_pPlayer->CollisionProp()->OBBMaxs();
	// float playerRadius = player2d.Length2D();
	// float flDot = DotProduct( forward, radial );

	// float radius = playerRadius + fabs( flDot );

	// float distance = 24 + ( radius * 2.0f );
	float radius = 1.0f;
	float distance = 1.0f;

	Vector start = GetAbsOrigin();
	Vector end = start + ( forward * distance );

	trace_t	tr;
	CTraceFilterSkipTwoEntities traceFilter( m_pPlayer, pEntity, COLLISION_GROUP_NONE );
	Ray_t ray;
	ray.Init( start, end );
	enginetrace->TraceRay( ray, MASK_SOLID_BRUSHONLY, &traceFilter, &tr );

	if ( tr.fraction < 0.5 )
	{
		end = start + forward * (radius*0.5f);
	}
	else if ( tr.fraction <= 1.0f )
	{
		end = start + forward * ( distance - radius );
	}

	Vector playerMins, playerMaxs, nearest;
	m_pPlayer->CollisionProp()->WorldSpaceAABB( &playerMins, &playerMaxs );
	Vector playerLine = m_pPlayer->CollisionProp()->WorldSpaceCenter();
	CalcClosestPointOnLine( end, playerLine+Vector(0,0,playerMins.z), playerLine+Vector(0,0,playerMaxs.z), nearest, NULL );

	Vector delta = end - nearest;
	float len = VectorNormalize(delta);
	if ( len < radius )
	{
		end = nearest + radius * delta;
	}

	// QAngle angles = TransformAnglesFromPlayerSpace( m_attachedAnglesPlayerSpace, m_pPlayer );
	QAngle angles = TransformAnglesToWorldSpace( GetAbsAngles(), m_pPlayer->EntityToWorldTransform() );

	//Show overlays of radius
	if ( false )
	{

#ifdef CLIENT_DLL

		debugoverlay->AddBoxOverlay( end, -Vector( 2,2,2 ), Vector(2,2,2), angles, 0, 255, 255, true, 0 );

		debugoverlay->AddBoxOverlay( m_pGrabbedObject->WorldSpaceCenter(), 
							-Vector( radius, radius, radius), 
							Vector( radius, radius, radius ),
							angles,
							255, 255, 0,
							true,
							0.0f );

#else

		NDebugOverlay::Box( end, -Vector( 2,2,2 ), Vector(2,2,2), 0, 255, 0, true, 0 );

		NDebugOverlay::Box( m_pGrabbedObject->WorldSpaceCenter(), 
							-Vector( radius+5, radius+5, radius+5), 
							Vector( radius+5, radius+5, radius+5 ),
							255, 0, 0,
							true,
							0.0f );
#endif
	}
	
#ifndef CLIENT_DLL
	// If it has a preferred orientation, update to ensure we're still oriented correctly.
	/*Pickup_GetPreferredCarryAngles( pEntity, pPlayer, pPlayer->EntityToWorldTransform(), angles );


	// We may be holding a prop that has preferred carry angles
	if ( m_bHasPreferredCarryAngles )
	{
		matrix3x4_t tmp;
		ComputePlayerMatrix( pPlayer, tmp );
		angles = TransformAnglesToWorldSpace( m_vecPreferredCarryAngles, tmp );
	}*/

#endif


	matrix3x4_t attachedToWorld;
	Vector offset;
	AngleMatrix( angles, attachedToWorld );
	VectorRotate( m_attachedPositionObjectSpace, attachedToWorld, offset );

#endif
	
#ifdef GAME_DLL

	// ----------------------------------------------------

	Vector handOrigin = GetAbsOrigin();
	QAngle handAngles = GetAbsAngles();

	// Vector entOrigin = m_pGrabbedObject->GetAbsOrigin();
	// QAngle entAngles = m_pGrabbedObject->GetAbsAngles();

	Vector originDiff = m_grabbedObjectPos - m_handPos;

	Vector entGrabOriginOffset;
	VectorYawRotate(originDiff, GetVRMoveData()->vr_viewRotation, entGrabOriginOffset);

	
	// Issue: rotation is not applied properly depending on how your facing it when you grab it
	QAngle angleDiff = m_grabbedObjectAng - m_handAng;
	NormalizeAngle( angleDiff );
	QAngle finalAngle = angleDiff + handAngles;


	// Vector originDiff = handOrigin - m_pGrabbedObject->GetAbsOrigin();
	Vector finalOrigin = handOrigin + entGrabOriginOffset;

	SetTargetPosition( finalOrigin, finalAngle );

	m_lastHandPos = handOrigin;
	m_lastHandAng = handAngles;


	/*EntityMatrix pickupMatrix, currentMatrix, entMatrix;
	pickupMatrix.SetupMatrixOrgAngles( m_handPos, m_handAng ); // parent->world
	currentMatrix.SetupMatrixOrgAngles( handOrigin, handAngles );
	entMatrix.SetupMatrixOrgAngles( m_grabbedObjectPos, m_grabbedObjectAng ); // child->world

	Vector entGrabOriginWorld;
	VectorYawRotate(originDiff, g_pMoveData->vr_viewRotation, entGrabOriginWorld);
	Vector localOrigin = pickupMatrix.WorldToLocal( entGrabOriginWorld );
	// Vector localOrigin = m_pGrabbedObject->GetAbsOrigin();
	// Vector localOrigin = matrix.WorldToLocal( GetAbsOrigin() );
	// Vector localOrigin = matrix.LocalToWorld( GetLocalOrigin() );

	// I have the axes of local space in world space. (childMatrix)
	// I want to compute those world space axes in the parent's local space
	// and set that transform (as angles) on the child's object so the net
	// result is that the child is now in parent space, but still oriented the same way
	VMatrix tmp = pickupMatrix.Transpose(); // world->parent
	tmp.MatrixMul( entMatrix, pickupMatrix ); // child->parent
	QAngle angles;
	MatrixToAngles( pickupMatrix, angles );*/




	/*EntityMatrix matrix, childMatrix;
	matrix.InitFromEntity( this, m_iParentAttachment ); // parent->world
	childMatrix.InitFromEntityLocal( m_pGrabbedObject ); // child->world
	Vector localOrigin = matrix.WorldToLocal( entGrabOriginOffset );
	// Vector localOrigin = m_pGrabbedObject->GetAbsOrigin();
	// Vector localOrigin = matrix.WorldToLocal( GetAbsOrigin() );
	// Vector localOrigin = matrix.LocalToWorld( GetLocalOrigin() );

	// I have the axes of local space in world space. (childMatrix)
	// I want to compute those world space axes in the parent's local space
	// and set that transform (as angles) on the child's object so the net
	// result is that the child is now in parent space, but still oriented the same way
	VMatrix tmp = matrix.Transpose(); // world->parent
	tmp.MatrixMul( childMatrix, matrix ); // child->parent
	QAngle angles;
	MatrixToAngles( matrix, angles );


	// concatenate with our parent's transform
	matrix3x4a_t tmpMatrix;
	matrix3x4a_t scratchSpace;

	// ConcatTransforms( GetParentToWorldTransform( scratchSpace ), m_pGrabbedObject->m_rgflCoordinateFrame, tmpMatrix );
	ConcatTransforms( EntityToWorldTransform(), m_pGrabbedObject->m_rgflCoordinateFrame, tmpMatrix );
	MatrixCopy( tmpMatrix, m_pGrabbedObject->m_rgflCoordinateFrame );

	// pull our absolute position out of the matrix
	// MatrixGetColumn( m_pGrabbedObject->m_rgflCoordinateFrame, 3, localOrigin ); 

	// if we have any angles, we have to extract our absolute angles from our matrix
	if ( m_grabbedObjectAng == vec3_angle )
	{
		// just copy our parent's absolute angles
		// VectorCopy( GetAbsAngles(), angles );
	}
	else
	{
		// MatrixAngles( m_pGrabbedObject->m_rgflCoordinateFrame, angles );
	}


	matrix3x4_t tempMat;
	matrix3x4_t &parentTransform = EntityToWorldTransform();

	// Moveparent case: transform the abs position into local space
	Vector newPos;
	VectorITransform( m_pGrabbedObject->GetAbsOrigin(), parentTransform, newPos );


	// Moveparent case: transform the abs transform into local space
	matrix3x4_t worldToParent, localMatrix;
	QAngle angNewRotation;
	MatrixInvert( EntityToWorldTransform(), worldToParent );
	ConcatTransforms( worldToParent, m_pGrabbedObject->m_rgflCoordinateFrame, localMatrix );
	MatrixAngles( localMatrix, angNewRotation );*/




	// m_pGrabbedObject->SetLocalAngles( angles );
	// UTIL_SetOrigin( m_pGrabbedObject, localOrigin );

	// Move our step data into the correct space
	// if ( bWasNotParented )
	{
		// Transform step data from world to parent-space
		// TransformStepData_WorldToParent( m_pGrabbedObject );
		// UTIL_WorldToParentSpace( m_pGrabbedObject, step->m_Previous2.vecOrigin, step->m_Previous2.qRotation );
		// UTIL_WorldToParentSpace( m_pGrabbedObject, step->m_Previous.vecOrigin, step->m_Previous.qRotation );
	}
	/*else
	{
		// Transform step data between parent-spaces
		TransformStepData_ParentToParent( pOldParent, this );
	}*/



	// SetTargetPosition( handOrigin + m_grabbedObjectPos, handAngles + m_grabbedObjectAng );
	// SetTargetPosition( handOrigin + finalOrigin, m_grabbedObjectAng );

#endif

	return true;
}


void CVRController::SetTargetPosition( const Vector &target, const QAngle &targetOrientation )
{
	m_shadow.targetPosition = target;
	m_shadow.targetRotation = targetOrientation;

	m_timeToArrive = gpGlobals->frametime;

	CBaseEntity *pAttached = m_pGrabbedObject;
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


//---------------------------------------------------------------
// IMotionEvent functions
//---------------------------------------------------------------
void CVRController::AttachEntity( CBaseEntity *pEntity, IPhysicsObject *pPhys, bool bIsMegaPhysCannon, const Vector &vGrabPosition, bool bUseGrabPosition )
{
}


void CVRController::DetachEntity( bool bClearVelocity )
{
	CBaseEntity *pEntity = m_pGrabbedObject;
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


