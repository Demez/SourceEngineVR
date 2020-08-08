#include "cbase.h"

#include "vr_openvr.h"
#include "vr.h"

#include "usermessages.h"
#include "IGameUIFuncs.h"
#include "vgui_controls/Controls.h"
#include <vgui/ISurface.h>
#include "filesystem.h"
#include "igamesystem.h"
#include "rendertexture.h"
#include "materialsystem/itexture.h"

#include "view.h"
#include "viewrender.h"

#include <openvr.h>
#include <mathlib/vector.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar vr_enabled("vr_enabled", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);

VRSystem g_VR;


void vr_start_f() { g_VR.ClientStart(); }
void vr_exit_f()  { g_VR.ClientExit();  }

ConCommand vr_start("vr_start", vr_start_f);
ConCommand vr_exit("vr_exit", vr_exit_f);


CON_COMMAND_F(vr_list_trackers, "Lists all actions", 0)
{
	Msg("VR Trackers: \n");

	for ( int i = 0; i < g_VR.trackers.Count(); i++ )
	{
		Msg(" - %s\n", g_VR.trackers[i]->name);
	}
}

CON_COMMAND_F(vr_list_actions, "Lists all actions", 0)
{
	Msg("VR Actions: \n");

	for ( int i = 0; i < g_VR.actions.Count(); i++ )
	{
		Msg(" - %s\n", g_VR.actions[i]->name);
	}
}



VRSystem::VRSystem(): CAutoGameSystemPerFrame("vr_client")
{
}


VRSystem::~VRSystem()
{
}


bool VRSystem::Init()
{
	return true;
}


void VRSystem::Update( float frametime )
{
	if ( !active )
		return;

	currentViewParams = OVR_GetViewParameters();
	OVR_UpdatePosesAndActions();
	UpdateTrackers();
	UpdateActions();
}

void VRSystem::LevelInitPostEntity()
{
	if ( vr_enabled.GetBool() )
		ClientStart();
}

void VRSystem::LevelShutdownPostEntity()
{
	// what if this is a level transition? shutting down would probably be stupid, right?
	if ( active )
		ClientExit();
}


struct VRTracker* VRSystem::GetTrackerByName( const char* name )
{
	for ( int i = 0; i < trackers.Count(); i++ )
	{
		if ( V_strcmp(trackers[i]->name, name) == 0 )
		{
			return trackers[i];
		}
	}

	return NULL;
}

struct VRBaseAction* VRSystem::GetActionByName( const char* name )
{
	for ( int i = 0; i < actions.Count(); i++ )
	{
		if ( V_strcmp(actions[i]->name, name) == 0 )
		{
			return actions[i];
		}
	}

	return NULL;
}


void VRSystem::UpdateTrackers()
{
	trackers.PurgeAndDeleteElements();
	OVR_GetPoses( trackers );
}


void VRSystem::UpdateActions()
{
	actions.PurgeAndDeleteElements();
	OVR_GetActions( actions );
}


VRViewParams VRSystem::GetViewParams()
{
	// im guessing this could change between frames, plus we don't need to call openvr again multiple times per frame
	return currentViewParams;
}


bool VRSystem::ClientStart()
{
	// in case we're retrying after an error and shutdown wasn't called
	OVR_Shutdown();

	int result = OVR_Init();
	if (result != 0)
	{
		return false;
	}

	// copied right from gmod vr
	OVR_SetActionManifest("vr/vrmod_action_manifest.txt");

	OVR_ResetActiveActionSets();
	OVR_AddActiveActionSet("/actions/base");
	OVR_AddActiveActionSet("/actions/main");

	active = true;

	Update( 0.0 );
	return true;
}


bool VRSystem::ClientExit()
{
	active = false;
	OVR_Shutdown();
	return true;
}


