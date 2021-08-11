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

IMPLEMENT_NETWORKCLASS_ALIASED( VRBaseWeapon, DT_VRBaseCombatWeapon )

BEGIN_NETWORK_TABLE( CVRBaseWeapon, DT_VRBaseCombatWeapon )
#ifdef CLIENT_DLL
#else
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CVRBaseWeapon )
END_PREDICTION_DATA()
#endif


#ifdef GAME_DLL
BEGIN_DATADESC( CVRBaseWeapon )
END_DATADESC()
#endif

// -----------------------------------------------------------------------------

CVRBaseWeapon::CVRBaseWeapon()
{
}


#ifdef CLIENT_DLL
bool CVRBaseWeapon::ShouldDraw()
{
	if ( g_VR.active && IsCarriedByLocalPlayer() )
		return false;

	return BaseClass::ShouldDraw();
}

#endif

