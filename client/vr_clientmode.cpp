//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "ivmodemanager.h"
#include "vr_clientmode.h"
#include "panelmetaclassmgr.h"
#include "iinput.h"
#include "vr_input.h"

#if ENGINE_NEW
#include "c_gameinstructor.h"
#include "c_baselesson.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


CVRModeManager::CVRModeManager( void )
{
}

CVRModeManager::~CVRModeManager( void )
{
}

void CVRModeManager::CreateMove( float flInputSampleTime, CUserCmd *cmd )
{
	CVRInput* vr_input = (CVRInput*)input;
	vr_input->VRMove( flInputSampleTime, cmd );
}

