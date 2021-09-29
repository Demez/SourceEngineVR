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


// disabled as it's unfinished
ConVar vr_weaponmodel("vr_weaponmodel", "0");


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


CVRBasePlayer* CVRBaseWeapon::GetVRPlayer()
{
	if ( !GetOwner() || !GetOwner()->IsPlayer() )
		return NULL;

	return (CVRBasePlayer*)GetOwner();
}

//-----------------------------------------------------------------------------
// Purpose: Become a child of the owner (MOVETYPE_FOLLOW)
//			disables collisions, touch functions, thinking
// Input  : *pOwner - new owner/operator
//-----------------------------------------------------------------------------
void CVRBaseWeapon::Equip( CBaseCombatCharacter *pOwner )
{
	// Attach the weapon to an owner
	SetAbsVelocity( vec3_origin );
	RemoveSolidFlags( FSOLID_TRIGGER );
	FollowEntity( pOwner );
	SetOwner( pOwner );
	SetOwnerEntity( pOwner );

	// Break any constraint I might have to the world.
	RemoveEffects( EF_ITEM_BLINK );

#if !defined( CLIENT_DLL )
	if ( m_pConstraint != NULL )
	{
		RemoveSpawnFlags( SF_WEAPON_START_CONSTRAINED );
		physenv->DestroyConstraint( m_pConstraint );
		m_pConstraint = NULL;
	}
#endif

	m_flNextPrimaryAttack		= gpGlobals->curtime;
	m_flNextSecondaryAttack		= gpGlobals->curtime;
	SetTouch( NULL );
	SetThink( NULL );
#if !defined( CLIENT_DLL )
	VPhysicsDestroyObject();
#endif

	bool useViewModel = pOwner->IsPlayer();

	if ( CVRBasePlayer* vrPlayer = dynamic_cast<CVRBasePlayer*>(pOwner) )
	{
		useViewModel = vrPlayer->InVR() ? vr_weaponmodel.GetBool() : true;
	}

	if ( useViewModel )
	{
		SetModel( GetViewModel() );
	}
	else
	{
		// Make the weapon ready as soon as any NPC picks it up.
		m_flNextPrimaryAttack = gpGlobals->curtime;
		m_flNextSecondaryAttack = gpGlobals->curtime;
		SetModel( GetWorldModel() );
	}
}


#ifdef CLIENT_DLL
bool CVRBaseWeapon::Simulate()
{
	bool bRet = BaseClass::Simulate();

	CVRBasePlayer* player = GetVRPlayer();
	if ( !player || !player->m_bInVR )
		return bRet;

	if ( m_flashlightMgr )
	{
		Vector muzzleForward, muzzleRight, muzzleUp;
		AngleVectors( player->GetWeaponShootAng(), &muzzleForward, &muzzleRight, &muzzleUp );
		m_flashlightMgr->UpdateFlashlight( player->Weapon_ShootPosition(), muzzleForward, muzzleRight, muzzleUp, 60, true, 500, 0/*, "effects/flashlight001"*/ );
	}

	return bRet;
}


bool CVRBaseWeapon::ShouldDraw()
{
	if ( g_VR.active && IsCarriedByLocalPlayer() && ( m_iState == WEAPON_IS_ACTIVE ) && vr_weaponmodel.GetBool() )
		return true;

	return BaseClass::ShouldDraw();
}


void CVRBaseWeapon::BuildTransformations( CStudioHdr *hdr, Vector *pos, Quaternion *q, const matrix3x4_t &cameraTransform, int boneMask, CBoneBitList &boneComputed )
{
	CVRBasePlayer* player = GetVRPlayer();

	bool boneMerge = IsEffectActive( EF_BONEMERGE );

	//if ( !player || !player->m_bInVR )
	if ( !player || !player->m_bInVR || player->GetActiveWeapon() != this || !vr_weaponmodel.GetBool() )
		return BaseClass::BuildTransformations( hdr, pos, q, cameraTransform, boneMask, boneComputed );

	if ( boneMerge )
		RemoveEffects( EF_BONEMERGE );

	BaseClass::BuildTransformations( hdr, pos, q, cameraTransform, boneMask, boneComputed );

	// put the active weapon onto the hand here
	CVRController* weaponHand = player->GetWeaponHand();
	if ( weaponHand )
	{
		// int i = LookupBone( "ValveBiped.Bip01_R_Hand" );
		int i = LookupBone( "ValveBiped.Weapon_bone" );
		bool isHandBone = false;

		// 357 has no Weapon_bone, but has bone???
		if ( i == -1 )
		{
			//i = LookupBone( "ValveBiped.bone" );
			i = LookupBone( "ValveBiped.Bip01_R_Hand" );
			isHandBone = true;
		}

		if ( i != -1 )
		{
			matrix3x4a_t& bone = GetBoneForWrite(i);

			Vector pos, absPos, absBonePos;
			QAngle ang, absAng, absBoneAng;

			AngleMatrix( weaponHand->GetAbsAngles(), weaponHand->GetAbsOrigin(), bone );

			MatrixAngles( bone, absAng, absPos );
			SetAbsOrigin( absPos );
			SetAbsAngles( absAng );

			// now fix the angles somehow
			// what i want to do is rotate the weapon so that the forward vector will match the tracker point vector
			Vector pointDir, forwardDir;
			pointDir = weaponHand->GetPointDir();
			QAngle pointAng = weaponHand->GetPointAng();

			AngleVectors( absAng, &forwardDir );

			//NDebugOverlay::Axis( absPos, absAng, 10, true, 0.f );
			//NDebugOverlay::Line( absPos, absPos + forwardDir * 12, 100, 255, 100, true, 0.f );

			// VectorPerpendicularToVector
			// QuaternionLookAt

			VectorAngles( pointDir, forwardDir, absAng );
			AngleVectors( absAng, &forwardDir );

			// now rotate 180 if needed? how tf can i check that?
			if ( !isHandBone )
			{
				VMatrix mat, rotateMat, outMat;
				MatrixFromAngles( absAng, mat );
				MatrixBuildRotationAboutAxis( rotateMat, Vector(1, 0, 0), 180 );
				MatrixMultiply( mat, rotateMat, outMat );
				MatrixToAngles( outMat, absAng );

				// now rotate down slightly (god this is awful, really should just have a vr weapon bone instead)
				// also this needs to be different PER WEAPON AAAA
				MatrixFromAngles( absAng, mat );
				//MatrixBuildRotationAboutAxis( rotateMat, Vector(0, 1, 0), -12.5 );
				MatrixBuildRotationAboutAxis( rotateMat, Vector(0, 1, 0), -4 );
				MatrixMultiply( mat, rotateMat, outMat );
				MatrixToAngles( outMat, absAng );
			}

			AngleMatrix( absAng, absPos, bone );
			SetAbsAngles( absAng );

			//AngleVectors( absAng, &forwardDir );
			//NDebugOverlay::Line( absPos, absPos + forwardDir * 12, 100, 150, 255, true, 0.f );

			//NDebugOverlay::BoxAngles( absBonePos, Vector(-2,-2,-2), Vector(2,2,2), absBoneAng, 100, 100, 200, 2, 0.f );
			//NDebugOverlay::BoxAngles( absPos, Vector(-2,-2,-2), Vector(2,2,2), absAng, 200, 100, 100, 2, 0.f );
		}
		else
		{
			SetAbsOrigin( weaponHand->GetAbsOrigin() );
			SetAbsAngles( weaponHand->GetAbsAngles() );
		}

		/*for (int i = 0; i < 5; i++)
		{
			matrix3x4a_t& bone = GetBoneForWrite(i);

			mstudiobone_t *pOwnerBones = GetModelPtr()->pBone( i );

			const char* name = pOwnerBones ? pOwnerBones->pszName() : "bruh";

			Vector pos;
			MatrixGetColumn( bone, 3, pos );

			NDebugOverlay::Box( pos, Vector(-2,-2,-2), Vector(2,2,2), 200, 100, 100, 2, 0.f );
			NDebugOverlay::Text( pos, name, false, 0.f );
		}*/
	}

	if ( boneMerge )
		AddEffects( EF_BONEMERGE );
}

#endif

