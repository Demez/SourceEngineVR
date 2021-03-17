#include "cbase.h"
#include "vr_util.h"
#include "vr_tracker.h"
#include "hl2_shareddefs.h"  // TODO: move LINK_ENTITY_TO_CLASS_DUMB out of here
#include "vr_gamemovement.h"

#include "vr_player_shared.h"


LINK_ENTITY_TO_CLASS_DUMB( vr_tracker, CVRTracker );


extern CMoveData* g_pMoveData;


#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CVRTracker )
	DEFINE_PRED_FIELD( m_pos, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()
#endif


void CVRTracker::Spawn()
{
    BaseClass::Spawn();

    m_pPlayer = NULL;
    
    m_pos.Init();
    m_ang.Init();

#ifdef GAME_DLL
    // m_debugOverlays |= OVERLAY_BBOX_BIT;
#endif
}


void CVRTracker::InitTracker(CmdVRTracker& cmdTracker, CVRBasePlayerShared* pPlayer)
{
    m_pPlayer = pPlayer;

    Vector worldPos = pPlayer->GetAbsOrigin();
    // Vector oldPos = GetAbsOrigin();

    m_trackerName = cmdTracker.name;

#ifdef GAME_DLL
    SetName( MAKE_STRING(m_trackerName) );
#endif

    // a bit dumb
    if ( V_strcmp(m_trackerName, "hmd") == 0 )
    {
        m_bIsHeadset = true;
    }
    else
    {
        // use a temporary model for visualizing it's position
        bool precache = IsPrecacheAllowed();
        SetAllowPrecache( true );
        PrecacheModel( "models/props_junk/PopCan01a.mdl" );
        SetModel( "models/props_junk/PopCan01a.mdl" );
        SetAllowPrecache( precache );
    }

    m_pos = cmdTracker.pos;
    m_ang = cmdTracker.ang;

    SetAbsOrigin(worldPos + m_pos);
    SetAbsAngles(m_ang);

#if 0
	if ( V_strcmp(tracker->m_trackerName, "pose_lefthand") == 0 )
	{
		m_BoneFollowerManager.AddBoneFollower( tracker, "ValveBiped.Bip01_L_Hand" );
	}

	else if ( V_strcmp(tracker->m_trackerName, "pose_righthand") == 0 )
	{
		m_BoneFollowerManager.AddBoneFollower( tracker, "ValveBiped.Bip01_R_Hand" );
	}
#endif
}


void CVRTracker::UpdateTracker(CmdVRTracker& cmdTracker)
{
    if ( m_pPlayer == NULL )
    {
        // tf happened
        UTIL_Remove( this );
        return;
    }

    Vector hmdOffset = GetVRMoveData()->vr_hmdOrigin;
    float viewRotation = GetVRMoveData()->vr_viewRotation;

    m_pos = cmdTracker.pos;
    m_pos.x -= hmdOffset.x;
    m_pos.y -= hmdOffset.y;

    Vector finalPos;
    VectorYawRotate(m_pos, viewRotation, finalPos);
    m_pos = finalPos;

    m_ang = cmdTracker.ang;
    m_ang.y += viewRotation;

    SetAbsOrigin(m_pPlayer->GetAbsOrigin() + m_pos);
    SetAbsAngles(m_ang);

#if 0
	if ( V_strcmp(tracker->m_trackerName, "pose_lefthand") == 0 )
	{
		m_BoneFollowerManager.AddBoneFollower( tracker, "ValveBiped.Bip01_L_Hand" );
	}

	else if ( V_strcmp(tracker->m_trackerName, "pose_righthand") == 0 )
	{
		m_BoneFollowerManager.AddBoneFollower( tracker, "ValveBiped.Bip01_R_Hand" );
	}
#endif
}


