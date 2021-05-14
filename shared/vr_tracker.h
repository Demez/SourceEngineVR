#pragma once

#ifdef CLIENT_DLL
#include "c_baseentity.h"
#include "c_baseanimating.h"
#define CBaseEntity C_BaseEntity
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

    /*
    // TODO: set these up later
    LKNEE,
    RKNEE,
    LELBOW,
    RELBOW,
    LSHOULDER,
    RSHOULDER,
    SPINE,
    CHEST,
    */
    
    COUNT = RFOOT
};


const char*     GetTrackerName( short index );
short           GetTrackerIndex( const char* name );
EVRTracker      GetTrackerEnum( const char* name );
EVRTracker      GetTrackerEnum( short index );


// Each tracker is supposed to control bone positions, like the hand bone positions, or the hip, feet, or the head
class CVRTracker
{
public:

    CVRTracker();

    virtual void                Spawn();

    virtual void                InitTracker( CmdVRTracker& cmdTracker, CVRBasePlayerShared* pPlayer );
    virtual void                UpdateTracker( CmdVRTracker& cmdTracker );

    //---------------------------------------------------------------
    // convenience tracker type checks
    //---------------------------------------------------------------
    virtual inline bool         IsHeadset()     { return (m_type == EVRTracker::HMD); }
    virtual inline bool         IsHand()        { return (m_type == EVRTracker::LHAND || m_type == EVRTracker::RHAND); }
    virtual inline bool         IsFoot()        { return (m_type == EVRTracker::LFOOT || m_type == EVRTracker::RFOOT); }
    virtual inline bool         IsHip()         { return (m_type == EVRTracker::HIP); }

    virtual inline bool         IsLeftHand()    { return (m_type == EVRTracker::LHAND); }
    virtual inline bool         IsRightHand()   { return (m_type == EVRTracker::RHAND); }
    virtual inline bool         IsLeftFoot()    { return (m_type == EVRTracker::LFOOT); }
    virtual inline bool         IsRightFoot()   { return (m_type == EVRTracker::RFOOT); }

    //---------------------------------------------------------------
    // other
    //---------------------------------------------------------------

    virtual void                SetupBoneName();
    virtual inline const char*  GetBoneName() { return m_boneName; }
    virtual inline const char*  GetRootBoneName() { return m_boneRootName; }

    //---------------------------------------------------------------
    // position/angles
    //---------------------------------------------------------------
    virtual matrix3x4_t         GetWorldCoordinate();
    virtual Vector              GetAbsOrigin();
    virtual QAngle              GetAbsAngles();
    virtual void                SetAbsOrigin( Vector pos );
    virtual void                SetAbsAngles( QAngle ang );

    //---------------------------------------------------------------
    // vars
    //---------------------------------------------------------------
    bool                        m_bInit;
    CVRBasePlayerShared*        m_pPlayer;
    EVRTracker                  m_type;

    const char*                 m_trackerName;
    const char*                 m_boneName;
    const char*                 m_boneRootName;

    Vector                      m_posOffset;
    Vector                      m_pos;
    QAngle                      m_ang;

    Vector                      m_absPos;
    QAngle                      m_absAng;
    matrix3x4_t                 m_absCoord;
};

