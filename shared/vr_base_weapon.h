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

	virtual void Equip( CBaseCombatCharacter *pOwner );

#ifdef CLIENT_DLL
	virtual bool Simulate();
	virtual bool ShouldDraw();

	virtual void BuildTransformations( CStudioHdr *hdr, Vector *pos, Quaternion *q, const matrix3x4_t &cameraTransform, int boneMask, CBoneBitList &boneComputed );
#endif

	CVRBasePlayer* GetVRPlayer();

private:
	CVRBaseWeapon( const CVRBaseWeapon & );
};


