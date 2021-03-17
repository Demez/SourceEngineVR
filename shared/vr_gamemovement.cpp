#include "cbase.h"
#include "vr_gamemovement.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


extern CMoveData* g_pMoveData;

CVRMoveData* g_VRMoveData = (CVRMoveData*)g_pMoveData;

CVRMoveData* GetVRMoveData()
{
	return g_VRMoveData;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CVRGameMovement::CVRGameMovement()
{
}


CVRMoveData* CVRGameMovement::GetVRMoveData()
{
	return ToVRMoveData( mv );
}


void CVRGameMovement::PlayerMove()
{
	if ( GetVRMoveData()->vr_active )
	{
		// const Vector prevVelocity = mv->m_vecVelocity;
		// mv->m_vecVelocity.x += mv->vr_hmdOriginOffset.x;
		// mv->m_vecVelocity.y += mv->vr_hmdOriginOffset.y;

		// mv->m_flForwardMove += mv->vr_hmdOriginOffset.x;
		// mv->m_flSideMove += mv->vr_hmdOriginOffset.y;
	}

	BaseClass::PlayerMove();

	// this doesn't really work, it just jitters a lot
#if 0
	if ( mv->vr_active && !mv->vr_hmdOriginOffset.IsZero() )
	{
		// Vector originOffset = mv->vr_hmdOrigin - m_oldViewPos;
		// m_oldViewPos = mv->vr_hmdOrigin;

		Vector startPos = player->GetAbsOrigin();
		Vector endPos = player->GetAbsOrigin() + mv->vr_hmdOriginOffset;

		trace_t tr;
		TracePlayerBBox( startPos, endPos, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, tr );

		// hmmmm
		/*if ( tr.fraction == 1 )
		{
			player->SetAbsOrigin( tr.endpos );
			mv->SetAbsOrigin( tr.endpos );
		}*/

		player->SetAbsOrigin( endPos );
		mv->SetAbsOrigin( endPos );
	}
#endif

	/*if ( mv->vr_active )
	{
		// const Vector prevVelocity = mv->m_vecVelocity;
		mv->m_vecVelocity.x += mv->vr_hmdOriginOffset.x;
		mv->m_vecVelocity.y += mv->vr_hmdOriginOffset.y;

		Vector absVelocity = player->GetAbsVelocity();
		absVelocity.x += mv->vr_hmdOriginOffset.x;
		absVelocity.y += mv->vr_hmdOriginOffset.y;
		player->SetAbsVelocity( absVelocity );
	}*/
}


void CVRGameMovement::ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMoveBase )
{
	CVRMoveData* pMove = ToVRMoveData( pMoveBase );

	if ( pMove->vr_active )
	{
		// this does nothing lmao
		pMove->m_vecVelocity.x += pMove->vr_hmdOriginOffset.x;
		pMove->m_vecVelocity.y += pMove->vr_hmdOriginOffset.y;

		Vector absVelocity = pPlayer->GetAbsVelocity();
		absVelocity.x += pMove->vr_hmdOriginOffset.x;
		absVelocity.y += pMove->vr_hmdOriginOffset.y;
		pPlayer->SetAbsVelocity( absVelocity );

		/*Vector forward, right, up;

		VMatrix test;
		test.SetupMatrixOrgAngles( mv->vr_hmdOriginOffset, mv->m_vecViewAngles );
		forward = test.GetForward();
		right = test.GetLeft();*/

		// pMove->m_flForwardMove += pMove->vr_hmdOriginOffset.x;
		// pMove->m_flSideMove += pMove->vr_hmdOriginOffset.y;
	}

	BaseClass::ProcessMovement( pPlayer, pMoveBase );

	// this doesn't really work, it just jitters a lot
	if ( pMove->vr_active && !pMove->vr_hmdOriginOffset.IsZero() )
	{
		// Vector originOffset = mv->vr_hmdOrigin - m_oldViewPos;
		// m_oldViewPos = mv->vr_hmdOrigin;

		Vector startPos = pMove->GetAbsOrigin();
		Vector endPos = pMove->GetAbsOrigin() + pMove->vr_hmdOriginOffset;

		/*trace_t tr;
		TracePlayerBBox( startPos, endPos, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, tr );

		// hmmmm
		if ( tr.fraction == 1 )
		{
		player->SetAbsOrigin( tr.endpos );
		mv->SetAbsOrigin( tr.endpos );
		}*/

		pPlayer->SetAbsOrigin( endPos );
		pMove->SetAbsOrigin( endPos );
	}
}


