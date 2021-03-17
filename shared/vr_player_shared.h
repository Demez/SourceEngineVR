#ifndef VR_PLAYER_SHARED_H
#define VR_PLAYER_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#ifdef CLIENT_DLL
#include "c_baseplayer.h"
#include "vr.h"
#define CVRTracker C_VRTracker
#define CVRController C_VRController
#else
#include "player.h"
#include "physics_bone_follower.h"
#endif

// #include "vr_tracker.h"
class CVRTracker;
class CVRController;

// #include "c_hl2_playerlocaldata.h"
// #include "vr_game_movement.h"

// handle shared code between client and server
class CVRBasePlayerShared : public CBasePlayer
{
public:
	DECLARE_CLASS( CVRBasePlayerShared, CBasePlayer );
	// DECLARE_PREDICTABLE();

	virtual void                    Spawn();

#ifdef GAME_DLL
	virtual bool                    CreateVPhysics();
	void                            InitBoneFollowers();
#endif

	virtual void                    VRThink();

	virtual void                    HandleVRMoveData();

	CVRTracker*                     CreateTracker(CmdVRTracker& cmdTracker);
	CVRTracker*                     GetTracker(const char* name);

	inline CVRTracker*              GetHeadset()    { return GetTracker("hmd"); }
	inline CVRController*           GetLeftHand()   { return (CVRController*)GetTracker("pose_lefthand"); }
	inline CVRController*           GetRightHand()  { return (CVRController*)GetTracker("pose_righthand"); }

	virtual CBaseEntity*            FindUseEntity();

	// ------------------------------------------------------------------------------------------------
	// Other
	// ------------------------------------------------------------------------------------------------

#ifdef GAME_DLL
	CBoneFollowerManager	m_BoneFollowerManager;
#endif

	CUtlVector< CVRTracker* > m_VRTrackers;
	bool m_bInVR;

	QAngle m_vrViewAngles;

friend class CVRGameMovement;
};




#endif
