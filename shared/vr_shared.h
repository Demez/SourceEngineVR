#pragma once

#include "vr_util.h"
#include "igamesystem.h"


enum class EVRTracker;


class VRDeviceType
{
public:
	VRDeviceType();
	~VRDeviceType();

	void            Init( const char* m_path );
	const char*	    GetTrackerModelName( EVRTracker tracker );

	// char            m_name[128];  // device name
	const char*     m_name;  // device name

							 // CUtlMap< const char*, const char* > m_trackerModels;
	CUtlVector< const char* > m_trackerModels;
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

