#include "cbase.h"
#include "vr_util.h"
#include "vr_tracker.h"
#include "hl2_shareddefs.h"  // TODO: move LINK_ENTITY_TO_CLASS_DUMB out of here
#include "vr_gamemovement.h"
#include "debugoverlay_shared.h"

#include "vr_player_shared.h"


// LINK_ENTITY_TO_CLASS_DUMB( vr_tracker, CVRTracker );


extern CMoveData* g_pMoveData;


#ifdef CLIENT_DLL
ConVar vr_interp_trackers("vr_interp_trackers", "0.1");
#endif


// ==============================================================


const int g_trackerCount = 6;

const char* g_trackerNames[g_trackerCount] = 
{
    "hmd",
    "pose_lefthand",
    "pose_righthand",
    "pose_waist",
    "pose_leftfoot",
    "pose_rightfoot",
};


const char* GetTrackerName(short index)
{
    if (index < 0 || index > g_trackerCount)
    {
        Warning("Invalid tracker index %h", index);
        return "";
    }

    return g_trackerNames[index];
}


short GetTrackerIndex(const char* name)
{
    for (int i = 0; i < g_trackerCount; i++)
    {
        if ( V_strcmp(name, g_trackerNames[i]) == 0 )
        {
            return i;
        }
    }

    Warning("Invalid tracker name %s", name);
    return -1;
}


EVRTracker GetTrackerEnum( const char* name )
{
    return (EVRTracker)(GetTrackerIndex(name) + 1);
}

EVRTracker GetTrackerEnum( short index )
{
    if ( index < 0 || index > (int)EVRTracker::COUNT )
        return EVRTracker::INVALID;

    return (EVRTracker)(index + 1);
}


// ==============================================================


CVRTracker::CVRTracker()
{
}


void CVRTracker::Spawn()
{
    m_pPlayer = NULL;
    
    m_posOffset.Init();
    m_pos.Init();
    m_ang.Init();
    m_absPos.Init();
    m_absAng.Init();
}


void CVRTracker::LoadModel()
{
    
}


matrix3x4_t CVRTracker::GetWorldCoordinate()
{
    AngleMatrix( m_absAng, m_absPos, m_absCoord );
    return m_absCoord;
}


Vector CVRTracker::GetAbsOrigin()
{
    return m_absPos;
}


QAngle CVRTracker::GetAbsAngles()
{
    return m_absAng;
}


void CVRTracker::SetAbsOrigin( Vector pos )
{
    m_absPos = pos;
}


void CVRTracker::SetAbsAngles( QAngle ang )
{
    m_absAng = ang;
}


void CVRTracker::InitTracker(CmdVRTracker& cmdTracker, CVRBasePlayerShared* pPlayer)
{
    m_pPlayer = pPlayer;

    m_trackerName = cmdTracker.name;
    m_type = GetTrackerEnum(cmdTracker.index);

    m_posOffset = cmdTracker.pos;
    m_pos = cmdTracker.pos;
    m_ang = cmdTracker.ang;

    SetAbsOrigin(pPlayer->GetAbsOrigin() + m_posOffset);
    SetAbsAngles(m_ang);

#ifdef CLIENT_DLL
    SetupBoneName();
#endif

    m_bInit = true;
}


void CVRTracker::UpdateTracker(CmdVRTracker& cmdTracker)
{
    if ( m_pPlayer == NULL )
    {
        // tf happened
        Warning("[VR] Tracker: where did the player go??\n");
        return;
    }

    Vector originOffset;
    originOffset.Init();

    if ( IsHeadset() )
    {
        originOffset = m_pos;
    }
    else
    {
        CVRTracker* hmd = m_pPlayer->GetHeadset();
        if ( hmd )
        {
            originOffset = hmd->m_pos;
        }
    }

    Vector trackerPos = cmdTracker.pos;
    QAngle trackerAng = cmdTracker.ang;

// TODO: maybe get rid of this?
#if 0 // def CLIENT_DLL
    if ( m_pPlayer->IsLocalPlayer() )
    {
        // rip data directly from g_VR (is this even faster?)
        // the only way this would be worth it is if i didn't send tracker updates every frame
        // and only every x frames, which i doubt i would do for this
        VRHostTracker* localTracker = g_VR.GetTrackerByName( m_trackerName );

        // did it get nuked somehow?
        if ( localTracker == NULL )
        {
            Warning("[VR] Local Tracker \"%s\" does not exist, but the entity does?", m_trackerName );
        }
        else
        {
            trackerPos = localTracker->pos;
            trackerAng = localTracker->ang;
        }
    }
#endif

    float viewRotation = GetVRMoveData()->vr_viewRotation;

    m_pos = trackerPos;

    m_posOffset = trackerPos;
    m_posOffset.x -= originOffset.x;
    m_posOffset.y -= originOffset.y;

    Vector finalPos;
    VectorYawRotate(m_posOffset, viewRotation, finalPos);
    m_posOffset = finalPos;

    m_absAng = m_ang = trackerAng;
    m_absAng.y += viewRotation;

    SetAbsOrigin(m_pPlayer->GetAbsOrigin() + m_posOffset);

#ifdef CLIENT_DLL
    if ( !IsHeadset() )
    {
        NDebugOverlay::Axis( GetAbsOrigin(), GetAbsAngles(), 5, true, 0.0f );
        // NDebugOverlay::Box( GetAbsOrigin(), Vector(-4,-4,-4), Vector(4,4,4), 255, 255, 255, 1, 0);
    }

    // NDebugOverlay::Text( GetAbsOrigin(), m_trackerName, true, 0.0f );
#endif
}


#ifdef CLIENT_DLL
void CVRTracker::SetupBoneName()
{
    m_boneName = "";
    m_boneRootName = "";

    C_VRBasePlayer* ply = (C_VRBasePlayer*)m_pPlayer;

    if ( m_type == EVRTracker::HMD )
    {
        m_boneName = ply->GetBoneName( EVRBone::Head );
        m_boneRootName = ply->GetBoneName( EVRBone::Spine ); // idk
    }
    else if ( m_type == EVRTracker::LHAND )
    {
        m_boneName = ply->GetBoneName( EVRBone::LHand );
        m_boneRootName = ply->GetBoneName( EVRBone::LShoulder );
    }
    else if ( m_type == EVRTracker::RHAND )
    {
        m_boneName = ply->GetBoneName( EVRBone::RHand );
        m_boneRootName = ply->GetBoneName( EVRBone::RShoulder );
    }
    else if ( m_type == EVRTracker::HIP )
    {
        m_boneName = ply->GetBoneName( EVRBone::Pelvis );
        m_boneRootName = ply->GetBoneName( EVRBone::Pelvis );
    }
    else if ( m_type == EVRTracker::LFOOT )
    {
        m_boneName = ply->GetBoneName( EVRBone::LFoot );
        m_boneRootName = ply->GetBoneName( EVRBone::LThigh );
    }
    else if ( m_type == EVRTracker::RFOOT )
    {
        m_boneName = ply->GetBoneName( EVRBone::RFoot );
        m_boneRootName = ply->GetBoneName( EVRBone::RThigh );
    }
}
#endif

