#ifndef VR_OPENVR_H
#define VR_OPENVR_H

#pragma once


#include "tier1/utlvector.h"
#include "tier1/utldict.h"
#include "mathlib/vector.h"
#include "mathlib/vmatrix.h"
// #include "tier1/interface.h"
// #include "appframework/IAppSystem.h"

#include <openvr.h>
#include "vr_util.h"


struct VRHostTracker
{
    const char* name;
    VMatrix mat;
    VMatrix matconv;    // converted to source's coordinate system
    Vector pos;         // converted to source's coordinate system
    Vector posraw;      // position from whatever is setup for calculating this
    Vector vel;
    QAngle ang;
    QAngle angvel;
};


struct VRViewParams
{
    int horizontalFOVLeft;
    int horizontalFOVRight;

    float aspectRatioLeft;
    float aspectRatioRight;

    // int recommendedWidth;
    // int recommendedHeight;

    int rtWidth = 512;
    int rtHeight = 512;

    Vector eyeToHeadTransformPosLeft;
    Vector eyeToHeadTransformPosRight;
};


enum class VRAction
{
    None = -1,
    Boolean,
    Vector1, // rename to Analog?
    Vector2,
    Skeleton,
};



// kind of shit, oh well
typedef struct VRBaseAction
{
    const char* name = NULL;
    VRAction type = VRAction::None;
} VRBaseAction;


typedef struct VRBoolAction: VRBaseAction {
    VRBoolAction() {type = VRAction::Boolean;}
    bool value;
} VRBoolAction;

typedef struct VRVector1Action: VRBaseAction {
    VRVector1Action() {type = VRAction::Vector1;}
    float value;
} VRVector1Action;

typedef struct VRVector2Action: VRBaseAction {
    VRVector2Action() {type = VRAction::Vector2;}
    float x;
    float y;
} VRVector2Action;

typedef struct VRSkeletonAction: VRBaseAction {
    VRSkeletonAction() {type = VRAction::Skeleton;}
    Vector2D fingerCurls[5];
} VRSkeletonAction;


extern vr::IVRSystem*          g_pOpenVR;


bool            OVR_NeedD3DInit();
int             OVR_Shutdown();
void            OVR_SetupFOVOffset();

int             OVR_SetActionManifest( const char* fileName );
int             OVR_TriggerHaptic( const char* actionName );

void            OVR_AddActiveActionSet(const char* actionSetName);
void            OVR_ResetActiveActionSets();
void            OVR_UpdatePosesAndActions();

void            OVR_GetPoses( CUtlVector< VRHostTracker* > &trackers );
void            OVR_GetActions( CUtlVector< VRBaseAction* > &actions );

void            OVR_Submit( void* submitData, vr::EVREye eye );

void            OVR_InitDX9Device( void* deviceData );
void            OVR_DX9EXToDX11( void* leftEyeData, void* rightEyeData );

// VRBaseAction*   OVR_GetActions();

VRViewParams    OVR_GetViewParameters();



#endif