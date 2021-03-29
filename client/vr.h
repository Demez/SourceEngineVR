#ifndef VR_SYSTEM_H
#define VR_SYSTEM_H
#pragma once

#include "mathlib/vector.h"

#include <openvr.h>
#include "vr_util.h"
#include "igamesystem.h"

// #include <materialsystem/imaterialsystem.h>
#include <materialsystem/itexture.h>

class ITexture;


struct VRHostTracker
{
	const char* name;

	VMatrix mat;        // converted to source's coordinate system
	Vector pos;
	Vector vel;
	QAngle ang;
	QAngle angvel;
};


struct VREyeViewParams
{
	int hFOV;
	float hOffset;
	float vOffset;
	Vector eyeToHeadTransformPos;
};


struct VRViewParams
{
	VREyeViewParams left;
	VREyeViewParams right;

	float aspectRatio;

	int rtWidth = 512;
	int rtHeight = 512;
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



// why?
struct VRViewParams;
struct OVRDX11;


enum class VREye
{
	Left = 0,
	Right
};


enum class VRActionSet
{
	MAIN = 0,
	DRIVING
};


// maybe use in the future?
enum class VRInterface
{
	OPENVR = 0,
	OPENXR
};


vr::EVREye      ToOVREye( VREye eye );

// used in vr_viewrender.cpp still
void            OVR_InitDX9Device( void* deviceData );
void            OVR_DX9EXToDX11( void* leftEyeData, void* rightEyeData );

extern vr::IVRSystem* g_pOVR;


class VRSystem : public CAutoGameSystemPerFrame
{
public:
	VRSystem();
	~VRSystem();

	// ========================================
	// GameSystem Methods
	// ========================================
	virtual char const*         Name() { return "vr_system"; }
	virtual void                Update( float frametime );
	virtual bool                Init();
	virtual void                LevelInitPostEntity();
	virtual void                LevelShutdownPostEntity();

	// ========================================
	// Utils
	// ========================================

	bool                        GetEyeProjectionMatrix( VMatrix *pResult, VREye eEye, float zNear, float zFar, float fovScale );
	VMatrix                     GetMidEyeFromEye( VREye eEye );

	// ========================================
	// OpenVR Handling Stuff
	// ========================================

	// convenience functions
	inline bool                 IsHMDPresent()          { return vr::VR_IsHmdPresent(); }

	inline VRHostTracker*       GetHeadset()            { return GetTrackerByName("hmd"); }
	inline VRHostTracker*       GetLeftController()     { return GetTrackerByName("pose_lefthand"); }
	inline VRHostTracker*       GetRightController()    { return GetTrackerByName("pose_righthand"); }

	inline VRSkeletonAction*    GetLeftHandSkeleton()   { return (VRSkeletonAction*)GetActionByName("skeleton_lefthand"); }
	inline VRSkeletonAction*    GetRightHandSkeleton()  { return (VRSkeletonAction*)GetActionByName("skeleton_righthand"); }

	VRHostTracker*              GetTrackerByName( const char* name );
	VRBaseAction*               GetActionByName( const char* name );

	void                        UpdateBaseViewParams();
	void                        UpdateViewParams();
	VRViewParams                GetViewParams();
	VREyeViewParams             GetEyeViewParams( VREye eye );

	void                        UpdateTrackers();
	void                        UpdateActions();

	bool                        Enable();
	bool                        Disable();
	void                        SetupFOVOffset();
	void                        GetFOVOffset( VREye eye, int &hFov, float &hOffset, float &vOffset, float &aspectRatio );

	bool                        NeedD3DInit();

	void                        Submit( ITexture* rtEye, VREye eye );
	void                        Submit( ITexture* leftEye, ITexture* rightEye );

private:
	void                        SubmitInternal( void* submitData, vr::EVREye eye, vr::VRTextureBounds_t &textureBounds );

public:
	bool active;
	VRViewParams m_baseViewParams;
	VRViewParams m_currentViewParams;

	CUtlVector< VRHostTracker* > m_currentTrackers;
	CUtlVector< VRBaseAction* > m_currentActions;
	// CUtlVector< VRBaseAction* > previousActions;
};

extern VRSystem g_VR;

#endif
