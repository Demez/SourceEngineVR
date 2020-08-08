#ifndef VR_OPENVR_H
#define VR_OPENVR_H

#pragma once


#include "tier1/utlvector.h"
#include "tier1/utldict.h"
#include "mathlib/vector.h"
// #include "tier1/interface.h"
// #include "appframework/IAppSystem.h"

#include <openvr.h>


class VRTracker
{
public:
    const char* name;
    Vector pos;
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
    Vector1,
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


bool            OVR_Enabled();
bool            OVR_IsHMDPresent();

int             OVR_Init();
int             OVR_Shutdown();
int             OVR_GetVersion();

int             OVR_SetActionManifest( const char* fileName );
int             OVR_TriggerHaptic( const char* actionName );

// void            VRMOD_SetActiveActionSets(const char* actionSetName, ...);
void            OVR_AddActiveActionSet(const char* actionSetName);
void            OVR_ResetActiveActionSets();
void            OVR_UpdatePosesAndActions();

void            OVR_GetPoses( CUtlVector< VRTracker* > &trackers );
void            OVR_GetActions( CUtlVector< VRBaseAction* > &actions );

void            OVR_Submit( void* submitData, vr::EVREye eye );
void            OVR_DX9EXToDX11( void* deviceData, void* leftEyeData, void* rightEyeData );

// VRBaseAction*   OVR_GetActions();

VRViewParams    OVR_GetViewParameters();

vr::HmdMatrix44_t OVR_GetProjectionMatrix( vr::EVREye eye, float nearZ, float farZ );



#endif