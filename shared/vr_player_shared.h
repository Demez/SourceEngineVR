#ifndef SDK_PLAYER_SHARED_H
#define SDK_PLAYER_SHARED_H
#ifdef _WIN32
#pragma once
#endif

// useless file?

#include "networkvar.h"
#include "vr_base_weapon.h"

#ifdef CLIENT_DLL
class C_VRBasePlayer;
#else
class CVRBasePlayer;
#endif

// tf is this?
class CVRBasePlayerShared
{
public:

#ifdef CLIENT_DLL
	friend class C_VRBasePlayer;
	typedef C_VRBasePlayer OuterClass;
	DECLARE_PREDICTABLE();
#else
	friend class CVRBasePlayer;
	typedef CVRBasePlayer OuterClass;
#endif

	DECLARE_EMBEDDED_NETWORKVAR()
	DECLARE_CLASS_NOBASE( CVRBasePlayerShared );

	CVRBasePlayerShared();
	~CVRBasePlayerShared();

	void	Init( OuterClass *pOuter );

	bool	IsJumping( void ) { return m_bJumping; }
	void	SetJumping( bool bJumping );

	void	ForceUnzoom( void );

	void ComputeWorldSpaceSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	bool m_bJumping;

	float m_flLastViewAnimationTime;

	//Tony; player speeds; at spawn server and client update both of these based on class (if any)
	float m_flRunSpeed;
	float m_flSprintSpeed;
	float m_flProneSpeed;

	OuterClass *m_pOuter;
};			   




#endif //SDK_PLAYER_SHARED_H