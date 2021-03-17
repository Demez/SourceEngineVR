#include "cbase.h"
#include "vr_cl_player.h"
#include "vr_input.h"

#include "playerandobjectenumerator.h"
#include "engine/ivdebugoverlay.h"
#include "c_ai_basenpc.h"
#include "in_buttons.h"
#include "collisionutils.h"
#include "igamemovement.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// How fast to avoid collisions with center of other object, in units per second
#define AVOID_SPEED 2000.0f
extern ConVar cl_forwardspeed;
extern ConVar cl_backspeed;
extern ConVar cl_sidespeed;
extern ConVar vr_scale;

ConVar vr_fov_override( "vr_fov_override", "0", FCVAR_CLIENTDLL );
ConVar vr_disable_eye_ang( "vr_disable_eye_ang", "0", FCVAR_CLIENTDLL );

#if defined( CVRBasePlayer )
	#undef CVRBasePlayer	
#endif

IMPLEMENT_CLIENTCLASS_DT( C_VRBasePlayer, DT_VRBasePlayer, CVRBasePlayer )
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_VRBasePlayer )
END_PREDICTION_DATA()

extern CMoveData* g_pMoveData;

// iojsadiu9fj
ConVar cl_meathook_neck_pivot_ingame_up( "cl_meathook_neck_pivot_ingame_up", "7.0" );
ConVar cl_meathook_neck_pivot_ingame_fwd( "cl_meathook_neck_pivot_ingame_fwd", "3.0" );


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
// TODO: most of these functions use g_VR, and we can't use that for multiplayer
// ------------------------------------------------------------------------------------------------
float C_VRBasePlayer::GetFOV()
{
	if ( g_VR.active )
	{
		if ( vr_fov_override.GetInt() <= 0 )
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


// this is stupid
Vector C_VRBasePlayer::EyePosition()
{
	if ( g_VR.active && !vr_disable_eye_ang.GetBool() )
	{
		VRHostTracker* headset = g_VR.GetTrackerByName("hmd");
		if ( headset == NULL )
			return BaseClass::EyePosition();

		Vector basePos = GetAbsOrigin();
		basePos.z += headset->pos.z;
		return basePos;
	}
	else
	{
		return BaseClass::EyePosition();
	}
}


const QAngle &C_VRBasePlayer::EyeAnglesNoOffset()
{
	if ( g_VR.active && !vr_disable_eye_ang.GetBool() )
	{
		VRHostTracker* headset = g_VR.GetTrackerByName("hmd");
		if ( headset == NULL )
			return BaseClass::EyeAngles();

		return headset->ang;
	}
	else
	{
		return BaseClass::EyeAngles();
	}
}


const QAngle &C_VRBasePlayer::EyeAngles()
{
	// get the headset view angles, fix if needed, and return
	if ( g_VR.active && !vr_disable_eye_ang.GetBool() )
	{
		return m_vrViewAngles;
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
		VRHostTracker* headset = g_VR.GetTrackerByName("hmd");
		if ( headset == NULL )
			return BaseClass::LocalEyeAngles();

		// QAngle eyeAngles( headset->ang );
		// eyeAngles.y += viewOffset;
		// return eyeAngles;
		return headset->ang;
	}
	else
	{
		return BaseClass::LocalEyeAngles();
	}
}


bool C_VRBasePlayer::CreateMove( float flInputSampleTime, CUserCmd *pCmd )
{
	bool ret = BaseClass::CreateMove( flInputSampleTime, pCmd );

	if ( !IsInAVehicle() )
	{
		// PerformClientSideObstacleAvoidance( TICK_INTERVAL, pCmd );
		// PerformClientSideNPCSpeedModifiers( TICK_INTERVAL, pCmd );
	}

	CVRInput* vr_input = (CVRInput*)input;
	vr_input->VRMove( flInputSampleTime, pCmd );

	/*if ( g_VR.active && ret )
	{
		VRTracker* hmd = g_VR.GetTrackerByName("hmd");
		if ( hmd != NULL )
		{
			pCmd->roomScaleMove = hmd->pos * vr_scale.GetFloat();
		}
	}*/

	return ret;
}


void C_VRBasePlayer::ClientThink()
{
	HandleVRMoveData();
}


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

	if ( g_VR.active )
	{
		VRHostTracker* headset = g_VR.GetTrackerByName("hmd");
		if ( headset == NULL )
			return;

		m_vrViewAngles = headset->ang;
		m_vrViewAngles.y += viewOffset;
	}
	else
	{
		m_vrViewAngles = BaseClass::EyeAngles();
	}
}


/*void C_VRBasePlayer::HandleVRMoveData()
{
	BaseClass::HandleVRMoveData();

	m_bInVR = g_pMoveData->vr_active;

	// TODO: delete the trackers
	if ( !m_bInVR )
		return;

	for (int i = 0; i < g_pMoveData->vr_trackers.Count(); i++)
	{
		CmdVRTracker cmdTracker = g_pMoveData->vr_trackers[i];
		C_VRTracker* tracker = GetTracker(cmdTracker.name);
		if ( tracker )
		{
			tracker->UpdateTracker(cmdTracker);
		}
		else
		{
			CreateTracker(cmdTracker);
		}

		if ( V_strcmp(tracker->m_trackerName, "pose_lefthand") == 0 )
		{
			m_BoneFollowerManager.AddBoneFollower( tracker, "ValveBiped.Bip01_L_Hand" );
		}
		
		else if ( V_strcmp(tracker->m_trackerName, "pose_righthand") == 0 )
		{
			m_BoneFollowerManager.AddBoneFollower( tracker, "ValveBiped.Bip01_R_Hand" );
		}
	}
}


C_VRTracker* C_VRBasePlayer::GetTracker( const char* name )
{
	if ( !m_bInVR )
		return NULL;

	for (int i = 0; i < m_VRTrackers.Count(); i++)
	{
		C_VRTracker* tracker = m_VRTrackers[i];
		if ( V_strcmp(tracker->m_trackerName, name) == 0 )
			return tracker;
	}

	return NULL;
}


C_VRTracker* C_VRBasePlayer::CreateTracker( CmdVRTracker& cmdTracker )
{
	if ( !m_bInVR )
		return NULL;

	C_VRTracker* tracker;
	if ( V_strcmp(cmdTracker.name, "pose_lefthand") == 0 || V_strcmp(cmdTracker.name, "pose_righthand") == 0 )
	{
		tracker = (C_VRTracker*)CreateEntityByName("vr_controller");
	}
	else
	{
		tracker = (C_VRTracker*)CreateEntityByName("vr_tracker");
	}

	tracker->InitializeAsClientEntity("", false);
	tracker->UpdateTracker(cmdTracker);

	m_VRTrackers.AddToTail(tracker);
}*/


// ------------------------------------------------------------------------------------------------
// Playermodel controlling
// ------------------------------------------------------------------------------------------------
void C_VRBasePlayer::BuildTransformations( CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed )
{
	BaseClass::BuildTransformations( hdr, pos, q, cameraTransform, boneMask, boneComputed );
	BuildFirstPersonMeathookTransformations( hdr, pos, q, cameraTransform, boneMask, boneComputed, "ValveBiped.Bip01_Head1" );
}


//-----------------------------------------------------------------------------
// Purpose: In meathook mode, fix the bone transforms to hang the user's own
//			avatar under the camera.
//-----------------------------------------------------------------------------
void C_VRBasePlayer::BuildFirstPersonMeathookTransformations( CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed, const char *pchHeadBoneName )
{
#if ENGINE_CSGO || ENGINE_2013 || ENGINE_TF2
	// Handle meathook mode. If we aren't rendering, just use last frame's transforms
	// if ( !InFirstPersonView() )
	// 	return;

	// If we're in third-person view, don't do anything special.
	// If we're in first-person view rendering the main view and using the viewmodel, we shouldn't have even got here!
	// If we're in first-person view rendering the main view(s), meathook and headless.
	// If we're in first-person view rendering shadowbuffers/reflections, don't do anything special either (we could do meathook but with a head?)
	// if ( IsAboutToRagdoll() )
	// {
		// We're re-animating specifically to set up the ragdoll.
		// Meathook can push the player through the floor, which makes the ragdoll fall through the world, which is no good.
		// So do nothing.
		// 	return;
		// }

	// if ( !DrawingMainView() )
	// {
	// 	return;
	// }

	// If we aren't drawing the player anyway, don't mess with the bones. This can happen in Portal.
	// if( !ShouldDrawThisPlayer() )
	// {
	// 	return;
	// }

	m_BoneAccessor.SetWritableBones( BONE_USED_BY_ANYTHING );

	int iHead = LookupBone( pchHeadBoneName );
	if ( iHead == -1 )
	{
		return;
	}

	matrix3x4_t &mHeadTransform = GetBoneForWrite( iHead );

	// "up" on the head bone is along the negative Y axis - not sure why.
	//Vector vHeadTransformUp ( -mHeadTransform[0][1], -mHeadTransform[1][1], -mHeadTransform[2][1] );
	//Vector vHeadTransformFwd ( mHeadTransform[0][1], mHeadTransform[1][1], mHeadTransform[2][1] );
	Vector vHeadTransformTranslation ( mHeadTransform[0][3], mHeadTransform[1][3], mHeadTransform[2][3] );


	// Find out where the player's head (driven by the HMD) is in the world.
	// We can't move this with animations or effects without causing nausea, so we need to move
	// the whole body so that the animated head is in the right place to match the player-controlled head.
	Vector vHeadUp;
	Vector vRealPivotPoint;
	if( m_bInVR )
	{
		VMatrix mWorldFromMideye;
		mWorldFromMideye.SetupMatrixOrgAngles( EyePosition(), EyeAngles() );

		// What we do here is:
		// * Take the required eye pos+orn - the actual pose the player is controlling with the HMD.
		// * Go downwards in that space by cl_meathook_neck_pivot_ingame_* - this is now the neck-pivot in the game world of where the player is actually looking.
		// * Now place the body of the animated character so that the head bone is at that position.
		// The head bone is the neck pivot point of the in-game character.

		Vector vRealMidEyePos = mWorldFromMideye.GetTranslation();
		vRealPivotPoint = vRealMidEyePos - ( mWorldFromMideye.GetUp() * cl_meathook_neck_pivot_ingame_up.GetFloat() ) - ( mWorldFromMideye.GetForward() * cl_meathook_neck_pivot_ingame_fwd.GetFloat() );
	}
	else
	{
		// figure out where to put the body from the aim angles
		Vector vForward, vRight, vUp;
		AngleVectors( GetAbsAngles(), &vForward, &vRight, &vUp );

		vRealPivotPoint = GetAbsOrigin() - ( vUp * cl_meathook_neck_pivot_ingame_up.GetFloat() ) - ( vForward * cl_meathook_neck_pivot_ingame_fwd.GetFloat() );		
	}

	Vector vDeltaToAdd = vRealPivotPoint - vHeadTransformTranslation;


	// Now add this offset to the entire skeleton.
	for (int i = 0; i < hdr->numbones(); i++)
	{
		// Only update bones reference by the bone mask.
		if ( !( hdr->boneFlags( i ) & boneMask ) )
		{
			continue;
		}
		matrix3x4_t& bone = GetBoneForWrite( i );
		Vector vBonePos;
		MatrixGetTranslation ( bone, vBonePos );
		vBonePos += vDeltaToAdd;
		MatrixSetTranslation ( vBonePos, bone );
	}

	// Then scale the head to zero, but leave its position - forms a "neck stub".
	// This prevents us rendering junk all over the screen, e.g. inside of mouth, etc.
	MatrixScaleByZero( mHeadTransform );

	// TODO: right now we nuke the hats by shrinking them to nothing,
	// but it feels like we should do something more sensible.
	// For example, for one sniper taunt he takes his hat off and waves it - would be nice to see it then.
	int iHelm = LookupBone( "prp_helmet" );
	if ( iHelm != -1 )
	{
		// Scale the helmet.
		matrix3x4_t  &transformhelmet = GetBoneForWrite( iHelm );
		MatrixScaleByZero( transformhelmet );
	}

	iHelm = LookupBone( "prp_hat" );
	if ( iHelm != -1 )
	{
		matrix3x4_t  &transformhelmet = GetBoneForWrite( iHelm );
		MatrixScaleByZero( transformhelmet );
	}
#endif
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







