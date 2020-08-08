#ifndef VR_CL_PLAYER_H
#define VR_CL_PLAYER_H
#ifdef _WIN32
#pragma once
#endif

#include "c_baseplayer.h"
// #include "c_hl2_playerlocaldata.h"
// #include "vr_game_movement.h"
#include "vr.h"

// TODO: make this a template class
class C_VRBasePlayer : public C_BasePlayer
{
public:
	DECLARE_CLASS( C_VRBasePlayer, C_BasePlayer );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_VRBasePlayer();
	C_VRBasePlayer( const C_VRBasePlayer & );

	// ------------------------------------------------------------------------------------------------
	// Modified Functions if in VR
	// ------------------------------------------------------------------------------------------------
	virtual float                   GetFOV();
	virtual const QAngle&           EyeAngles();
	virtual const QAngle&           LocalEyeAngles();

	virtual void					PreThink( void );
	virtual void					PostThink( void );

	// ------------------------------------------------------------------------------------------------
	// New VR Only Functions
	// ------------------------------------------------------------------------------------------------
	virtual void                    SetViewRotateOffset( float offset );
	virtual void                    AddViewRotateOffset( float offset );
	virtual void                    CorrectViewRotateOffset();

	// ------------------------------------------------------------------------------------------------
	// Other
	// ------------------------------------------------------------------------------------------------

	virtual void                    RegisterUserMessages();
	virtual void                    OnDataChanged( DataUpdateType_t updateType );
	virtual void                    ExitLadder();

	// Input handling
	virtual bool                    CreateMove( float flInputSampleTime, CUserCmd *pCmd );
	virtual void                    PerformClientSideObstacleAvoidance( float flFrameTime, CUserCmd *pCmd );

	virtual bool                    TestMove( const Vector &pos, float fVertDist, float radius, const Vector &objPos, const Vector &objDir );

	// NOTE: you might need this in Alien Swarm or newer, otherwise the player is stuck at the origin point
	virtual bool                    ShouldRegenerateOriginFromCellBits() const { return true; }


	float viewOffset;

friend class CVRGameMovement;
};


#define CVRBasePlayer C_VRBasePlayer


#endif
