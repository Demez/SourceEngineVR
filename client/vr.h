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
	VMatrix mat;
	Vector pos;
	Vector vel;
	QAngle ang;
	QAngle angvel;
};


struct VRViewParams
{
	int width = 512;
	int height = 512;
	float fov = 0;
	float aspect = 0.0;
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


vr::EVREye      ToOVREye( VREye eye );

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

	VMatrix                     OVRToSrcCoords( const VMatrix& vortex, bool scale = true );
	bool                        GetEyeProjectionMatrix( VMatrix *pResult, VREye eEye, float zNear, float zFar, float fovScale );
	VMatrix                     GetMidEyeFromEye( VREye eEye );

	// ========================================
	// convenience functions
	// ========================================

	inline bool                 IsHMDPresent()          { return vr::VR_IsHmdPresent(); }

	inline VRHostTracker*       GetHeadset()            { return GetTrackerByName("hmd"); }
	inline VRHostTracker*       GetLeftController()     { return GetTrackerByName("pose_lefthand"); }
	inline VRHostTracker*       GetRightController()    { return GetTrackerByName("pose_righthand"); }

	inline VRSkeletonAction*    GetLeftHandSkeleton()   { return (VRSkeletonAction*)GetActionByName("skeleton_lefthand"); }
	inline VRSkeletonAction*    GetRightHandSkeleton()  { return (VRSkeletonAction*)GetActionByName("skeleton_righthand"); }

	// ========================================
	// Information
	// ========================================

	/*
    public float hmd_SecondsFromVsyncToPhotons { get { return GetFloatProperty(ETrackedDeviceProperty.Prop_SecondsFromVsyncToPhotons_Float); } }
    public float hmd_DisplayFrequency { get { return GetFloatProperty(ETrackedDeviceProperty.Prop_DisplayFrequency_Float); } }
	*/

	const char*                 GetTrackingPropString( vr::ETrackedDeviceProperty prop, uint deviceId = vr::k_unTrackedDeviceIndex_Hmd );

	const char*                 GetHeadsetTrackingSystemName();
	const char*                 GetHeadsetModelNumber();
	const char*                 GetHeadsetSerialNumber();
	const char*                 GetHeadsetType();

	float                       GetHeadsetSecondsFromVsyncToPhotons();
	float                       GetHeadsetDisplayFrequency();

	// ========================================
	// OpenVR Handling Stuff
	// ========================================

	bool                        Enable();
	bool                        Disable();

	void                        UpdateTrackers();
	void                        UpdateActions();

	VRHostTracker*              GetTrackerByName( const char* name );
	VRBaseAction*               GetActionByName( const char* name );

	void                        UpdateViewParams();
	VRViewParams                GetViewParams();
	void                        GetFOVOffset( VREye eye, float &aspectRatio, float &hFov );

	double                      GetScale();
	void                        SetScale( double newScale );
	void                        SetSeatedMode( bool seated );

	bool                        IsDX11();
	bool                        NeedD3DInit();
	void                        DX9EXToDX11( void* leftEyeData, void* rightEyeData );
	void                        InitDX9Device( void* deviceData );

	void                        Submit( ITexture* rtEye, VREye eye );
	void                        Submit( ITexture* leftEye, ITexture* rightEye );

public:
	bool active;
	VRViewParams m_currentViewParams;

	CUtlVector< VRHostTracker* > m_currentTrackers;
	CUtlVector< VRBaseAction* > m_currentActions;
	// CUtlVector< VRBaseAction* > previousActions;

	bool m_seatedMode;
	double m_scale;
};

extern VRSystem g_VR;

bool InVR();
bool InVRRender();

#endif
