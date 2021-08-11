#pragma once

#include "vr_util.h"
#include "igamesystem.h"


enum class EVRTracker;


// basically copied from openvr.h
enum class EVRDeviceType
{
	Invalid = 0,			// the ID was not valid.
	Headset = 1,			// Head-Mounted Displays
	Controller = 2,			// Tracked controllers
	Tracker = 3,			// Generic trackers, similar to controllers
	BaseStation = 4,		// Camera and base stations that serve as tracking reference points
	// DisplayRedirect = 5,	// Accessories that aren't necessarily tracked themselves, but may redirect video output from other tracked devices
	Max
};


class VRDeviceType
{
public:
	VRDeviceType();
	~VRDeviceType();

	void            Init( const char* path );
	const char*	    GetTrackerModelName( EVRTracker tracker );
	void            SetPoseName( const char* name );

	// char            m_name[128];  // device name
	const char*     m_name;  // device name
	EVRDeviceType   m_type = EVRDeviceType::Invalid;

	CUtlMap< EVRTracker, const char* > m_trackerModels;
	// CUtlVector< const char* > m_trackerModels;
};


// TODO: probably rework and move the CVRBoneInfo stuff into this, but on the client only still?
// just for if people use the same model, we won't have duplicated bone info


class VRSystemShared : public CAutoGameSystem
{
public:
	VRSystemShared();
	~VRSystemShared();

	// ========================================
	// GameSystem Methods
	// ========================================
	virtual char const*         Name() { return "vr_system_shared"; }
	virtual bool                Init();
	// virtual void                LevelInitPostEntity();
	// virtual void                LevelShutdownPostEntity();

	// ========================================

	void                        LoadDeviceTypes();
	void                        CreateDeviceType( const char* path );
	VRDeviceType*               GetDeviceType( const char* name );

	// ========================================

	CUtlVector< VRDeviceType* > m_deviceTypes;
};


extern VRSystemShared g_VRShared;

