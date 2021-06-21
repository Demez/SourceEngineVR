#include "cbase.h"

#ifdef CLIENT_DLL
#include "vr_cl_player.h"
#else
#include "vr_sv_player.h"
#endif

#include "vr_gamemovement.h"
#include "vr_player_shared.h"
#include "vr_tracker.h"
#include "vr_controller.h"

extern CMoveData* g_pMoveData;


extern ConVar vr_lefthand;


IMPLEMENT_NETWORKCLASS( CVRBasePlayerShared, DT_VRBasePlayerShared )

#ifdef CLIENT_DLL

BEGIN_PREDICTION_DATA( CVRBasePlayerShared )
END_PREDICTION_DATA()

#else

BEGIN_DATADESC( CVRBasePlayerShared )
END_DATADESC()

#endif


BEGIN_NETWORK_TABLE( CVRBasePlayerShared, DT_VRBasePlayerShared )
#ifdef CLIENT_DLL
#else
#endif
END_NETWORK_TABLE()


void CVRBasePlayerShared::Spawn()
{
	BaseClass::Spawn();
}


#ifdef GAME_DLL
bool CVRBasePlayerShared::CreateVPhysics()
{
	return BaseClass::CreateVPhysics();
}


void CVRBasePlayerShared::InitBoneFollowers()
{
}


#endif


void CVRBasePlayerShared::OnVREnabled()
{
}


void CVRBasePlayerShared::OnVRDisabled()
{
	m_VRTrackers.PurgeAndDeleteElements();
}


void CVRBasePlayerShared::PreThink()
{
	HandleVRMoveData();
}


Vector CVRBasePlayerShared::Weapon_ShootPosition()
{
	if ( !m_bInVR || !GetActiveWeapon() || !GetWeaponHand() )
		return BaseClass::Weapon_ShootPosition();

	Vector muzzlePos;
	QAngle muzzleAng;

	/*if ( GetActiveWeapon()->GetAttachment( GetActiveWeapon()->LookupAttachment( "muzzle" ), muzzlePos, muzzleAng ))
	{
		return muzzlePos;
	}*/

	return GetWeaponHand()->GetAbsOrigin();
}


QAngle CVRBasePlayerShared::GetWeaponShootAng()
{
	if ( !m_bInVR /*|| !GetActiveWeapon()*/ )
		return EyeAngles();

	Vector muzzlePos;
	QAngle muzzleAng;

	/*if ( GetActiveWeapon()->GetAttachment( GetActiveWeapon()->LookupAttachment( "muzzle" ), muzzlePos, muzzleAng ))
	{
		return muzzleAng;
	}
	else*/ if ( GetWeaponHand() )
	{
		return GetWeaponHand()->GetPointAng();
	}

	return EyeAngles();
}

Vector CVRBasePlayerShared::GetWeaponShootDir()
{
	if ( GetWeaponHand() )
		return GetWeaponHand()->GetPointDir();

	Vector forward;
	AngleVectors( EyeAngles(), &forward );
	return forward;
}

Vector CVRBasePlayerShared::GetAutoaimVector( float flScale )
{
	if ( !m_bInVR )
		return BaseClass::GetAutoaimVector( flScale );

	return GetWeaponShootDir();
}


#ifdef GAME_DLL
void CVRBasePlayerShared::GetAutoaimVector( autoaim_params_t &params )
{
	if ( !m_bInVR )
	{
		BaseClass::GetAutoaimVector( params );
		return;
	}

	params.m_bAutoAimAssisting = false;
	params.m_bOnTargetNatural = false;
	params.m_hAutoAimEntity.Set( NULL );
	params.m_vecAutoAimPoint = vec3_invalid;

	// if ( ( ShouldAutoaim() == false ) || ( params.m_fScale == AUTOAIM_SCALE_DIRECT_ONLY ) )
	{
		params.m_vecAutoAimDir = GetWeaponShootDir();
		return;
	}

	/*Vector vecShootPosition = Weapon_ShootPosition();
	// DEMEZ TODO: Needs to be changed for autoaim
	QAngle angles = AutoaimDeflection( vecShootPosition, params );
	
	// update ontarget if changed
	if ( !g_pGameRules->AllowAutoTargetCrosshair() )
	{
		m_fOnTarget = false;
	}

	if (angles.x > 180)
		angles.x -= 360;
	if (angles.x < -180)
		angles.x += 360;
	if (angles.y > 180)
		angles.y -= 360;
	if (angles.y < -180)
		angles.y += 360;

	if (angles.x > 25)
		angles.x = 25;
	if (angles.x < -25)
		angles.x = -25;
	if (angles.y > 12)
		angles.y = 12;
	if (angles.y < -12)
		angles.y = -12;

	m_vecAutoAim.Init( 0.0f, 0.0f, 0.0f );
	Vector	forward;

	if ( ( IsInAVehicle() && g_pGameRules->GetAutoAimMode() == AUTOAIM_ON_CONSOLE ) )
	{
		m_vecAutoAim = angles;
		AngleVectors( EyeAngles() + m_vecAutoAim, &forward );
	}
	else
	{
		// always use non-sticky autoaim
		m_vecAutoAim = angles * 0.9f;
		AngleVectors( EyeAngles() + m_Local.m_vecPunchAngle + m_vecAutoAim, &forward );
	}
	params.m_vecAutoAimDir = forward;*/
}
#endif


const Vector CVRBasePlayerShared::GetPlayerMins( void )
{
	if ( !m_bInVR || !GetHeadset() )
		return BaseClass::GetPlayerMins();

	CalculatePlayerBBox();
	return m_minSize;
}


const Vector CVRBasePlayerShared::GetPlayerMaxs( void )
{	
	if ( !m_bInVR || !GetHeadset() )
	 	return BaseClass::GetPlayerMaxs();

	CalculatePlayerBBox();
	return m_maxSize;
}


void CVRBasePlayerShared::CalculatePlayerBBox()
{
	// TODO: actually calculate it based on headset height
	CVRTracker* hmd = GetHeadset();

	hmd->m_pos.z;

	if ( GetFlags() & FL_DUCKING )
	{
		m_maxSize = VEC_DUCK_HULL_MAX;
		m_minSize = VEC_DUCK_HULL_MIN;
	}
	else
	{
		m_maxSize = VEC_HULL_MAX;
		m_minSize = VEC_HULL_MIN;
	}
}


Vector CVRBasePlayerShared::EyePosition()
{
	if ( m_bInVR )
	{
		// CVRTracker* hmd = GetHeadset();
		// if ( hmd == NULL )
		// 	return BaseClass::EyePosition();

		Vector basePos = GetAbsOrigin();
		basePos += m_viewOriginOffset;
		// basePos.z += hmd->m_pos.z;

		return basePos;
	}
	else
	{
		return BaseClass::EyePosition();
	}
}


const QAngle &CVRBasePlayerShared::EyeAngles()
{
	if ( m_bInVR )
	{
		return m_vrViewAngles;
	}
	else
	{
		return BaseClass::EyeAngles();
	}
}


void CVRBasePlayerShared::HandleVRMoveData()
{
	bool vr = m_bInVR;

	m_bInVR = GetVRMoveData()->vr_active;

	if ( vr != m_bInVR )
	{
		if ( m_bInVR )
		{
			OnVREnabled();
		}
		else
		{
			OnVRDisabled();
		}
	}

	// TODO: delete the trackers
	if ( !m_bInVR )
		return;

	UpdateTrackers();
}


void CVRBasePlayerShared::UpdateTrackers()
{
	for (int i = 0; i < GetVRMoveData()->vr_trackers.Count(); i++)
	{
		CmdVRTracker cmdTracker = GetVRMoveData()->vr_trackers[i];

		// not sure how this is happening yet
		if ( cmdTracker.name == NULL )
			continue;

		CVRTracker* tracker = GetTracker( cmdTracker.index );
		if ( tracker )
		{
			if ( !tracker->m_bInit )
			{
				tracker->InitTracker(cmdTracker, this);
			}
			else
			{
				tracker->UpdateTracker(cmdTracker);
			}
		}
		else
		{
			tracker = CreateTracker(cmdTracker);
		}
	}
}


CVRTracker* CVRBasePlayerShared::GetTracker( EVRTracker type )
{
	if ( !m_bInVR )
		return NULL;

	for (int i = 0; i < m_VRTrackers.Count(); i++)
	{
		CVRTracker* tracker = m_VRTrackers[i];

		if ( tracker == NULL )
			continue;

		if ( tracker->m_type == type )
			return tracker;
	}

	return NULL;
}


CVRTracker* CVRBasePlayerShared::GetTracker( short index )
{
	return GetTracker( GetTrackerEnum(index) );
}


CVRController* CVRBasePlayerShared::GetWeaponHand()
{
	// TODO: this could vary for everyone, hmm
	// if ( vr_lefthand.GetBool() )
	//	return GetLeftHand();

	return GetRightHand();
}


CVRTracker* CVRBasePlayerShared::CreateTracker( CmdVRTracker& cmdTracker )
{
	if ( !m_bInVR )
		return NULL;

	CVRTracker* tracker;
	EVRTracker type = GetTrackerEnum(cmdTracker.index);
	
	if ( type == EVRTracker::LHAND || type == EVRTracker::RHAND )
	{
		tracker = new CVRController;
	}
	else
	{
		tracker = new CVRTracker;
	}

	if ( tracker == NULL )
		return NULL;

	tracker->Spawn();
	tracker->InitTracker( cmdTracker, this );
	m_VRTrackers.AddToTail( tracker );
	return tracker;
}


CBaseEntity* CVRBasePlayerShared::FindUseEntity()
{
	// CVRController will handle this
	if ( !m_bInVR || (!GetLeftHand() && !GetRightHand()) )
		return BaseClass::FindUseEntity();

	return NULL;
}

