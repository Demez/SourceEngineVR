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

//-----------------------------------------------------------------------------
// Purpose: VR specific movement code
//-----------------------------------------------------------------------------
class CVRGameMovement : public CGameMovement
{
	typedef CGameMovement BaseClass;
public:

	CVRGameMovement();

	virtual void            ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMove );

	CVRBasePlayer*          GetVRPlayer();
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline CVRBasePlayer *CVRGameMovement::GetVRPlayer()
{
	return static_cast< CVRBasePlayer * >( player );
}


#endif