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
	CmdVRTracker* GetTracker( const char* name );

	CmdVRTracker* GetHeadset()          { return GetTracker("hmd"); }
	CmdVRTracker* GetLeftController()   { return GetTracker("pose_lefthand"); }
	CmdVRTracker* GetRightController()  { return GetTracker("pose_righthand"); }

	bool						vr_active;
	float						vr_viewRotation;
	Vector						vr_originOffset;
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

	virtual void                    HandlePlaySpaceMovement( CVRMoveData *pMove );
	virtual void                    ProcessVRMovement( CVRBasePlayerShared *pPlayer, CVRMoveData *pMove );

	Vector					        m_viewOriginOffset; // offset of view based on where bbox is
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline CVRBasePlayer *CVRGameMovement::GetVRPlayer()
{
	return static_cast< CVRBasePlayer * >( player );
}


#endif