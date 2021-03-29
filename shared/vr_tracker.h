#pragma once

#ifdef CLIENT_DLL
#include "c_baseentity.h"
#include "c_baseanimating.h"
#define CBaseEntity C_BaseEntity
// #define CVRTracker C_VRTracker
// #define CVRController C_VRController
#else
#include "baseentity.h"
#include "baseanimating.h"
#endif

// #include "vr_player_shared.h"
class CVRBasePlayerShared;

#include <mathlib/vector.h>
#include <mathlib/vmatrix.h>


enum class EVRTracker
{
    INVALID = 0,
    HMD,
    LHAND,
    RHAND,
    HIP,
    LFOOT,
    RFOOT,
    COUNT = RFOOT
};


const char*     GetTrackerName( short index );
short           GetTrackerIndex( const char* name );
EVRTracker      GetTrackerEnum( const char* name );
EVRTracker      GetTrackerEnum( short index );


// Each tracker is supposed to control bone positions, like the hand bone positions, or the hip, feet, or the head
class CVRTracker // : public CBaseAnimating
{
    // DECLARE_CLASS(CVRTracker, CBaseAnimating)
public:
    // DECLARE_NETWORKCLASS();

/*#ifdef CLIENT_DLL
    DECLARE_CLIENTCLASS();
#else
    DECLARE_SERVERCLASS();
#endif

    DECLARE_PREDICTABLE();*/

    CVRTracker();

    virtual void                Spawn();

    virtual void                InitTracker( CmdVRTracker& cmdTracker, CVRBasePlayerShared* pPlayer );
    virtual void                UpdateTracker( CmdVRTracker& cmdTracker );

    virtual bool                IsHeadset()     { return (m_type == EVRTracker::HMD); }
    virtual bool                IsHand()        { return (m_type == EVRTracker::LHAND || m_type == EVRTracker::RHAND); }
    virtual bool                IsFoot()        { return (m_type == EVRTracker::LFOOT || m_type == EVRTracker::RFOOT); }
    virtual bool                IsHip()         { return (m_type == EVRTracker::HIP); }

    virtual bool                IsLeftHand()    { return (m_type == EVRTracker::LHAND); }
    virtual bool                IsRightHand()   { return (m_type == EVRTracker::RHAND); }
    virtual bool                IsLeftFoot()    { return (m_type == EVRTracker::LFOOT); }
    virtual bool                IsRightFoot()   { return (m_type == EVRTracker::RFOOT); }

    virtual void                SetupBoneName();
    virtual const char*         GetBoneName() { return m_boneName; }
    virtual const char*         GetRootBoneName() { return m_boneRootName; }

    virtual matrix3x4_t         GetWorldCoordinate();
    virtual Vector              GetAbsOrigin();
    virtual QAngle              GetAbsAngles();
    virtual void                SetAbsOrigin( Vector pos );
    virtual void                SetAbsAngles( QAngle ang );

    bool                        m_bInit;
    CVRBasePlayerShared*        m_pPlayer;

    const char*                 m_trackerName;

    const char*                 m_boneName;
    const char*                 m_boneRootName;

    Vector                      m_posOffset;
    Vector                      m_pos;
    QAngle                      m_ang;

    Vector                      m_absPos;
    QAngle                      m_absAng;
    matrix3x4_t                 m_absCoord;

    // CNetworkVar( EVRTracker,    m_type );
    EVRTracker                  m_type;
};

