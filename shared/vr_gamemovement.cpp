#include "cbase.h"
#include "vr_gamemovement.h"
#include "tier1/fmtstr.h"
#include "coordsize.h"
#include "engine/ivdebugoverlay.h"

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

//-----------------------------------------------------------------------------
// Stupid functions in gamemovement that shouldn't exist
// breaks player movement a lot apparently
//-----------------------------------------------------------------------------
/*const Vector& CVRGameMovement::GetPlayerMins( bool ducked ) const
{
	return player->GetPlayerMins();
}

const Vector& CVRGameMovement::GetPlayerMaxs( bool ducked ) const
{	
	return player->GetPlayerMaxs();
}

const Vector& CVRGameMovement::GetPlayerMins( void ) const
{
	return player->GetPlayerMins();
}

const Vector& CVRGameMovement::GetPlayerMaxs( void ) const
{	
	return player->GetPlayerMaxs();
}*/


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

	Vector viewOrigin = pMove->GetAbsOrigin() + m_viewOriginOffset;
	viewOrigin.z -= m_viewOriginOffset.z;

	Vector goalPos = viewOrigin + vrPos;
	Vector newPos = goalPos;

	bool movePlayerOrigin = false;
	bool isMovingUp = false;
	trace_t	trace;

	TracePlayerBBox( viewOrigin, goalPos, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, trace );

	if ( !trace.startsolid && !trace.allsolid )
	{
		newPos = trace.endpos;
		movePlayerOrigin = ( trace.fraction == 1 );
	}

	// do another trace up for steps or slopes
	if ( trace.fraction != 1 && player->m_Local.m_bAllowAutoMovement )
	{
		Vector upGoalPos = goalPos;
		upGoalPos.z += player->m_Local.m_flStepSize + DIST_EPSILON;

		TracePlayerBBox( viewOrigin, upGoalPos, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, trace );

		movePlayerOrigin = ( trace.fraction == 1 );
		isMovingUp = ( trace.fraction == 1 );
	}

	// maybe remove this fraction check?
	// like if we're moving up in a very cramped area, this won't be called
	if ( trace.fraction == 1 )
	{
		// find the ground position now
		TracePlayerBBox( trace.endpos, goalPos, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, trace );

		newPos = trace.endpos;
		movePlayerOrigin = true;
	}

	// is the goal position a valid place to move?
	// we don't need to check this for moving up
	if ( movePlayerOrigin && !isMovingUp )
	{
		// we want to check if this is a valid place to move, and we aren't trying to walk through a wall
		// so trace where the player origin is vs. where the view origin is to make sure we aren't doing that
		TracePlayerBBox( mv->GetAbsOrigin(), viewOrigin, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, trace );

		if ( trace.fraction != 1 )
		{
			newPos = trace.endpos;
			movePlayerOrigin = ( trace.fraction == 1 );
		}
	}

	if ( movePlayerOrigin )
	{
		mv->SetAbsOrigin( newPos );
		m_viewOriginOffset.Init();
	}
	else
	{
		Vector goalDiff = goalPos - newPos;
		Vector posDiff = newPos - viewOrigin;
		m_viewOriginOffset += goalDiff + posDiff;
	}

	m_viewOriginOffset.z = pMove->GetHeadset() ? pMove->GetHeadset()->pos.z : 0.0f;

	GetVRPlayer()->m_viewOriginOffset = m_viewOriginOffset;

	if ( vr_dbg_playspacemove.GetBool() )
	{
		if ( newPos.DistTo( pMove->GetAbsOrigin() ) > 0.001f )
		{
			debugoverlay->AddLineOverlay( pMove->GetAbsOrigin() + Vector(0, 0, 8), newPos + Vector(0, 0, 8), 0, 255, 0, true, 0.0f );
			// NDebugOverlay::Line( viewOrigin, newPos, 0, 0, 255, false, 0.0f );
		}

		engine->Con_NPrintf( 20, "New Origin:               %s\n", VecToString(newPos) );
		engine->Con_NPrintf( 21, "Goal Origin:              %s\n", VecToString(goalPos) );
		engine->Con_NPrintf( 22, "Player Origin:            %s\n", VecToString(pMove->GetAbsOrigin()) );
		engine->Con_NPrintf( 23, "View Origin:              %s\n", VecToString(viewOrigin) );
		engine->Con_NPrintf( 24, "View Offset from Origin:  %s\n", VecToString(m_viewOriginOffset) );
		engine->Con_NPrintf( 25, "Trace Fraction: %.6f\n", trace.fraction );
		engine->Con_NPrintf( 26, "Should Move Player: %s\n", movePlayerOrigin ? "YES" : "NO" );
		engine->Con_NPrintf( 27, "Is Moving Up: %s\n", isMovingUp ? "YES" : "NO" );
	}
}


void CVRGameMovement::ProcessVRMovement( CVRBasePlayerShared *pPlayer, CVRMoveData *pMove )
{
	if ( !pMove->vr_active || pPlayer == NULL || pMove == NULL )
		return;

	// really shouldn't be here since this needs prediction on for the client, so it won't work in single player
	// pPlayer->UpdateTrackers();
}


