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


static const char *pFollowerBoneNames[] =
{
	"ValveBiped.Bip01_Head1",
	"ValveBiped.Bip01_Spine",

	"ValveBiped.Bip01_L_Hand",
	"ValveBiped.Bip01_R_Hand",

	"ValveBiped.Bip01_L_Foot",
	"ValveBiped.Bip01_R_Foot",
};

IMPLEMENT_NETWORKCLASS( CVRBasePlayerShared, DT_VRBasePlayerShared )

#ifdef CLIENT_DLL

BEGIN_PREDICTION_DATA( CVRBasePlayerShared )
	// DEFINE_PRED_ARRAY( m_VRTrackers, FIELD_EHANDLE, MAX_VR_TRACKERS, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()

#else

BEGIN_DATADESC( CVRBasePlayerShared )
	// DEFINE_ARRAY( m_VRTrackers, FIELD_EHANDLE, MAX_VR_TRACKERS ),
	// DEFINE_AUTO_ARRAY( m_VRTrackers, FIELD_EHANDLE ),
END_DATADESC()

#endif


BEGIN_NETWORK_TABLE( CVRBasePlayerShared, DT_VRBasePlayerShared )
#ifdef CLIENT_DLL
	// RecvPropArray3( RECVINFO_ARRAY(m_VRTrackers), RecvPropEHandle( RECVINFO( m_VRTrackers[0] ) ) ),
#else
	// SendPropArray3( SENDINFO_ARRAY3(m_VRTrackers), SendPropEHandle( SENDINFO_ARRAY(m_VRTrackers) ) ),
#endif
END_NETWORK_TABLE()


void CVRBasePlayerShared::Spawn()
{
	BaseClass::Spawn();

#if 0 // def GAME_DLL
	for (int i = 0; i < MAX_VR_TRACKERS; i++)
	{
		m_VRTrackers.Set( i, INVALID_EHANDLE );
	}
#endif
}


#ifdef GAME_DLL
bool CVRBasePlayerShared::CreateVPhysics()
{
	bool ret = BaseClass::CreateVPhysics();
	// InitBoneFollowers();
	return ret;
}


void CVRBasePlayerShared::InitBoneFollowers()
{
	// Don't do this if we're already loaded
	/*if ( m_BoneFollowerManager.GetNumBoneFollowers() != 0 )
		return;

	// Init our followers
	m_BoneFollowerManager.InitBoneFollowers( this, ARRAYSIZE(pFollowerBoneNames), pFollowerBoneNames );*/
}


void CVRBasePlayerShared::SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways )
{
	// Skip this work if we're already marked for transmission.
	if ( pInfo->m_pTransmitEdict->Get( entindex() ) )
		return;

	BaseClass::SetTransmit( pInfo, bAlways );

	/*for (int i = 0; i < MAX_VR_TRACKERS; i++)
	{
		CVRTracker *pTracker = m_VRTrackers[i];
		if ( !pTracker )
			continue;

		pTracker->SetTransmit( pInfo, bAlways );
	}*/
}
#endif


void CVRBasePlayerShared::PreThink()
{
	BaseClass::PreThink();

	HandleVRMoveData();
}


void CVRBasePlayerShared::HandleVRMoveData()
{
#if ENGINE_ASW
	m_bInVR = false;
#else
	m_bInVR = GetVRMoveData()->vr_active;
#endif

	// TODO: delete the trackers
	if ( !m_bInVR )
		return;

	// call moved to vr_gamemovement.cpp for prediction
	// UpdateTrackers();
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
#if 0 // def CLIENT_DLL
			// do we actually even need this check on the client?
			if ( IsLocalPlayer() )  // can only predict if this is the local player
#endif
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


/*CVRTracker* CVRBasePlayerShared::GetTracker( const char* name )
{
	return GetTracker( GetTrackerEnum(name) );
}*/


CVRTracker* CVRBasePlayerShared::CreateTracker( CmdVRTracker& cmdTracker )
{
	if ( !m_bInVR )
		return NULL;

	// uhhhh
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
	if ( !m_bInVR )
		return BaseClass::FindUseEntity();

	return NULL;
}

