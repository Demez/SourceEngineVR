#include "cbase.h"
#include "vr_gamemovement.h"
#include "tier1/fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


extern CMoveData* g_pMoveData;

CVRMoveData* g_VRMoveData = (CVRMoveData*)g_pMoveData;


ConVar vr_dbg_playspacemove( "vr_dbg_playspacemove", "0" );


CVRMoveData* GetVRMoveData()
{
	return g_VRMoveData;
}


CmdVRTracker* CVRMoveData::GetTracker( const char* name )
{
	for (int i = 0; i < vr_trackers.Count(); i++)
	{
		if ( V_strcmp(vr_trackers[i].name, name) == 0 )
			return &vr_trackers[i];
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CVRGameMovement::CVRGameMovement()
{
	m_viewOriginOffset.Init();
}


CVRMoveData* CVRGameMovement::GetVRMoveData()
{
	return ToVRMoveData( mv );
}


void CVRGameMovement::PlayerMove()
{
	BaseClass::PlayerMove();

	CVRMoveData* pMove = GetVRMoveData();

	if ( !pMove->vr_active )
		return;

	HandlePlaySpaceMovement( pMove );
}


void CVRGameMovement::ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMove )
{
	BaseClass::ProcessMovement( pPlayer, pMove );
	ProcessVRMovement( (CVRBasePlayerShared*)pPlayer, (CVRMoveData*)pMove );
}


void CVRGameMovement::HandlePlaySpaceMovement( CVRMoveData *pMove )
{
	Vector vrPos = pMove->vr_originOffset;
	vrPos.z = 0;

	Vector whereOriginWouldBe;
	whereOriginWouldBe = pMove->GetAbsOrigin() + m_viewOriginOffset;
	whereOriginWouldBe.z -= m_viewOriginOffset.z;

	Vector goalPos = whereOriginWouldBe + vrPos;
	Vector newPos = goalPos;

	trace_t	trace;
	TracePlayerBBox( whereOriginWouldBe, goalPos, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, trace );

	if ( !trace.startsolid && !trace.allsolid )
	{
		newPos = trace.endpos;
	}

	if ( trace.fraction == 1 )
	{
		mv->SetAbsOrigin( newPos );
		m_viewOriginOffset.Init();
	}
	else
	{
		Vector goalDiff = goalPos - newPos;
		Vector posDiff = newPos - whereOriginWouldBe;
		m_viewOriginOffset += goalDiff + posDiff;
	}

	m_viewOriginOffset.z = pMove->GetHeadset() ? pMove->GetHeadset()->pos.z : 0.0f;

	GetVRPlayer()->m_viewOriginOffset = m_viewOriginOffset;

	if ( vr_dbg_playspacemove.GetBool() )
	{
		// NDebugOverlay::Line( whereOriginWouldBe, goalPos, 0, 255, 0, false, 0.0f );
		// NDebugOverlay::Line( whereOriginWouldBe, newPos, 0, 0, 255, false, 0.0f );

		engine->Con_NPrintf( 20, "New Origin:               %s\n", VecToString(newPos) );
		engine->Con_NPrintf( 21, "Goal Origin:              %s\n", VecToString(goalPos) );
		engine->Con_NPrintf( 22, "Player Origin:            %s\n", VecToString(pMove->GetAbsOrigin()) );
		engine->Con_NPrintf( 23, "View Origin:              %s\n", VecToString(whereOriginWouldBe) );
		engine->Con_NPrintf( 24, "View Offset from Origin:  %s\n", VecToString(m_viewOriginOffset) );
		engine->Con_NPrintf( 25, "Trace Fraction: %.6f\n", trace.fraction );
	}
}


void CVRGameMovement::ProcessVRMovement( CVRBasePlayerShared *pPlayer, CVRMoveData *pMove )
{
	if ( !pMove->vr_active || pPlayer == NULL || pMove == NULL )
		return;

	// really shouldn't be here since this needs prediction on for the client, so it won't work in single player
	pPlayer->UpdateTrackers();
}


