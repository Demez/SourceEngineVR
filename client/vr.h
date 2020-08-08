#ifndef VR_H
#define VR_H
#pragma once

#include "mathlib/vector.h"

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
	EyeLeft = 0,
	EyeRight
};

// Which texture is being requested in GetRenderTarget?
enum class VRRT
{
	Color = 0,
	Depth,
};


// a bit messy
class VRSystem : public CAutoGameSystemPerFrame
{
public:
	VRSystem();
	~VRSystem();

	// ========================================
	// GameSystem Methods
	// ========================================
	virtual char const*     Name() { return "vr_client"; }
	virtual bool            IsPerFrame()  { return true; }
	virtual void            Update( float frametime );
	virtual bool            Init();
	virtual void            LevelInitPostEntity();
	virtual void            LevelShutdownPostEntity();

	// ========================================
	// VRUtil Stuff
	// ========================================

	VRTracker*              GetTrackerByName( const char* name );
	VRBaseAction*           GetActionByName( const char* name );
	VRViewParams            GetViewParams();

	void                    UpdateTrackers();
	void                    UpdateActions();

	bool                    ClientStart();
	bool                    ClientExit();

	void                    NextFrame();

	// why do i need this defined in a header file for vr_viewrender.cpp
	void Submit( ITexture* leftEye, ITexture* rightEye )
	{
		#if ENGINE_QUIVER
			OVR_Submit( materials->VR_GetSubmitInfo( leftEye ), vr::EVREye::Eye_Left );
			OVR_Submit( materials->VR_GetSubmitInfo( rightEye ), vr::EVREye::Eye_Right );
		#else
			OVR_Submit( NULL, vr::EVREye::Eye_Left );
			OVR_Submit( NULL, vr::EVREye::Eye_Right );
		#endif
	}

	bool active;
	VRViewParams currentViewParams;

	CUtlVector< VRTracker* > trackers;
	CUtlVector< VRBaseAction* > actions;
	// CUtlVector< VRBaseAction* > previousActions;
};

extern VRSystem g_VR;

#endif
