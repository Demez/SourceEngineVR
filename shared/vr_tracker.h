#pragma once

#ifdef CLIENT_DLL
#include "c_baseentity.h"
#include "c_baseanimating.h"
#include "vr_renderer.h"
#define CBaseEntity C_BaseEntity
#else
#include "baseentity.h"
#include "baseanimating.h"
#endif

#include "engine_defines.h"

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


// if you want to use fast path model rendering, set this to 1, it might be causing crashes
// also we won't be able to draw the beam here if we do use this
#define VR_TRACKER_FAST_PATH 0

// Each tracker is supposed to control bone positions, like the hand bone positions, or the hip, feet, or the head
class CVRTracker
#ifdef CLIENT_DLL
    : public CDefaultClientRenderable
#if ENGINE_NEW && VR_TRACKER_FAST_PATH
    , public IClientModelRenderable
#endif
#endif
{
public:

    CVRTracker();
    virtual ~CVRTracker();

    virtual void                InitTracker( CmdVRTracker& cmdTracker, CVRBasePlayerShared* pPlayer );
    virtual void                UpdateTracker( CmdVRTracker& cmdTracker );

#ifdef CLIENT_DLL

    //---------------------------------------------------------------
    // model stuff
    //---------------------------------------------------------------

    virtual int                 LookupAttachment( const char *pAttachmentName );
    virtual void                GetBoneTransform( int iBone, matrix3x4_t &pBoneToWorld );
    virtual bool                GetAttachment( int iAttachment, matrix3x4_t &attachmentToWorld );

    virtual bool                SetupBones( matrix3x4a_t *pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime );
    virtual void                ApplyBoneMatrixTransform( matrix3x4_t& transform );

    virtual void                LoadModel();
    virtual void                LockStudioHdr();
    virtual void                UnlockStudioHdr();

    inline const char*          GetModelName() { return m_modelName; }
    const model_t*              GetModel() const { return m_model; }
    CStudioHdr*                 GetModelPtr() const { return m_pStudioHdr; }
    float                       GetModelScale();

    //---------------------------------------------------------------
    // model rendering
    //---------------------------------------------------------------

    const Vector&               GetRenderOrigin();
    const QAngle&               GetRenderAngles();
    const matrix3x4_t&          RenderableToWorldTransform();
    virtual void                GetRenderBounds( Vector& theMins, Vector& theMaxs );
    virtual void                GetRenderBoundsWorldspace( Vector& absMins, Vector& absMaxs );

#if ENGINE_NEW && VR_TRACKER_FAST_PATH
    virtual bool                GetRenderData( void *pData, ModelDataCategory_t nCategory );
    IClientModelRenderable*     GetClientModelRenderable() { return this; }
#endif

#if ENGINE_OLD
    virtual bool                IsTransparent() { return false; }
#endif

    virtual void                CreateModelInstance();
    virtual void                DestroyModelInstance();
    ModelInstanceHandle_t       GetModelInstance() { return m_ModelInstance; }

    virtual bool                ShouldDraw();
    virtual int                 DrawModel( int flags RENDER_INSTANCE_INPUT );  // i hate this macro

    bool                        ShouldReceiveProjectedTextures( int flags )  { return true; }

    IClientUnknown*             GetIClientUnknown()     { return this; }

    // bruh
    virtual IClientEntity*      GetIClientEntity()		{ return (IClientEntity*)m_pPlayer; }
    virtual C_BaseEntity*       GetBaseEntity()			{ return (C_BaseEntity*)m_pPlayer; }
#endif

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

    CVRTracker*                 GetHeadset();

    //---------------------------------------------------------------
    // other
    //---------------------------------------------------------------

#ifdef CLIENT_DLL
    virtual void                SetupBoneName();
    virtual inline const char*  GetBoneName() { return m_boneName; }
    virtual inline const char*  GetRootBoneName() { return m_boneRootName; }
#endif

    //---------------------------------------------------------------
    // position/angles
    //---------------------------------------------------------------
    virtual const matrix3x4_t&  GetWorldCoordinate();
    virtual const Vector&       GetAbsOrigin();
    virtual const QAngle&       GetAbsAngles();
    virtual void                SetAbsOrigin( Vector pos );
    virtual void                SetAbsAngles( QAngle ang );

    //---------------------------------------------------------------
    // vars
    //---------------------------------------------------------------
    bool                        m_bInit;
    CVRBasePlayerShared*        m_pPlayer;
    EVRTracker                  m_type;

    CVRTracker*                 m_hmd;

    const char*                 m_trackerName;
    const char*                 m_boneName;
    const char*                 m_boneRootName;

#ifdef CLIENT_DLL
    // VR: oh boy...
    VRHostTracker*              m_hostTracker;

    const char*                 m_modelName;
    const model_t*              m_model;
    MDLHandle_t                 m_hStudioHdr;
    CStudioHdr*                 m_pStudioHdr;

    ModelInstanceHandle_t       m_ModelInstance;
    bool                        m_inRenderList = false;

    // Shadow data
    ClientShadowHandle_t        m_ShadowHandle;
    CBitVec<1>                  m_ShadowBits; // ah yes, splitscreen vr, smh

    IMaterial*                  m_beamMaterial;
#endif

    Vector                      m_posOffset;
    Vector                      m_pos;
    QAngle                      m_ang;

    Vector                      m_absPos;
    QAngle                      m_absAng;
    matrix3x4_t                 m_absCoord;
};

