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
class CVRBasePlayer : public CBasePlayer
{
public:
	DECLARE_CLASS( CVRBasePlayer, CBasePlayer );
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

	virtual void                    RegisterUserMessages();

	// CNetworkQAngle( m_angEyeAngles );	// Copied from EyeAngles() so we can send it to the client.

	// ------------------------------------------------------------------------------------------------
	// Modified Functions if in VR
	// ------------------------------------------------------------------------------------------------

	// ------------------------------------------------------------------------------------------------
	// New VR Functions
	// ------------------------------------------------------------------------------------------------

	// ------------------------------------------------------------------------------------------------
	// NON-VR Functions
	// ------------------------------------------------------------------------------------------------
	virtual void                    SDKPushawayThink( void );
	virtual bool                    ClientCommand( const CCommand &args );

	CVRBaseCombatWeapon*            GetActiveSDKWeapon() const;
	virtual void                    CreateViewModel( int viewmodelindex = 0 );

};


inline CVRBasePlayer *ToVRPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

	return static_cast< CVRBasePlayer* >( pEntity );
}


#endif
