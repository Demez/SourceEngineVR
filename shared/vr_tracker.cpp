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


#define TRACKER_MODEL "models/props_junk/PopCan01a.mdl"


#if 0
#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CVRTracker )
END_PREDICTION_DATA()

#undef CVRTracker

IMPLEMENT_CLIENTCLASS_DT( C_VRTracker, DT_VRTracker, CVRTracker )
    RecvPropInt( RECVINFO(m_type) ),
END_RECV_TABLE()

#define CVRTracker C_VRTracker

#else

IMPLEMENT_SERVERCLASS_ST( CVRTracker, DT_VRTracker )
    SendPropInt( SENDINFO(m_type), -1, SPROP_UNSIGNED ),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( vr_tracker, CVRTracker );
#endif
#endif


// ==============================================================


const int g_trackerCount = 6;

const char* g_trackerNames[g_trackerCount] = 
{
    "hmd",
    "pose_lefthand",
    "pose_righthand",
    "pose_hip",
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
    return (EVRTracker)GetTrackerIndex(name);
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

    // a bit dumb
    if (!IsHeadset())
    {
        // use a temporary model for visualizing it's position
        /*bool precache = IsPrecacheAllowed();
        SetAllowPrecache( true );
        PrecacheModel( TRACKER_MODEL );
        SetModel( TRACKER_MODEL );
        SetAllowPrecache( precache );*/
    }

    m_posOffset = cmdTracker.pos;
    m_pos = cmdTracker.pos;
    m_ang = cmdTracker.ang;

    SetAbsOrigin(pPlayer->GetAbsOrigin() + m_posOffset);
    SetAbsAngles(m_ang);

    SetupBoneName();

    m_bInit = true;
}


void CVRTracker::UpdateTracker(CmdVRTracker& cmdTracker)
{
    if ( m_pPlayer == NULL )
    {
        // tf happened
        Warning("[VR] Tracker: where did the player go??\n");
        // UTIL_Remove( this );
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

// TODO: test this
#if 0 // def CLIENT_DLL
    if ( m_pPlayer->IsLocalPlayer() )
    {
        // rip data directly from g_VR (is this even faster?)
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

    m_ang = trackerAng;
    m_ang.y += viewRotation;

    SetAbsOrigin(m_pPlayer->GetAbsOrigin() + m_posOffset);
    SetAbsAngles(m_ang);

#ifdef CLIENT_DLL
    if ( !IsHeadset() )
        NDebugOverlay::Axis( GetAbsOrigin(), GetAbsAngles(), 5, false, 0.0f );

    NDebugOverlay::Text( GetAbsOrigin(), m_trackerName, false, 0.0f );
#endif

}


void CVRTracker::SetupBoneName()
{
    m_boneName = "";
    m_boneRootName = "";

    if ( m_type == EVRTracker::HMD )
    {
        m_boneName = "ValveBiped.Bip01_Head1";
        m_boneRootName = "ValveBiped.Bip01_Spine1"; // idfk
    }
    else if ( m_type == EVRTracker::LHAND )
    {
        m_boneName = "ValveBiped.Bip01_L_Hand";
        m_boneRootName = "ValveBiped.Bip01_L_Clavicle";
    }
    else if ( m_type == EVRTracker::RHAND )
    {
        m_boneName = "ValveBiped.Bip01_R_Hand";
        m_boneRootName = "ValveBiped.Bip01_R_Clavicle";
    }
    else if ( m_type == EVRTracker::HIP )
    {
        m_boneName = "ValveBiped.Bip01_Pelvis";
        m_boneRootName = "ValveBiped.Bip01_Pelvis";
    }
    else if ( m_type == EVRTracker::LFOOT )
    {
        m_boneName = "ValveBiped.Bip01_L_Foot";
        m_boneRootName = "ValveBiped.Bip01_L_Thigh";
    }
    else if ( m_type == EVRTracker::RFOOT )
    {
        m_boneName = "ValveBiped.Bip01_R_Foot";
        m_boneRootName = "ValveBiped.Bip01_R_Thigh";
    }
}


