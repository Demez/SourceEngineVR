#include "cbase.h"
#include "vr_game_movement.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CVRGameMovement::CVRGameMovement()
{
}


void CVRGameMovement::ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMove )
{
	#ifdef CLIENT_DLL
	/*if ( g_VR.active )
	{
		VRTracker* headset = g_VR.GetTrackerByName("hmd");
		pMove->m_vecViewAngles;
	}*/
	#endif

	BaseClass::ProcessMovement( pPlayer, pMove );
}


