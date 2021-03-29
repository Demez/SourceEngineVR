//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "player.h"
#include "usercmd.h"
#include "vr_gamemovement.h"
#include "mathlib/mathlib.h"
#include "client.h"
#include "vr_player_move.h"
#include "movehelper_server.h"
#include "iservervehicle.h"
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IGameMovement *g_pGameMovement;
extern CMoveData *g_pMoveData;	// This is a global because it is subclassed by each game.
extern ConVar sv_noclipduringpause;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CVRPlayerMove::CVRPlayerMove( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Prepares for running movement
// Input  : *player - 
//			*ucmd - 
//			*pHelper - 
//			*move - 
//			time - 
//-----------------------------------------------------------------------------
void CVRPlayerMove::SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *moveBase )
{
	// VPROF( "CVRPlayerMove::SetupMove" );
	BaseClass::SetupMove( player, ucmd, pHelper, moveBase );

	// i should probably move this to CPlayerMove and CPrediction later
	// though if this works fine then i might not ever do that lmao

	CVRMoveData* move = ToVRMoveData( moveBase );

	if ( ucmd->vr_active != 1 )
	{
		if ( move->vr_active )
		{
			// move->m_vecAbsViewAngles.Init();
			// move->m_vecViewAngles.Init();
		}

		move->vr_active = false;
		move->vr_viewRotation = 0.0f;
		move->vr_originOffset.Init();
		return;
	}

	move->vr_active = true;
	move->vr_viewRotation = ucmd->vr_viewRotation;
	move->vr_originOffset = ucmd->vr_originOffset;
	move->vr_trackers = ucmd->vr_trackers;
}


