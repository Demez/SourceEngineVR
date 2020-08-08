//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for HL1.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "vr_sv_player.h"
// #include "vr_sv_base_weapon.h"

#include "predicted_viewmodel.h"
#include "iservervehicle.h"
#include "viewport_panel_names.h"
#include "info_camera_link.h"
#include "GameStats.h"
#include "obstacle_pushaway.h"
#include "in_buttons.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



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
DEFINE_THINKFUNC( SDKPushawayThink ),
END_DATADESC()

// main table
IMPLEMENT_SERVERCLASS_ST( CVRBasePlayer, DT_VRBasePlayer )
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

	QAngle angles = GetLocalAngles();
	angles[PITCH] = 0;
	SetLocalAngles( angles );
	
	// Store the eye angles pitch so the client can compute its animation state correctly.
	// m_angEyeAngles = EyeAngles();

    // m_PlayerAnimState->Update( m_angEyeAngles[YAW], m_angEyeAngles[PITCH] );
}



#define SDK_PUSHAWAY_THINK_CONTEXT	"SDKPushawayThink"
void CVRBasePlayer::SDKPushawayThink( void )
{
	// Push physics props out of our way.
	PerformObstaclePushaway( this );
}


void CVRBasePlayer::Spawn()
{
	SetMoveType( MOVETYPE_WALK );
	RemoveSolidFlags( FSOLID_NOT_SOLID );

	BaseClass::Spawn();

	// SetContextThink( &CVRBasePlayer::SDKPushawayThink, gpGlobals->curtime + PUSHAWAY_THINK_INTERVAL, SDK_PUSHAWAY_THINK_CONTEXT );
}


CVRBaseCombatWeapon* CVRBasePlayer::GetActiveSDKWeapon() const
{
	return dynamic_cast< CVRBaseCombatWeapon* >( GetActiveWeapon() );
}


void CVRBasePlayer::CreateViewModel( int index /*=0*/ )
{
	Assert( index >= 0 && index < MAX_VIEWMODELS );

	if ( GetViewModel( index ) )
		return;

	CPredictedViewModel *vm = ( CPredictedViewModel * )CreateEntityByName( "predicted_viewmodel" );
	if ( vm )
	{
		vm->SetAbsOrigin( GetAbsOrigin() );
		vm->SetOwner( this );
		vm->SetIndex( index );
		DispatchSpawn( vm );
		vm->FollowEntity( this, false );
		m_hViewModel.Set( index, vm );
	}
}


// might do something with this, idk
bool CVRBasePlayer::ClientCommand( const CCommand &args )
{
	return BaseClass::ClientCommand( args );

	const char *pcmd = args[0];
	
	if ( FStrEq( pcmd, "menuopen" ) )
	{
#if defined ( SDK_USE_PLAYERCLASSES )
		SetClassMenuOpen( true );
#endif
		return true;
	}
	else if ( FStrEq( pcmd, "menuclosed" ) )
	{
#if defined ( SDK_USE_PLAYERCLASSES )
		SetClassMenuOpen( false );
#endif
		return true;
	}
	else if ( FStrEq( pcmd, "droptest" ) )
	{
		// ThrowActiveWeapon();
		return true;
	}
}




// ------------------------------------------------------------------------------------------------
// Modified Functions if in VR
// ------------------------------------------------------------------------------------------------



