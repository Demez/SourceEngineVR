#include "cbase.h"
#include "vr_cl_player.h"

#include "playerandobjectenumerator.h"
#include "engine/ivdebugoverlay.h"
#include "c_ai_basenpc.h"
#include "in_buttons.h"
#include "collisionutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// How fast to avoid collisions with center of other object, in units per second
#define AVOID_SPEED 2000.0f
extern ConVar cl_forwardspeed;
extern ConVar cl_backspeed;
extern ConVar cl_sidespeed;

ConVar vr_fov_override( "vr_fov_override", "0", FCVAR_CLIENTDLL );
ConVar vr_disable_eye_ang( "vr_disable_eye_ang", "0", FCVAR_CLIENTDLL );

#if defined( CVRBasePlayer )
	#undef CVRBasePlayer	
#endif

IMPLEMENT_CLIENTCLASS_DT( C_VRBasePlayer, DT_VRBasePlayer, CVRBasePlayer )
	// RecvPropDataTable( RECVINFO_DT(m_HL2Local),0, &REFERENCE_RECV_TABLE(DT_HL2Local) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_VRBasePlayer )
	// DEFINE_PRED_TYPEDESCRIPTION( m_HL2Local, C_HL2PlayerLocalData ),
END_PREDICTION_DATA()


//-----------------------------------------------------------------------------
// GENERAL NOTES AND THOUGHTS:
//-----------------------------------------------------------------------------
// thinking of being able to put your weapons on holsters on your body
// or having a simple inventory
// except either would probably be changed with anything using this base class


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
C_VRBasePlayer::C_VRBasePlayer()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : updateType - 
//-----------------------------------------------------------------------------
void C_VRBasePlayer::OnDataChanged( DataUpdateType_t updateType )
{
	// Make sure we're thinking
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}

	BaseClass::OnDataChanged( updateType );
}


// ------------------------------------------------------------------------------------------------
// Modified Functions if in VR
// ------------------------------------------------------------------------------------------------
float C_VRBasePlayer::GetFOV()
{
	if ( g_VR.active )
	{
		if ( !vr_fov_override.GetInt() > 0 )
		{
			VRViewParams viewParams = g_VR.GetViewParams();
			return viewParams.horizontalFOVLeft;
		}
		else
		{
			return vr_fov_override.GetInt();
		}
	}
	else
	{
		return BaseClass::GetFOV();
	}
}


const QAngle &C_VRBasePlayer::EyeAngles()
{
	// get the headset view angles, fix if needed, and return
	if ( g_VR.active && !vr_disable_eye_ang.GetBool() )
	{
		VRTracker* headset = g_VR.GetTrackerByName("hmd");
		if ( headset == NULL )
			return BaseClass::EyeAngles();

		QAngle eyeAngles( headset->ang );
		eyeAngles.y += viewOffset;
		return eyeAngles;
	}
	else
	{
		return BaseClass::EyeAngles();
	}
}


const QAngle &C_VRBasePlayer::LocalEyeAngles()
{
	// get the headset view angles, fix if needed, and return
	if ( g_VR.active && !vr_disable_eye_ang.GetBool() )
	{
		VRTracker* headset = g_VR.GetTrackerByName("hmd");
		if ( headset == NULL )
			return BaseClass::LocalEyeAngles();

		QAngle eyeAngles( headset->ang );
		eyeAngles.y += viewOffset;
		return eyeAngles;
	}
	else
	{
		return BaseClass::LocalEyeAngles();
	}
}



void C_VRBasePlayer::PreThink( void )
{
	BaseClass::PreThink();
}

void C_VRBasePlayer::PostThink( void )
{
	BaseClass::PostThink();

	if ( g_VR.active && IsAlive() )
	{
		VRTracker* hmd = g_VR.GetTrackerByName("hmd");


		Msg("pog\n");
	}
}


// ------------------------------------------------------------------------------------------------
// User Messages
// ------------------------------------------------------------------------------------------------



// ------------------------------------------------------------------------------------------------
// New VR Functions
// ------------------------------------------------------------------------------------------------
void C_VRBasePlayer::SetViewRotateOffset( float offset )
{
	viewOffset = offset;
	CorrectViewRotateOffset();
}


void C_VRBasePlayer::AddViewRotateOffset( float offset )
{
	viewOffset += offset;
	CorrectViewRotateOffset();
}


void C_VRBasePlayer::CorrectViewRotateOffset()
{
	if ( viewOffset > 360.0 )
		viewOffset -= 360.0;
	else if ( viewOffset < -360.0 )
		viewOffset += 360.0;
}

// ------------------------------------------------------------------------------------------------
// Untouched Functions
// ------------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Hack to zero out player's pitch, use value from poseparameter instead
// Input  : flags - 
// Output : int
//-----------------------------------------------------------------------------
/*int C_VRBasePlayer::DrawModel( int flags )
{
	// Not pitch for player
	QAngle saveAngles = GetLocalAngles();

	QAngle useAngles = saveAngles;
	useAngles[ PITCH ] = 0.0f;

	SetLocalAngles( useAngles );

	int iret = BaseClass::DrawModel( flags );

	SetLocalAngles( saveAngles );

	return iret;
}*/

//-----------------------------------------------------------------------------
// Purpose: Helper to remove from ladder
//-----------------------------------------------------------------------------
void C_VRBasePlayer::ExitLadder()
{
	if ( MOVETYPE_LADDER != GetMoveType() )
		return;
	
	SetMoveType( MOVETYPE_WALK );
	SetMoveCollide( MOVECOLLIDE_DEFAULT );

	// Remove from ladder
	// m_HL2Local.m_hLadder = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Determines if a player can be safely moved towards a point
// Input:   pos - position to test move to, fVertDist - how far to trace downwards to see if the player would fall,
//			radius - how close the player can be to the object, objPos - position of the object to avoid,
//			objDir - direction the object is travelling
//-----------------------------------------------------------------------------
bool C_VRBasePlayer::TestMove( const Vector &pos, float fVertDist, float radius, const Vector &objPos, const Vector &objDir )
{
	trace_t trUp;
	trace_t trOver;
	trace_t trDown;
	float flHit1, flHit2;
	
	UTIL_TraceHull( GetAbsOrigin(), pos, GetPlayerMins(), GetPlayerMaxs(), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &trOver );
	if ( trOver.fraction < 1.0f )
	{
		// check if the endpos intersects with the direction the object is travelling.  if it doesn't, this is a good direction to move.
		if ( objDir.IsZero() ||
			( IntersectInfiniteRayWithSphere( objPos, objDir, trOver.endpos, radius, &flHit1, &flHit2 ) && 
			( ( flHit1 >= 0.0f ) || ( flHit2 >= 0.0f ) ) )
			)
		{
			// our first trace failed, so see if we can go farther if we step up.

			// trace up to see if we have enough room.
			UTIL_TraceHull( GetAbsOrigin(), GetAbsOrigin() + Vector( 0, 0, m_Local.m_flStepSize ), 
				GetPlayerMins(), GetPlayerMaxs(), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &trUp );

			// do a trace from the stepped up height
			UTIL_TraceHull( trUp.endpos, pos + Vector( 0, 0, trUp.endpos.z - trUp.startpos.z ), 
				GetPlayerMins(), GetPlayerMaxs(), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &trOver );

			if ( trOver.fraction < 1.0f )
			{
				// check if the endpos intersects with the direction the object is travelling.  if it doesn't, this is a good direction to move.
				if ( objDir.IsZero() ||
					IntersectInfiniteRayWithSphere( objPos, objDir, trOver.endpos, radius, &flHit1, &flHit2 ) && ( ( flHit1 >= 0.0f ) || ( flHit2 >= 0.0f ) ) )
				{
					return false;
				}
			}
		}
	}

	// trace down to see if this position is on the ground
	UTIL_TraceLine( trOver.endpos, trOver.endpos - Vector( 0, 0, fVertDist ), 
		MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &trDown );

	if ( trDown.fraction == 1.0f ) 
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Client-side obstacle avoidance
//-----------------------------------------------------------------------------
void C_VRBasePlayer::PerformClientSideObstacleAvoidance( float flFrameTime, CUserCmd *pCmd )
{
	// Don't avoid if noclipping or in movetype none
	switch ( GetMoveType() )
	{
	case MOVETYPE_NOCLIP:
	case MOVETYPE_NONE:
	case MOVETYPE_OBSERVER:
		return;
	default:
		break;
	}

	// Try to steer away from any objects/players we might interpenetrate
	Vector size = WorldAlignSize();

	float radius = 0.7f * sqrt( size.x * size.x + size.y * size.y );
	float curspeed = GetLocalVelocity().Length2D();

	//int slot = 1;
	//engine->Con_NPrintf( slot++, "speed %f\n", curspeed );
	//engine->Con_NPrintf( slot++, "radius %f\n", radius );

	// If running, use a larger radius
	float factor = 1.0f;

	if ( curspeed > 150.0f )
	{
		curspeed = MIN( 2048.0f, curspeed );
		factor = ( 1.0f + ( curspeed - 150.0f ) / 150.0f );

		//engine->Con_NPrintf( slot++, "scaleup (%f) to radius %f\n", factor, radius * factor );

		radius = radius * factor;
	}

	Vector currentdir;
	Vector rightdir;

	QAngle vAngles = pCmd->viewangles;
	vAngles.x = 0;

	AngleVectors( vAngles, &currentdir, &rightdir, NULL );
		
	bool istryingtomove = false;
	bool ismovingforward = false;
	if ( fabs( pCmd->forwardmove ) > 0.0f || 
		fabs( pCmd->sidemove ) > 0.0f )
	{
		istryingtomove = true;
		if ( pCmd->forwardmove > 1.0f )
		{
			ismovingforward = true;
		}
	}

	if ( istryingtomove == true )
		 radius *= 1.3f;

	CPlayerAndObjectEnumerator avoid( radius );
	partition->EnumerateElementsInSphere( PARTITION_CLIENT_SOLID_EDICTS, GetAbsOrigin(), radius, false, &avoid );

	// Okay, decide how to avoid if there's anything close by
	int c = avoid.GetObjectCount();
	if ( c <= 0 )
		return;

	//engine->Con_NPrintf( slot++, "moving %s forward %s\n", istryingtomove ? "true" : "false", ismovingforward ? "true" : "false"  );

	float adjustforwardmove = 0.0f;
	float adjustsidemove	= 0.0f;

	for ( int i = 0; i < c; i++ )
	{
		C_AI_BaseNPC *obj = dynamic_cast< C_AI_BaseNPC *>(avoid.GetObject( i ));

		if( !obj )
			continue;

		Vector vecToObject = obj->GetAbsOrigin() - GetAbsOrigin();

		float flDist = vecToObject.Length2D();
		
		// Figure out a 2D radius for the object
		Vector vecWorldMins, vecWorldMaxs;
		obj->CollisionProp()->WorldSpaceAABB( &vecWorldMins, &vecWorldMaxs );
		Vector objSize = vecWorldMaxs - vecWorldMins;

		float objectradius = 0.5f * sqrt( objSize.x * objSize.x + objSize.y * objSize.y );

		//Don't run this code if the NPC is not moving UNLESS we are in stuck inside of them.
		if ( !obj->IsMoving() && flDist > objectradius )
			  continue;

		if ( flDist > objectradius && obj->IsEffectActive( EF_NODRAW ) )
		{
			obj->RemoveEffects( EF_NODRAW );
		}

		Vector vecNPCVelocity;
		obj->EstimateAbsVelocity( vecNPCVelocity );
		float flNPCSpeed = VectorNormalize( vecNPCVelocity );

		Vector vPlayerVel = GetAbsVelocity();
		VectorNormalize( vPlayerVel );

		float flHit1, flHit2;
		Vector vRayDir = vecToObject;
		VectorNormalize( vRayDir );

		float flVelProduct = DotProduct( vecNPCVelocity, vPlayerVel );
		float flDirProduct = DotProduct( vRayDir, vPlayerVel );

		if ( !IntersectInfiniteRayWithSphere(
				GetAbsOrigin(),
				vRayDir,
				obj->GetAbsOrigin(),
				radius,
				&flHit1,
				&flHit2 ) )
			continue;

        Vector dirToObject = -vecToObject;
		VectorNormalize( dirToObject );

		float fwd = 0;
		float rt = 0;

		float sidescale = 2.0f;
		float forwardscale = 1.0f;
		bool foundResult = false;

		Vector vMoveDir = vecNPCVelocity;
		if ( flNPCSpeed > 0.001f )
		{
			// This NPC is moving. First try deflecting the player left or right relative to the NPC's velocity.
			// Start with whatever side they're on relative to the NPC's velocity.
			Vector vecNPCTrajectoryRight = CrossProduct( vecNPCVelocity, Vector( 0, 0, 1) );
			int iDirection = ( vecNPCTrajectoryRight.Dot( dirToObject ) > 0 ) ? 1 : -1;
			for ( int nTries = 0; nTries < 2; nTries++ )
			{
				Vector vecTryMove = vecNPCTrajectoryRight * iDirection;
				VectorNormalize( vecTryMove );
				
				Vector vTestPosition = GetAbsOrigin() + vecTryMove * radius * 2;

				if ( TestMove( vTestPosition, size.z * 2, radius * 2, obj->GetAbsOrigin(), vMoveDir ) )
				{
					fwd = currentdir.Dot( vecTryMove );
					rt = rightdir.Dot( vecTryMove );
					
					//Msg( "PUSH DEFLECT fwd=%f, rt=%f\n", fwd, rt );
					
					foundResult = true;
					break;
				}
				else
				{
					// Try the other direction.
					iDirection *= -1;
				}
			}
		}
		else
		{
			// the object isn't moving, so try moving opposite the way it's facing
			Vector vecNPCForward;
			obj->GetVectors( &vecNPCForward, NULL, NULL );
			
			Vector vTestPosition = GetAbsOrigin() - vecNPCForward * radius * 2;
			if ( TestMove( vTestPosition, size.z * 2, radius * 2, obj->GetAbsOrigin(), vMoveDir ) )
			{
				fwd = currentdir.Dot( -vecNPCForward );
				rt = rightdir.Dot( -vecNPCForward );

				if ( flDist < objectradius )
				{
					obj->AddEffects( EF_NODRAW );
				}

				//Msg( "PUSH AWAY FACE fwd=%f, rt=%f\n", fwd, rt );

				foundResult = true;
			}
		}

		if ( !foundResult )
		{
			// test if we can move in the direction the object is moving
			Vector vTestPosition = GetAbsOrigin() + vMoveDir * radius * 2;
			if ( TestMove( vTestPosition, size.z * 2, radius * 2, obj->GetAbsOrigin(), vMoveDir ) )
			{
				fwd = currentdir.Dot( vMoveDir );
				rt = rightdir.Dot( vMoveDir );

				if ( flDist < objectradius )
				{
					obj->AddEffects( EF_NODRAW );
				}

				//Msg( "PUSH ALONG fwd=%f, rt=%f\n", fwd, rt );

				foundResult = true;
			}
			else
			{
				// try moving directly away from the object
				Vector vTestPosition = GetAbsOrigin() - dirToObject * radius * 2;
				if ( TestMove( vTestPosition, size.z * 2, radius * 2, obj->GetAbsOrigin(), vMoveDir ) )
				{
					fwd = currentdir.Dot( -dirToObject );
					rt = rightdir.Dot( -dirToObject );
					foundResult = true;

					//Msg( "PUSH AWAY fwd=%f, rt=%f\n", fwd, rt );
				}
			}
		}

		if ( !foundResult )
		{
			// test if we can move through the object
			Vector vTestPosition = GetAbsOrigin() - vMoveDir * radius * 2;
			fwd = currentdir.Dot( -vMoveDir );
			rt = rightdir.Dot( -vMoveDir );

			if ( flDist < objectradius )
			{
				obj->AddEffects( EF_NODRAW );
			}

			//Msg( "PUSH THROUGH fwd=%f, rt=%f\n", fwd, rt );

			foundResult = true;
		}

		// If running, then do a lot more sideways veer since we're not going to do anything to
		//  forward velocity
		if ( istryingtomove )
		{
			sidescale = 6.0f;
		}

		if ( flVelProduct > 0.0f && flDirProduct > 0.0f )
		{
			sidescale = 0.1f;
		}

		float force = 1.0f;
		float forward = forwardscale * fwd * force * AVOID_SPEED;
		float side = sidescale * rt * force * AVOID_SPEED;

		adjustforwardmove	+= forward;
		adjustsidemove		+= side;
	}

	pCmd->forwardmove	+= adjustforwardmove;
	pCmd->sidemove		+= adjustsidemove;
	
	// Clamp the move to within legal limits, preserving direction. This is a little
	// complicated because we have different limits for forward, back, and side

	//Msg( "PRECLAMP: forwardmove=%f, sidemove=%f\n", pCmd->forwardmove, pCmd->sidemove );

	float flForwardScale = 1.0f;
	if ( pCmd->forwardmove > fabs( cl_forwardspeed.GetFloat() ) )
	{
		flForwardScale = fabs( cl_forwardspeed.GetFloat() ) / pCmd->forwardmove;
	}
	else if ( pCmd->forwardmove < -fabs( cl_backspeed.GetFloat() ) )
	{
		flForwardScale = fabs( cl_backspeed.GetFloat() ) / fabs( pCmd->forwardmove );
	}
	
	float flSideScale = 1.0f;
	if ( fabs( pCmd->sidemove ) > fabs( cl_sidespeed.GetFloat() ) )
	{
		flSideScale = fabs( cl_sidespeed.GetFloat() ) / fabs( pCmd->sidemove );
	}
	
	float flScale = MIN( flForwardScale, flSideScale );
	pCmd->forwardmove *= flScale;
	pCmd->sidemove *= flScale;

	//Msg( "POSTCLAMP: forwardmove=%f, sidemove=%f\n", pCmd->forwardmove, pCmd->sidemove );
}


//-----------------------------------------------------------------------------
// Purpose: Input handling
//-----------------------------------------------------------------------------
bool C_VRBasePlayer::CreateMove( float flInputSampleTime, CUserCmd *pCmd )
{
	bool bResult = BaseClass::CreateMove( flInputSampleTime, pCmd );

	if ( !IsInAVehicle() )
	{
		PerformClientSideObstacleAvoidance( TICK_INTERVAL, pCmd );
		// PerformClientSideNPCSpeedModifiers( TICK_INTERVAL, pCmd );
	}

	return bResult;
}





