#pragma once

#include "clientmode_shared.h"
#include "engine_defines.h"


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class VRClientMode : public ClientModeShared
{
public:
	DECLARE_CLASS( VRClientMode, ClientModeShared );

	VRClientMode();
	~VRClientMode();

	virtual bool CreateMove( float flInputSampleTime, CUserCmd *cmd );
};


extern VRClientMode* GetVRClientMode();

