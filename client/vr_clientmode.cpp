#include "cbase.h"
#include "vr_clientmode.h"
#include "vr_input.h"

// useless file

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


VRClientMode* GetVRClientMode()
{
	Assert( dynamic_cast< VRClientMode* >( GetClientModeNormal() ) );

	return static_cast< VRClientMode* >( GetClientModeNormal() );
}


VRClientMode::VRClientMode()
{
}


VRClientMode::~VRClientMode()
{
}


bool VRClientMode::CreateMove( float flInputSampleTime, CUserCmd *cmd )
{
	bool ret = BaseClass::CreateMove( flInputSampleTime, cmd );

	GetVRInput()->VRMove( flInputSampleTime, cmd );

	return ret;
}

