#ifndef VR_PLAYER_SHARED_H
#define VR_PLAYER_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#ifdef CLIENT_DLL
#include "c_baseplayer.h"
#include "vr.h"
// #define CVRTracker C_VRTracker
// #define CVRController C_VRController
#else
#include "player.h"
#include "physics_bone_follower.h"
#endif

#include "vr_tracker.h"
class CVRTracker;
class CVRController;

// #define MAX_VR_TRACKERS (int)EVRTracker::COUNT
#define MAX_VR_TRACKERS 6

// #include "c_hl2_playerlocaldata.h"
// #include "vr_game_movement.h"

// handle shared code between client and server
class CVRBasePlayerShared : public CBasePlayer
{
public:
	DECLARE_CLASS( CVRBasePlayerShared, CBasePlayer );
	DECLARE_PREDICTABLE();
	DECLARE_NETWORKCLASS();

#if !defined( CLIENT_DLL )
	DECLARE_DATADESC();
#endif

	virtual void                    Spawn();

#ifdef GAME_DLL
	virtual bool                    CreateVPhysics();
	void                            InitBoneFollowers();
	virtual void                    SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways );
#endif

	virtual void                    PreThink();

	virtual Vector                  Weapon_ShootPosition();

	virtual const Vector            GetPlayerMins(); // NOTE: need to change in base code to be not const
	virtual const Vector            GetPlayerMaxs(); // NOTE: need to change in base code to be not const
	virtual void                    CalculatePlayerBBox();
	virtual Vector                  EyePosition();
	virtual const QAngle&           EyeAngles();

	virtual void                    HandleVRMoveData();
	virtual void                    UpdateTrackers();

	CVRTracker*                     CreateTracker(CmdVRTracker& cmdTracker);
	CVRTracker*                     GetTracker( EVRTracker type );
	CVRTracker*                     GetTracker( short index );
	// CVRTracker*                     GetTracker( const char* name );

	inline CVRTracker*              GetHeadset()    { return GetTracker( EVRTracker::HMD ); }
	inline CVRController*           GetLeftHand()   { return (CVRController*)GetTracker( EVRTracker::LHAND ); }
	inline CVRController*           GetRightHand()  { return (CVRController*)GetTracker( EVRTracker::RHAND ); }

	virtual CBaseEntity*            FindUseEntity();

	// ------------------------------------------------------------------------------------------------
	// Other
	// ------------------------------------------------------------------------------------------------

#ifdef GAME_DLL
	// CBoneFollowerManager	m_BoneFollowerManager;
#endif

	// CNetworkArray( CHandle<CVRTracker>, m_VRTrackers, MAX_VR_TRACKERS );
	CUtlVector<CVRTracker*> m_VRTrackers;

	// CNetworkVar( bool, m_bInVR );
	bool m_bInVR;

	// network this? only used on the client at the moment
	QAngle m_vrViewAngles;
	Vector m_viewOriginOffset;  // calculated from vr_gamemovement.cpp

	Vector m_minSize;
	Vector m_maxSize;

friend class CVRGameMovement;
};




#endif
