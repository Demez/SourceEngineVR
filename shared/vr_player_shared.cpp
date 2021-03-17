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


void CVRBasePlayerShared::Spawn()
{
	BaseClass::Spawn();
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
#endif


void CVRBasePlayerShared::VRThink()
{
#if 1 // def GAME_DLL
	// m_BoneFollowerManager.UpdateBoneFollowers(this);
	HandleVRMoveData();
#endif
}


void CVRBasePlayerShared::HandleVRMoveData()
{
	m_bInVR = GetVRMoveData()->vr_active;

	// TODO: delete the trackers
	if ( !m_bInVR )
		return;

	for (int i = 0; i < GetVRMoveData()->vr_trackers.Count(); i++)
	{
		CmdVRTracker cmdTracker = GetVRMoveData()->vr_trackers[i];

		// not sure how this is happening yet
		if ( cmdTracker.name == NULL )
			continue;

		CVRTracker* tracker = GetTracker(cmdTracker.name);
		if ( tracker )
		{
			tracker->UpdateTracker(cmdTracker);
		}
		else
		{
			tracker = CreateTracker(cmdTracker);
		}

		// wtf
		if ( tracker == NULL )
			continue;

#ifdef GAME_DLL
		/*if ( V_strcmp(tracker->m_trackerName, "pose_lefthand") == 0 )
		{
			m_BoneFollowerManager.AddBoneFollower( tracker, "ValveBiped.Bip01_L_Hand" );
		}

		else if ( V_strcmp(tracker->m_trackerName, "pose_righthand") == 0 )
		{
			m_BoneFollowerManager.AddBoneFollower( tracker, "ValveBiped.Bip01_R_Hand" );
		}*/
#endif
	}
}


CVRTracker* CVRBasePlayerShared::GetTracker( const char* name )
{
	if ( !m_bInVR )
		return NULL;

	for (int i = 0; i < m_VRTrackers.Count(); i++)
	{
		CVRTracker* tracker = m_VRTrackers[i];
		if ( V_strcmp(tracker->m_trackerName, name) == 0 )
			return tracker;
	}

	return NULL;
}


CVRTracker* CVRBasePlayerShared::CreateTracker( CmdVRTracker& cmdTracker )
{
	if ( !m_bInVR )
		return NULL;

	// uhhhh
#ifdef GAME_DLL
	CVRTracker* tracker;
	if ( V_strcmp(cmdTracker.name, "pose_lefthand") == 0 || V_strcmp(cmdTracker.name, "pose_righthand") == 0 )
	{
		tracker = (CVRTracker*)CreateEntityByName("vr_controller");
	}
	else
	{
		tracker = (CVRTracker*)CreateEntityByName("vr_tracker");
	}

	if ( tracker == NULL )
		return NULL;

	// tracker->InitializeAsClientEntity("", false);
	DispatchSpawn(tracker);
	tracker->InitTracker(cmdTracker, this);

	m_VRTrackers.AddToTail(tracker);
	return tracker;
#else
	return NULL;
#endif
}


CBaseEntity* CVRBasePlayerShared::FindUseEntity()
{
	// CVRController will handle this
	if ( !m_bInVR )
		return BaseClass::FindUseEntity();

#if 0
	// NOTE: Some debris objects are useable too, so hit those as well
	// A button, etc. can be made out of clip brushes, make sure it's +useable via a traceline, too.
	int useableContents = MASK_SOLID | CONTENTS_DEBRIS | CONTENTS_PLAYERCLIP;

	Vector boxSize(16, 16, 16);
	Vector controllerOrigin(0, 0, 0);

	trace_t tr;

	// Demez TODO: same issue with not knowing what controller triggered the grip button here as well
	// maybe it would be a better idea to somehow store the buttons each controller presses in the CmdVRTracker struct?
	// not sure how i would figure out what controller pressed that button though, because both controllers can press it aaaa
	if ( GetRightHand() )
	{
		controllerOrigin = GetRightHand()->GetAbsOrigin();
		UTIL_TraceHull( controllerOrigin, controllerOrigin, -boxSize, boxSize, useableContents, this, COLLISION_GROUP_NONE, &tr );
		if (tr.m_pEnt)
			return tr.m_pEnt;
	}

	if ( GetLeftHand() )
	{
		controllerOrigin = GetLeftHand()->GetAbsOrigin();
		UTIL_TraceHull( controllerOrigin, controllerOrigin, -boxSize, boxSize, useableContents, this, COLLISION_GROUP_NONE, &tr );
		if (tr.m_pEnt)
			return tr.m_pEnt;
	}
#endif

	return NULL;
}

