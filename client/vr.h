#ifndef VR_SYSTEM_H
#define VR_SYSTEM_H
#pragma once

#include "mathlib/vector.h"

#include <openvr.h>
#include "vr_util.h"
#include "vr_dxvk.h"
#include "igamesystem.h"

// #include <materialsystem/imaterialsystem.h>
#include <materialsystem/itexture.h>

class ITexture;
class VRDeviceType;
enum class EVRTracker;


class VRHostTracker
{
public:
	VRHostTracker();
	~VRHostTracker();

	const char* name;
	VMatrix mat;
	Vector pos;
	Vector vel;
	QAngle ang;
	QAngle angvel;
	bool valid;
	EVRTracker type;

	int actionIndex;
	uint32_t deviceIndex;

	VRDeviceType* device = nullptr;

	// inline const char*         GetModelName() { return device ? device->GetTrackerModelName(type) : ""; }
	const char*         GetModelName();

	/*inline bool         IsHeadset()     { return (type == EVRTracker::HMD); }
	inline bool         IsHand()        { return (type == EVRTracker::LHAND || type == EVRTracker::RHAND); }
	inline bool         IsFoot()        { return (type == EVRTracker::LFOOT || type == EVRTracker::RFOOT); }
	inline bool         IsHip()         { return (type == EVRTracker::HIP); }

	inline bool         IsLeftHand()    { return (type == EVRTracker::LHAND); }
	inline bool         IsRightHand()   { return (type == EVRTracker::RHAND); }
	inline bool         IsLeftFoot()    { return (type == EVRTracker::LFOOT); }
	inline bool         IsRightFoot()   { return (type == EVRTracker::RFOOT); }*/
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
	vr::VRActionHandle_t handle = 0;
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

	char*                       GetTrackingPropString( vr::ETrackedDeviceProperty prop, uint deviceId = vr::k_unTrackedDeviceIndex_Hmd );
	void                        GetTrackingPropString( char* value, int size, vr::ETrackedDeviceProperty prop, uint deviceId = vr::k_unTrackedDeviceIndex_Hmd );

	// ========================================
	// OpenVR Handling Stuff
	// ========================================

	bool                        Enable();
	bool                        Disable();

	VRHostTracker*              GetTracker( EVRTracker tracker );
	VRHostTracker*              GetTrackerByName( const char* name );
	VRBaseAction*               GetActionByName( const char* name );
	VRBaseAction*               GetActionByHandle( vr::VRActionHandle_t handle );

	VRViewParams                GetViewParams();
	void                        GetFOVOffset( VREye eye, float &aspectRatio, float &hFov );

	double                      GetScale();
	void                        SetScale( double newScale );
	void                        SetSeatedMode( bool seated );

	// delay and duration is in seconds, amplitude is 0.0 to 1.0, no clue how freq works
	void                        TriggerHapticFeedback( EVRTracker tracker, float amplitude = 0.75f, float duration = 1.0f, float freq = 1.0f, float delay = 0.0f );

	// ========================================
	// Lower Level OpenVR Handling Stuff
	// ========================================

	bool                        IsDX11();
	bool                        NeedD3DInit();
	void                        DX9EXToDX11( void* leftEyeData, void* rightEyeData );
	void                        InitDX9Device( void* deviceData );

	void                        Submit( ITexture* rtEye, VREye eye );
	void                        Submit( ITexture* leftEye, ITexture* rightEye );
	void                        WaitGetPoses();

// these are only private because they really should not be accessed outside of the VRSystem class
private:
	void                        StartThread();
	void                        StopThread();

	void                        UpdateTrackers();
	void                        UpdateActions();

	void                        UpdateViewParams();
	void                        UpdateDevices();

public:
	bool active;
	VRViewParams m_currentViewParams;

	CUtlVector< VRHostTracker* > m_currentTrackers;
	CUtlVector< VRBaseAction* > m_currentActions;
	// CUtlVector< VRBaseAction* > previousActions;

	// CUtlVector< VRDeviceType* > m_deviceTypes;
	CUtlMap< uint, vr::ETrackedDeviceClass > m_deviceClasses;

	bool m_seatedMode;
	double m_scale;
	bool m_inMap;
};

extern VRSystem g_VR;

bool InVR();
bool InVRRender();
bool InTestRender();

#endif
