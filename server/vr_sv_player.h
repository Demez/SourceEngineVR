//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for SDK Game
//
// $NoKeywords: $
//=============================================================================//

#ifndef VR_SV_PLAYER_H
#define VR_SV_PLAYER_H
#pragma once

#include "basemultiplayerplayer.h"
#include "server_class.h"
// #include "sdk_playeranimstate.h"
#include "vr_player_shared.h"


// =============================================================================
// Base VR Player - Server
// Maybe make this a template to use your own base player class here?
// like using CBaseMultiplayerPlayer instead?
// =============================================================================
class CVRBasePlayer : public CVRBasePlayerShared
{
public:
	DECLARE_CLASS( CVRBasePlayer, CVRBasePlayerShared );
	DECLARE_SERVERCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CVRBasePlayer();
	~CVRBasePlayer();

	static CVRBasePlayer *CreatePlayer( const char *className, edict_t *ed );
	// static CVRBasePlayer* Instance( int iEnt );

	virtual void                    PreThink();
	virtual void                    PostThink();
	virtual void                    Spawn();

	virtual void                    SetAnimation( PLAYER_ANIM playerAnim );

	// CNetworkQAngle( m_angEyeAngles );	// Copied from EyeAngles() so we can send it to the client.

	// ------------------------------------------------------------------------------------------------
	// Modified Functions if in VR
	// ------------------------------------------------------------------------------------------------
	virtual void                    PlayerUse();
	virtual void                    SendUseEvent( CBaseEntity* pUseEntity );
	virtual void                    PickupObject( CBaseEntity *pObject, bool bLimitMassAndSize = true );

	// ------------------------------------------------------------------------------------------------
	// New VR Functions
	// ------------------------------------------------------------------------------------------------


};


inline CVRBasePlayer *ToVRPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

	return static_cast< CVRBasePlayer* >( pEntity );
}


#endif
