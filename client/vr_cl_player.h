#ifndef VR_CL_PLAYER_H
#define VR_CL_PLAYER_H
#ifdef _WIN32
#pragma once
#endif

#include "c_baseplayer.h"
// #include "c_hl2_playerlocaldata.h"
// #include "vr_game_movement.h"
#include "vr_player_shared.h"
#include "vr_tracker.h"
#include "vr.h"

// TODO: make this a template class
class C_VRBasePlayer : public CVRBasePlayerShared
{
public:
	DECLARE_CLASS( C_VRBasePlayer, CVRBasePlayerShared );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_VRBasePlayer();
	C_VRBasePlayer( const C_VRBasePlayer & );

	// ------------------------------------------------------------------------------------------------
	// Modified Functions if in VR
	// ------------------------------------------------------------------------------------------------
	virtual float                   GetFOV();
	virtual Vector                  EyePosition();
	virtual const QAngle&           EyeAngles();
	virtual const QAngle&           LocalEyeAngles();
	virtual bool					CreateMove( float flInputSampleTime, CUserCmd *pCmd );
	virtual void                    ClientThink();

	// ------------------------------------------------------------------------------------------------
	// Playermodel controlling
	// ------------------------------------------------------------------------------------------------
	virtual void                    BuildTransformations( CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed );
	virtual void                    BuildFirstPersonMeathookTransformations( CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed, const char *pchHeadBoneName );

	// ------------------------------------------------------------------------------------------------
	// New VR Only Functions
	// ------------------------------------------------------------------------------------------------
	virtual void                    SetViewRotateOffset( float offset );
	virtual void                    AddViewRotateOffset( float offset );
	virtual void                    CorrectViewRotateOffset();
	virtual const QAngle&           EyeAnglesNoOffset();
	/*virtual void                    HandleVRMoveData();

	C_VRTracker*                    CreateTracker(CmdVRTracker& cmdTracker);
	C_VRTracker*                    GetTracker(const char* name);

	inline C_VRTracker*             GetHeadset()    { return GetTracker("hmd"); }
	inline C_VRTracker*             GetLeftHand()   { return GetTracker("pose_lefthand"); }
	inline C_VRTracker*             GetRightHand()  { return GetTracker("pose_righthand"); }*/

	// ------------------------------------------------------------------------------------------------
	// Other
	// ------------------------------------------------------------------------------------------------
	virtual void                    OnDataChanged( DataUpdateType_t updateType );
	virtual void                    ExitLadder();

	// Input handling
	// virtual void                    PerformClientSideObstacleAvoidance( float flFrameTime, CUserCmd *pCmd );

	virtual bool                    TestMove( const Vector &pos, float fVertDist, float radius, const Vector &objPos, const Vector &objDir );

	// NOTE: you might need this in Alien Swarm or newer, otherwise the player is stuck at the origin point
	virtual bool                    ShouldRegenerateOriginFromCellBits() const { return true; }

	// CUtlVector< C_VRTracker* > m_VRTrackers;

	float lastViewHeight;
	float viewOffset;

friend class CVRGameMovement;
};


#define CVRBasePlayer C_VRBasePlayer


#endif
