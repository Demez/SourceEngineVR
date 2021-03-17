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
	bool						vr_active;
	float						vr_viewRotation;
	Vector						vr_hmdOrigin;
	Vector						vr_hmdOriginOffset;
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

	CVRMoveData*            GetVRMoveData();
	CVRBasePlayer*          GetVRPlayer();

	virtual void            ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMove );
	virtual void            PlayerMove();

	Vector					m_oldViewPos;
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline CVRBasePlayer *CVRGameMovement::GetVRPlayer()
{
	return static_cast< CVRBasePlayer * >( player );
}


#endif