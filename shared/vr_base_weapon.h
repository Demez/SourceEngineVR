#pragma once


#if defined( CLIENT_DLL )
	#define CVRBaseWeapon C_VRBaseWeapon
	#define CVRBasePlayer C_VRBasePlayer
#endif

class CVRBasePlayer;

class CVRBaseWeapon : public CBaseCombatWeapon
{
public:
	DECLARE_CLASS( CVRBaseWeapon, CBaseCombatWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

#ifdef GAME_DLL
	DECLARE_DATADESC();
#endif

	CVRBaseWeapon();

#ifdef CLIENT_DLL
	virtual bool ShouldDraw();
#endif

private:
	CVRBaseWeapon( const CVRBaseWeapon & );
};


