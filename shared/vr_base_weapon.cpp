#include "cbase.h"
#include "in_buttons.h"
#include "takedamageinfo.h"
#include "vr_base_weapon.h"
#include "ammodef.h"


#if defined( CLIENT_DLL )
	#include "vr_cl_player.h"
#else
	#include "vr_sv_player.h"
#endif


// ----------------------------------------------------------------------------- //
// Global functions.
// ----------------------------------------------------------------------------- //

//--------------------------------------------------------------------------------------------------------
static const char * s_WeaponAliasInfo[] = 
{
	"none",		// WEAPON_NONE
	"mp5",		// WEAPON_MP5
	"shotgun",	// WEAPON_SHOTGUN
	"grenade",	// WEAPON_GRENADE
	NULL,		// WEAPON_NONE
};

//--------------------------------------------------------------------------------------------------------
//
// Given an alias, return the associated weapon ID
//
int AliasToWeaponID( const char *alias )
{
	if (alias)
	{
		for( int i=0; s_WeaponAliasInfo[i] != NULL; ++i )
			if (!Q_stricmp( s_WeaponAliasInfo[i], alias ))
				return i;
	}

	return -1;
}

//--------------------------------------------------------------------------------------------------------
//
// Given a weapon ID, return its alias
//
const char *WeaponIDToAlias( int id )
{
	if ( (id >= 32) || (id < 0) )
		return NULL;

	return s_WeaponAliasInfo[id];
}

// ----------------------------------------------------------------------------- //
// CVRBaseCombatWeapon tables.
// ----------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( VRBaseWeapon, DT_VRBaseCombatWeapon )

BEGIN_NETWORK_TABLE( CVRBaseWeapon, DT_VRBaseCombatWeapon )
#ifdef CLIENT_DLL
#else
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CVRBaseWeapon )
	DEFINE_PRED_FIELD( m_flTimeWeaponIdle, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK ),
END_PREDICTION_DATA()
#endif


#ifdef GAME_DLL

	BEGIN_DATADESC( CVRBaseWeapon )

		// New weapon Think and Touch Functions go here..

	END_DATADESC()

#endif

// ----------------------------------------------------------------------------- //
// CWeaponCSBase implementation. 
// ----------------------------------------------------------------------------- //
CVRBaseWeapon::CVRBaseWeapon()
{
	SetPredictionEligible( true );

	AddSolidFlags( FSOLID_TRIGGER ); // Nothing collides with these but it gets touches.
}


bool CVRBaseWeapon::PlayEmptySound()
{
	CPASAttenuationFilter filter( this );
	filter.UsePredictionRules();

	EmitSound( filter, entindex(), "Default.ClipEmpty_Rifle" );
	
	return 0;
}


CVRBasePlayer* CVRBaseWeapon::GetPlayerOwner() const
{
	return dynamic_cast< CVRBasePlayer* >( GetOwner() );
}


#ifdef GAME_DLL

void CVRBaseWeapon::SendReloadEvents()
{
	CVRBasePlayer *pPlayer = dynamic_cast< CVRBasePlayer* >( GetOwner() );
	if ( !pPlayer )
		return;

	// Send a message to any clients that have this entity to play the reload.
	/*CPASFilter filter( pPlayer->GetAbsOrigin() );
	filter.RemoveRecipient( pPlayer );

	UserMessageBegin( filter, "ReloadEffect" );
	WRITE_SHORT( pPlayer->entindex() );
	MessageEnd();*/

	// Make the player play his reload animation.
	// pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD );
}

#endif

