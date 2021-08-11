#ifndef VR_GAME_MOVEMENT_H
#define VR_GAME_MOVEMENT_H

#include "gamemovement.h"

#if defined( CLIENT_DLL )
	class C_VRBasePlayer;
	#include "vr_cl_player.h"
	#define CVRBasePlayer C_VRBasePlayer
#else
	#include "vr_sv_player.h"
#endif


class CVRMoveData: public CMoveData
{
public:
	CmdVRTracker* GetTracker( EVRTracker tracker );

	CmdVRTracker* GetHeadset()          { return GetTracker( EVRTracker::HMD ); }
	CmdVRTracker* GetLeftController()   { return GetTracker( EVRTracker::LHAND ); }
	CmdVRTracker* GetRightController()  { return GetTracker( EVRTracker::RHAND ); }

	bool						vr_active;
	float						vr_viewRotation;
	CUtlVector< CmdVRTracker >	vr_trackers;
};


CVRMoveData* GetVRMoveData();

inline CVRMoveData* ToVRMoveData( CMoveData* move )
{
	return (CVRMoveData*)move;
}


//-----------------------------------------------------------------------------
// Purpose: VR specific movement code
//-----------------------------------------------------------------------------
class CVRGameMovement : public CGameMovement
{
	typedef CGameMovement BaseClass;
public:

	CVRGameMovement();

	CVRMoveData*                    GetVRMoveData();
	CVRBasePlayer*                  GetVRPlayer();

	/*virtual const Vector&           GetPlayerMins( bool ducked ) const;
	virtual const Vector&           GetPlayerMaxs( bool ducked ) const;
	virtual const Vector&           GetPlayerMins() const;
	virtual const Vector&           GetPlayerMaxs() const;*/

	virtual void                    ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMove );
	virtual void                    PlayerMove();

	virtual void                    PlaySpaceMove( CVRBasePlayerShared* vrPlayer, CVRMoveData *pMove );
	virtual void                    PlaySpaceMoveWalk( CVRBasePlayerShared* vrPlayer, CVRMoveData *pMove );
	virtual void                    PlaySpaceMoveFrozen( CVRBasePlayerShared* vrPlayer, CVRMoveData *pMove );
	virtual bool                    PlaySpaceMoveLadder( CVRBasePlayerShared* vrPlayer, CVRMoveData *pMove );

	// will return something else later, or have trace_t in it
	virtual bool                    CheckForLadderModelHack();

	virtual void                    ProcessVRMovement( CVRBasePlayerShared *pPlayer, CVRMoveData *pMove );
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline CVRBasePlayer *CVRGameMovement::GetVRPlayer()
{
	return static_cast< CVRBasePlayer * >( player );
}


#endif