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


// you know, *technically* i don't need this with the abstracted bone system
// i could just have the type be EVRBone instead, theoretically allowing for trackers for every single bone
// even though that won't ever happen, unless valve's BCI headset will do that, idk
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
    inline bool                 IsType( EVRTracker type ) { return (m_type == type); }

    inline bool                 IsHeadset()     { return (m_type == EVRTracker::HMD); }
    inline bool                 IsHand()        { return (m_type == EVRTracker::LHAND || m_type == EVRTracker::RHAND); }
    inline bool                 IsFoot()        { return (m_type == EVRTracker::LFOOT || m_type == EVRTracker::RFOOT); }
    inline bool                 IsHip()         { return (m_type == EVRTracker::HIP); }

    inline bool                 IsLeftHand()    { return (m_type == EVRTracker::LHAND); }
    inline bool                 IsRightHand()   { return (m_type == EVRTracker::RHAND); }
    inline bool                 IsLeftFoot()    { return (m_type == EVRTracker::LFOOT); }
    inline bool                 IsRightFoot()   { return (m_type == EVRTracker::RFOOT); }

    //---------------------------------------------------------------
    // other
    //---------------------------------------------------------------

#ifdef CLIENT_DLL
    virtual void                SetupBoneName();
    virtual inline const char*  GetBoneName() { return m_boneName; }
    virtual inline const char*  GetRootBoneName() { return m_boneRootName; }
#endif

    virtual void               LoadModel();
    inline const char*         GetModelName() { return m_modelName; }

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

    const char*                 m_modelName;
    const model_t*              m_model;
    IMaterial*                  m_beamMaterial;

    Vector                      m_posOffset;
    Vector                      m_pos;
    QAngle                      m_ang;

    Vector                      m_absPos;
    QAngle                      m_absAng;
    matrix3x4_t                 m_absCoord;
};

