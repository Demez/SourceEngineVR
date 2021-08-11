//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for HL1.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "vr_sv_player.h"
#include "vr_tracker.h"
#include "vr_controller.h"

#include "predicted_viewmodel.h"
#include "iservervehicle.h"
#include "viewport_panel_names.h"
#include "info_camera_link.h"
#include "GameStats.h"
#include "obstacle_pushaway.h"
#include "in_buttons.h"
#include "igamemovement.h"
#include "vr_util.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


extern CMoveData* g_pMoveData;


// -------------------------------------------------------------------------------- //
// Tables.
// -------------------------------------------------------------------------------- //

// CVRBasePlayerShared Data Tables
//=============================

// specific to the local player
/*BEGIN_SEND_TABLE_NOBASE( CVRBasePlayerShared, DT_SDKSharedLocalPlayerExclusive )
END_SEND_TABLE()

BEGIN_SEND_TABLE_NOBASE( CVRBasePlayerShared, DT_SDKPlayerShared )
	SendPropDataTable( "sdksharedlocaldata", 0, &REFERENCE_SEND_TABLE(DT_SDKSharedLocalPlayerExclusive), SendProxy_SendLocalDataTable ),
END_SEND_TABLE()

extern void SendProxy_Origin( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );

BEGIN_SEND_TABLE_NOBASE( CVRBasePlayer, DT_SDKLocalPlayerExclusive )
	// send a hi-res origin to the local player for use in prediction
	SendPropVector	(SENDINFO(m_vecOrigin), -1,  SPROP_NOSCALE|SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_Origin ),

	// SendPropFloat( SENDINFO_VECTORELEM(m_angEyeAngles, 0), 8, SPROP_CHANGES_OFTEN, -90.0f, 90.0f ),

	SendPropInt( SENDINFO( m_ArmorValue ), 8, SPROP_UNSIGNED ),
END_SEND_TABLE()

BEGIN_SEND_TABLE_NOBASE( CVRBasePlayer, DT_SDKNonLocalPlayerExclusive )
	SendPropFloat( SENDINFO_VECTORELEM(m_angEyeAngles, 0), 8, SPROP_CHANGES_OFTEN, -90.0f, 90.0f ),
	SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 1), 10, SPROP_CHANGES_OFTEN ),
END_SEND_TABLE()*/


BEGIN_DATADESC( CVRBasePlayer )
	// DEFINE_ARRAY( m_VRTrackers, FIELD_EHANDLE, MAX_VR_TRACKERS ),
END_DATADESC()

// main table
// IMPLEMENT_SERVERCLASS_ST( CVRBasePlayer, DT_VRBasePlayer )
// END_SEND_TABLE()

IMPLEMENT_SERVERCLASS_ST( CVRBasePlayer, DT_VRBasePlayer )
	// SendPropArray3( SENDINFO_ARRAY3(m_VRTrackers), SendPropEHandle( SENDINFO_ARRAY(m_VRTrackers) ) ),
END_SEND_TABLE()


CVRBasePlayer::CVRBasePlayer()
{
	UseClientSideAnimation();
	// m_angEyeAngles.Init();
}


CVRBasePlayer::~CVRBasePlayer()
{
}


CVRBasePlayer *CVRBasePlayer::CreatePlayer( const char *className, edict_t *ed )
{
	CVRBasePlayer::s_PlayerEdict = ed;
	return (CVRBasePlayer*)CreateEntityByName( className );
}


void CVRBasePlayer::PreThink(void)
{
	BaseClass::PreThink();
}


void CVRBasePlayer::PostThink()
{
	BaseClass::PostThink();
	
	// Store the eye angles pitch so the client can compute its animation state correctly.
	// m_angEyeAngles = EyeAngles();

    // m_PlayerAnimState->Update( m_angEyeAngles[YAW], m_angEyeAngles[PITCH] );
}


void CVRBasePlayer::Spawn()
{
	SetMoveType( MOVETYPE_WALK );
	RemoveSolidFlags( FSOLID_NOT_SOLID );

	BaseClass::Spawn();

	// SetContextThink( &CVRBasePlayer::SDKPushawayThink, gpGlobals->curtime + PUSHAWAY_THINK_INTERVAL, SDK_PUSHAWAY_THINK_CONTEXT );
}


void CVRBasePlayer::SendUseEvent( CBaseEntity* pUseEntity )
{
	if ( !pUseEntity )
		return;

	//!!!UNDONE: traceline here to prevent +USEing buttons through walls			

	int caps = pUseEntity->ObjectCaps();
	variant_t emptyVariant;
	// if ( ( (m_nButtons & IN_USE) && (caps & FCAP_CONTINUOUS_USE) ) || ( (m_afButtonPressed & IN_USE) && (caps & (FCAP_IMPULSE_USE|FCAP_ONOFF_USE)) ) )
	if ( ( (m_nButtons & IN_USE) && (caps & FCAP_CONTINUOUS_USE) ) || ( (caps & (FCAP_IMPULSE_USE|FCAP_ONOFF_USE)) ) )
	{
		if ( caps & FCAP_CONTINUOUS_USE )
		{
			m_afPhysicsFlags |= PFLAG_USING;
		}

		if ( pUseEntity->ObjectCaps() & FCAP_ONOFF_USE )
		{
			pUseEntity->AcceptInput( "Use", this, this, emptyVariant, USE_ON );
		}
		else
		{
			pUseEntity->AcceptInput( "Use", this, this, emptyVariant, USE_TOGGLE );
		}
	}
	// UNDONE: Send different USE codes for ON/OFF.  Cache last ONOFF_USE object to send 'off' if you turn away
	// else if ( (m_afButtonReleased & IN_USE) && (pUseEntity->ObjectCaps() & FCAP_ONOFF_USE) )	// BUGBUG This is an "off" use
	else if ( pUseEntity->ObjectCaps() & FCAP_ONOFF_USE )	// BUGBUG This is an "off" use
	{
		pUseEntity->AcceptInput( "Use", this, this, emptyVariant, USE_OFF );
	}
}


void CVRBasePlayer::PlayerUse()
{
	if ( !m_bInVR )
	{
		BaseClass::PlayerUse();
		return;
	}

	CBaseEntity* pUseEntity = NULL;
	if ( g_pMoveData->m_nButtons & IN_PICKUP_LEFT && GetLeftHand() )
	{
		SendUseEvent( GetLeftHand()->FindUseEntity() );
	}

	if ( g_pMoveData->m_nButtons & IN_PICKUP_RIGHT && GetRightHand() )
	{
		SendUseEvent( GetRightHand()->FindUseEntity() );
	}
}


void CVRBasePlayer::PickupObject( CBaseEntity *pObject, bool bLimitMassAndSize )
{
	if ( !m_bInVR )
	{
		BaseClass::PickupObject( pObject, bLimitMassAndSize );
		return;
	}

	if ( g_pMoveData->m_nButtons & IN_PICKUP_LEFT && GetLeftHand() && GetLeftHand()->GetLastUseEntity() == pObject )
	{
		GetLeftHand()->GrabObject( pObject );
	}

	if ( g_pMoveData->m_nButtons & IN_PICKUP_RIGHT && GetRightHand() && GetRightHand()->GetLastUseEntity() == pObject )
	{
		GetRightHand()->GrabObject( pObject );
	}
}


void CVRBasePlayer::SetAnimation( PLAYER_ANIM playerAnim )
{
	int animDesired;
	Activity idealActivity = ACT_DIERAGDOLL;

	SetActivity( idealActivity );

	// animDesired = SelectWeightedSequence( Weapon_TranslateActivity ( idealActivity ) );
	animDesired = SelectWeightedSequence( idealActivity );

	if ( animDesired == -1 )
	{
		animDesired = 0;
	}

	// Already using the desired animation?
	if ( GetSequence() == animDesired )
		return;

	m_flPlaybackRate = 1.0;
	ResetSequence( animDesired );
	SetCycle( 0 );
}


void CVRBasePlayer::RumbleEffect( unsigned char index, unsigned char rumbleData, unsigned char rumbleFlags )
{
	if ( !m_bInVR )
		BaseClass::RumbleEffect( index, rumbleData, rumbleFlags );
}



