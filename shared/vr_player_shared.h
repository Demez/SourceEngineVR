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

#define MAX_VR_TRACKERS (unsigned int)EVRTracker::COUNT
// #define MAX_VR_TRACKERS 6

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
#endif
	virtual void                    OnVREnabled();
	virtual void                    OnVRDisabled();

	virtual void                    PreThink();
	virtual void                    PostThink();

	virtual Vector                  Weapon_ShootPosition();
	virtual QAngle                  GetWeaponShootAng();
	virtual Vector                  GetWeaponShootDir();

	virtual Vector                  GetAutoaimVector( float flScale );

#ifdef GAME_DLL
	virtual void                    GetAutoaimVector( autoaim_params_t &params );
#endif

	virtual const Vector            GetPlayerMins(); // NOTE: need to change in base code to be not const
	virtual const Vector            GetPlayerMaxs(); // NOTE: need to change in base code to be not const
	virtual void                    CalculatePlayerBBox();
	virtual Vector                  EyePosition();
	virtual const QAngle&           EyeAngles();

	virtual void                    AddViewRotation( float offset );
	virtual void                    CorrectViewRotateOffset();
	virtual float                   VRHeightOffset();

	virtual void                    HandleVRMoveData();
	virtual void                    UpdateTrackers();
	virtual void                    ClearCmdTrackers();

	CVRTracker*                     CreateTracker(CmdVRTracker& cmdTracker);
	CVRTracker*                     GetTracker( EVRTracker type );
	CVRTracker*                     GetTracker( short index );
	// CVRTracker*                     GetTracker( const char* name );

	inline CVRTracker*              GetHeadset()    { return GetTracker( EVRTracker::HMD ); }
	inline CVRController*           GetLeftHand()   { return (CVRController*)GetTracker( EVRTracker::LHAND ); }
	inline CVRController*           GetRightHand()  { return (CVRController*)GetTracker( EVRTracker::RHAND ); }
	CVRController*                  GetWeaponHand();

	virtual CBaseEntity*            FindUseEntity();

	// ------------------------------------------------------------------------------------------------
	// Other
	// ------------------------------------------------------------------------------------------------

	// CNetworkArray( CHandle<CVRTracker>, m_VRTrackers, MAX_VR_TRACKERS );
	// CNetworkArray( CVRTracker*, m_VRTrackers, MAX_VR_TRACKERS );
	// CNetworkArray( CVRTracker, m_VRTrackers, MAX_VR_TRACKERS );

	// CNetworkArray( CmdVRTracker, m_VRCmdTrackers, MAX_VR_TRACKERS );

	// CNetworkArray( NetworkVRTracker, m_VRCmdTrackers, MAX_VR_TRACKERS );

	// the best i can really do for this
	CNetworkArray( short, m_vrtrackers_index, MAX_VR_TRACKERS );
	CNetworkArray( Vector, m_vrtrackers_pos, MAX_VR_TRACKERS );
	CNetworkArray( QAngle, m_vrtrackers_ang, MAX_VR_TRACKERS );

	CUtlVector<CVRTracker*> m_VRTrackers;

	CNetworkVar( bool, m_bInVR );

	// network this? only used on the client at the moment
	QAngle m_vrViewAngles;

	CNetworkVector( m_viewOriginOffset );
	CNetworkVector( m_vrOriginOffset );

	CNetworkVar( float, m_vrViewRotation );
	float m_vrViewRotationGame;  // server view rotation changes, separate from client view rotation

	Vector m_minSize;
	Vector m_maxSize;

friend class CVRGameMovement;
};




#endif
