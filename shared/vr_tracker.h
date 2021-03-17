#pragma once

#ifdef CLIENT_DLL
#include "c_baseentity.h"
#include "c_baseanimating.h"
#define CBaseEntity C_BaseEntity
#define CVRTracker C_VRTracker
#define CVRController C_VRController
#else
#include "baseentity.h"
#include "baseanimating.h"
#endif

// #include "vr_player_shared.h"
class CVRBasePlayerShared;

#include <mathlib/vector.h>
#include <mathlib/vmatrix.h>


// Each tracker is supposed to control bone positions, like the hand bone positions, or the hip, feet, or the head
class CVRTracker: public CBaseAnimating
{
    DECLARE_CLASS(CVRTracker, CBaseAnimating)
public:
    DECLARE_PREDICTABLE();

    virtual void                Spawn();

    virtual void                InitTracker( CmdVRTracker& cmdTracker, CVRBasePlayerShared* pPlayer );
    virtual void                UpdateTracker( CmdVRTracker& cmdTracker );

    CVRBasePlayerShared*        m_pPlayer;
    bool                        m_bIsHeadset;
    const char*                 m_trackerName;
    Vector                      m_pos;
    QAngle                      m_ang;
};

