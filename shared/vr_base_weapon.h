#ifndef WEAPON_SDKBASE_H
#define WEAPON_SDKBASE_H
#ifdef _WIN32
#pragma once
#endif

// #include "sdk_playeranimstate.h"
// #include "sdk_weapon_parse.h"

#if defined( CLIENT_DLL )
	#define CVRBaseCombatWeapon C_VRBaseCombatWeapon
	#define CVRBasePlayer C_VRBasePlayer
#endif

class CVRBasePlayer;

// These are the names of the ammo types that the weapon script files reference.
#define AMMO_BULLETS			"AMMO_BULLETS"
#define AMMO_ROCKETS			"AMMO_ROCKETS"
#define AMMO_GRENADE			"AMMO_GRENADE"

//--------------------------------------------------------------------------------------------------------
//
// Weapon IDs for all SDK Game weapons
//
/*typedef enum
{
	WEAPON_NONE = 0,

	WEAPON_MP5,
	WEAPON_SHOTGUN,
	WEAPON_GRENADE,
	
	WEAPON_MAX,		// number of weapons weapon index
} SDKWeaponID;*/

typedef enum
{
	Primary_Mode = 0,
	Secondary_Mode,
} SDKWeaponMode;

const char *WeaponIDToAlias( int id );

// also make this a template?
class CVRBaseCombatWeapon : public CBaseCombatWeapon
{
public:
	DECLARE_CLASS( CVRBaseCombatWeapon, CBaseCombatWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CVRBaseCombatWeapon();

	#ifdef GAME_DLL
		DECLARE_DATADESC();
	#endif

	// All predicted weapons need to implement and return true
	virtual bool	IsPredicted() const { return true; }
	// virtual SDKWeaponID GetWeaponID( void ) const { return WEAPON_NONE; }
	
	// Get SDK weapon specific weapon data.
	// CSDKWeaponInfo const	&GetSDKWpnData() const;

	// Get a pointer to the player that owns this weapon
	CVRBasePlayer* GetPlayerOwner() const;

	// override to play custom empty sounds
	virtual bool PlayEmptySound();

#ifdef GAME_DLL
	virtual void SendReloadEvents();
#endif

private:
	CVRBaseCombatWeapon( const CVRBaseCombatWeapon & );
};


#endif // WEAPON_SDKBASE_H
