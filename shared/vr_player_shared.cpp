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

#include "hl_gamemovement.h"
#include "tier1/fmtstr.h"
#include "coordsize.h"

extern CMoveData* g_pMoveData;


extern ConVar vr_lefthand;


IMPLEMENT_NETWORKCLASS( CVRBasePlayerShared, DT_VRBasePlayerShared )

#ifdef CLIENT_DLL

// works fine with only m_vrViewRotation in there, idk if anything else here is needed
BEGIN_PREDICTION_DATA( CVRBasePlayerShared )
	DEFINE_PRED_FIELD( m_bInVR, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_vrViewRotation, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_viewOriginOffset, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()

#else

BEGIN_DATADESC( CVRBasePlayerShared )
	DEFINE_FIELD( m_bInVR, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_vrViewRotation, FIELD_FLOAT ),
	DEFINE_FIELD( m_viewOriginOffset, FIELD_VECTOR ),
END_DATADESC()

#endif


/*BEGIN_NETWORK_TABLE_NOBASE( NetworkVRTracker, DT_VRTracker )
#ifdef CLIENT_DLL
	RecvPropInt( RECVINFO(index) ),
	RecvPropVector( RECVINFO(pos) ),
	RecvPropVector( RECVINFO(ang) ),
#else
	SendPropInt( SENDINFO(index), MAX_VR_TRACKERS, SPROP_UNSIGNED ),
	SendPropVector	(SENDINFO(pos), -1, SPROP_COORD|SPROP_CHANGES_OFTEN ),
	SendPropQAngles	(SENDINFO(ang), 13, SPROP_COORD|SPROP_CHANGES_OFTEN ),
#endif
END_NETWORK_TABLE()*/


BEGIN_NETWORK_TABLE( CVRBasePlayerShared, DT_VRBasePlayerShared )
#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO(m_bInVR) ),
	RecvPropFloat( RECVINFO(m_vrViewRotation) ),
	RecvPropVector( RECVINFO(m_viewOriginOffset) ),

	RecvPropVector( RECVINFO(m_vrOriginOffset) ),

	RecvPropArray3( RECVINFO_ARRAY(m_vrtrackers_index), RecvPropInt( RECVINFO( m_vrtrackers_index[0] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_vrtrackers_pos), RecvPropVector( RECVINFO( m_vrtrackers_pos[0] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_vrtrackers_ang), RecvPropQAngles( RECVINFO( m_vrtrackers_ang[0] ) ) ),

	// what the fuck
	// RecvPropArray( RecvPropDataTable( "vrtracker", 0, 0, &REFERENCE_RECV_TABLE( DT_VRTracker ) ), m_VRCmdTrackers ),
#else
	SendPropBool( SENDINFO(m_bInVR) ),
	SendPropFloat( SENDINFO(m_vrViewRotation), 0, SPROP_NOSCALE ),
	//SendPropVector( SENDINFO(m_viewOriginOffset), -1, SPROP_NOSCALE|SPROP_COORD|SPROP_CHANGES_OFTEN ),
	SendPropVector( SENDINFO(m_viewOriginOffset), -1, SPROP_COORD ),

	SendPropVector( SENDINFO(m_vrOriginOffset), -1, SPROP_COORD|SPROP_CHANGES_OFTEN ),

	SendPropArray3( SENDINFO_ARRAY3(m_vrtrackers_index), SendPropInt( SENDINFO_ARRAY(m_vrtrackers_index), MAX_VR_TRACKERS, 0 ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_vrtrackers_pos), SendPropVector( SENDINFO_ARRAY(m_vrtrackers_pos) ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_vrtrackers_ang), SendPropQAngles( SENDINFO_ARRAY(m_vrtrackers_ang), MAX_VR_TRACKERS, SPROP_COORD ) ),

	// SendPropArray( SendPropDataTable( "vrtracker", 0, &REFERENCE_SEND_TABLE( DT_VRTracker ) ), m_VRCmdTrackers ),

#endif
END_NETWORK_TABLE()


void CVRBasePlayerShared::Spawn()
{
	BaseClass::Spawn();

	m_bInVR = false;
	m_vrViewRotation = 0.0;
	m_vrViewRotationGame = 0.0;
	m_viewOriginOffset.Init();

	ClearCmdTrackers();
}


#ifdef GAME_DLL
bool CVRBasePlayerShared::CreateVPhysics()
{
	return BaseClass::CreateVPhysics();
}
#endif


void CVRBasePlayerShared::OnVREnabled()
{
}


void CVRBasePlayerShared::OnVRDisabled()
{
	m_VRTrackers.PurgeAndDeleteElements();
	ClearCmdTrackers();
}


void CVRBasePlayerShared::ClearCmdTrackers()
{
	for (int i = 0; i < MAX_VR_TRACKERS; i++)
	{
		m_vrtrackers_index.Set(i, -1);
		m_vrtrackers_pos.Set(i, vec3_origin);
		m_vrtrackers_ang.Set(i, vec3_angle);
	}
}


void CVRBasePlayerShared::PreThink()
{
}


void CVRBasePlayerShared::PostThink()
{
	BaseClass::PostThink();
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

	return GetWeaponHand()->GetPointPos();
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
	// CVRTracker* hmd = GetHeadset();

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


float CVRBasePlayerShared::VRHeightOffset()
{
	return -(VEC_VIEW.z - GetViewOffset().z);
}


Vector CVRBasePlayerShared::EyePosition()
{
	if ( m_bInVR )
	{
		Vector basePos = GetAbsOrigin();
		basePos += m_viewOriginOffset;
		basePos.z += VRHeightOffset();

		return basePos;
	}
	else
	{
		return BaseClass::EyePosition();
	}
}

#ifdef CLIENT_DLL
extern ConVar vr_mouse_look;
#endif

const QAngle &CVRBasePlayerShared::EyeAngles()
{
	if ( m_bInVR 
#ifdef CLIENT_DLL
		&& !vr_mouse_look.GetBool()
#endif
		)
	{
		CVRTracker* headset = GetHeadset();
		if ( headset == NULL )
			return BaseClass::EyeAngles();

		return headset->GetAbsAngles();
	}
	else
	{
		return BaseClass::EyeAngles();
	}
}


void CVRBasePlayerShared::AddViewRotation( float offset )
{
	m_vrViewRotation += offset;
	CorrectViewRotateOffset();
}

void CVRBasePlayerShared::CorrectViewRotateOffset()
{
	if ( m_vrViewRotation > 360.0 )
		m_vrViewRotation -= 360.0;
	else if ( m_vrViewRotation < -360.0 )
		m_vrViewRotation += 360.0;
}


// server and local player only
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

	if ( !m_bInVR )
		return;

#ifdef GAME_DLL
	m_vrViewRotation = GetVRMoveData()->vr_viewRotation + m_vrViewRotationGame;
	CorrectViewRotateOffset();

	// VR TODO: can this be done on the client somewhere so it can be smoother?
	Vector prevHmdPos(0, 0, 0);
	CVRTracker* hmd = GetHeadset();

	if ( hmd )
	{
		prevHmdPos = hmd->m_pos;
	}

	UpdateTrackers();

	if ( hmd )
	{
		VectorYawRotate( hmd->m_pos - prevHmdPos, m_vrViewRotation, m_vrOriginOffset.GetForModify() );
	}
	else
	{
		m_vrOriginOffset.Init();
	}
#endif
}


// server and local player only
void CVRBasePlayerShared::UpdateTrackers()
{
	if ( !m_bInVR )
		return;

#ifdef GAME_DLL
	// maybe do on local player too?
	for (int i = 0; i < MAX_VR_TRACKERS; i++)
	{
		if ( i < GetVRMoveData()->vr_trackers.Count() )
		{
			CmdVRTracker cmdTracker = GetVRMoveData()->vr_trackers[i];
			m_vrtrackers_index.Set(i, cmdTracker.index);
			m_vrtrackers_pos.Set(i, cmdTracker.pos);
			m_vrtrackers_ang.Set(i, cmdTracker.ang);
		}
		else
		{
			m_vrtrackers_index.Set(i, -1);
			m_vrtrackers_pos.Set(i, vec3_origin);
			m_vrtrackers_ang.Set(i, vec3_angle);
		}
	}
#endif

	// there's probably a better way to do this
	CUtlVector<CVRTracker*> updateList;

	for (int i = 0; i < m_vrtrackers_index.Count(); i++)
	{
		short index = m_vrtrackers_index[i];

		if ( index == -1 )
			continue;

		if ( index > MAX_VR_TRACKERS )
		{
			Warning("[VR] Tracker index above %u: %u\n", MAX_VR_TRACKERS, index);
			continue;
		}

		// lazy, also might be a good idea to check pos and ang too probably
		CmdVRTracker cmdTracker = { GetTrackerName(index), index, m_vrtrackers_pos[i], m_vrtrackers_ang[i] };

		CVRTracker* tracker = GetTracker( index );
		if ( tracker )
		{
			tracker->UpdateTracker(cmdTracker);
		}
		else
		{
			tracker = CreateTracker(cmdTracker);
		}

		updateList.AddToTail(tracker);
	}
	
	// now check to see if we need to delete any trackers
	for (int i = 0; i < m_VRTrackers.Count(); i++)
	{
		CVRTracker* tracker = m_VRTrackers[i];
		if ( updateList.Find(tracker) == -1 )
		{
			m_VRTrackers.FastRemove(i);
			delete tracker;
			i--;
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

