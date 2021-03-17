#ifndef VR_SYSTEM_H
#define VR_SYSTEM_H
#pragma once

#include "mathlib/vector.h"

#include <d3d9.h>
#include <d3d11.h>

#ifdef GetObject
#undef GetObject
#endif

#ifdef CreateEvent
#undef CreateEvent
#endif

#include "vr_openvr.h"
#include "igamesystem.h"

// #include <materialsystem/imaterialsystem.h>
#include <materialsystem/itexture.h>

class ITexture;

extern ConVar vr_disable_eye_ang;


// why?
struct VRViewParams;


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


#define MAX_ACTIONS 64
#define MAX_ACTIONSETS 16


//openvr related
typedef struct {
	vr::VRActionHandle_t handle;
	char fullname[1024];
	char type[1024];
	char* name;
} action;

typedef struct {
	vr::VRActionSetHandle_t handle;
	char name[1024];
} actionSet;


vr::EVREye      ToOVREye( VREye eye );


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

	VRHostTracker*              GetTrackerByName( const char* name );
	VRBaseAction*               GetActionByName( const char* name );

	void                        UpdateViewParams();
	VRViewParams                GetViewParams();

	void                        UpdateTrackers();
	void                        UpdateActions();

	bool                        Enable();
	bool                        Disable();
	void                        SetupFOVOffset();

	bool                        NeedD3DInit();
	void                        Submit( ITexture* rtEye, VREye eye );
	// why do i need this defined in a header file for vr_viewrender.cpp
	void                        Submit( ITexture* leftEye, ITexture* rightEye );

	bool active;
	VRViewParams m_currentViewParams;

	CUtlVector< VRHostTracker* > m_currentTrackers;
	CUtlVector< VRBaseAction* > m_currentActions;
	// CUtlVector< VRBaseAction* > previousActions;


	// TODO: move into here later
	/*vr::IVRSystem*          m_pOpenVR = NULL;
	vr::IVRInput*           m_pInput = NULL;
	vr::TrackedDevicePose_t m_poses[vr::k_unMaxTrackedDeviceCount];
	actionSet               m_actionSets[MAX_ACTIONSETS];
	int                     m_actionSetCount = 0;
	vr::VRActiveActionSet_t m_activeActionSets[MAX_ACTIONSETS];
	int                     m_activeActionSetCount = 0;
	action                  m_savedActions[MAX_ACTIONS];
	int                     m_actionCount = 0;

	// directx
	ID3D11Device*           m_d3d11Device = NULL;
	ID3D11Texture2D*        m_d3d11TextureL = NULL;
	ID3D11Texture2D*        m_d3d11TextureR = NULL;
	HANDLE                  m_sharedTextureL = NULL;
	HANDLE                  m_sharedTextureR = NULL;
	IDirect3DDevice9*       m_d3d9Device = NULL;

	// other
	float                   m_horizontalFOVLeft = 0;
	float                   m_horizontalFOVRight = 0;
	float                   m_aspectRatioLeft = 0;
	float                   m_aspectRatioRight = 0;
	float                   m_horizontalOffsetLeft = 0;
	float                   m_horizontalOffsetRight = 0;
	float                   m_verticalOffsetLeft = 0;
	float                   m_verticalOffsetRight = 0;*/
};

extern VRSystem g_VR;

#endif
