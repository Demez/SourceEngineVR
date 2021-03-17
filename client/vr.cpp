#include "cbase.h"

#include "vr_openvr.h"
#include "vr_util.h"
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

ConVar vr_autostart("vr_autostart", "0", FCVAR_ARCHIVE, "auto start vr on level load");

VRSystem g_VR;


// temporary
extern float g_horizontalFOVLeft;
extern float g_horizontalFOVRight;
extern float g_aspectRatioLeft;
extern float g_aspectRatioRight;


CON_COMMAND(vr_enable, "")   { g_VR.Enable(); }
CON_COMMAND(vr_disable, "")  { g_VR.Disable();  }


void ListTrackers()
{
	Msg("[VR] Current Trackers: \n");

	for ( int i = 0; i < g_VR.m_currentTrackers.Count(); i++ )
	{
		Msg(" - %s\n", g_VR.m_currentTrackers[i]->name);
	}
}


void ListActions()
{
	Msg("[VR] Available Actions: \n");

	for ( int i = 0; i < g_VR.m_currentActions.Count(); i++ )
	{
		Msg(" - %s\n", g_VR.m_currentActions[i]->name);
	}
}


ConCommand vr_list_trackers("vr_list_trackers", ListTrackers, "Lists all trackers");
ConCommand vr_list_actions("vr_list_actions", ListActions, "Lists all actions");


CON_COMMAND(vr_info, "Lists all information")
{
	Msg( "[VR] %s", g_pOpenVR->GetRuntimeVersion() );

	// Msg( "[VR] %s", g_pOpenVR->GetStringTrackedDeviceProperty() );

	ListTrackers();
	ListActions();
}


vr::EVREye ToOVREye( VREye eye )
{
	return (eye == VREye::Left) ? vr::EVREye::Eye_Left : vr::EVREye::Eye_Right;
}


bool VRSystem::GetEyeProjectionMatrix( VMatrix *pResult, VREye eEye, float zNear, float zFar, float fovScale )
{
	Assert ( pResult != NULL );
	if( !pResult || !g_pOpenVR || !active )
		return false;

	float fLeft, fRight, fTop, fBottom;
	g_pOpenVR->GetProjectionRaw( ToOVREye( eEye ), &fLeft, &fRight, &fTop, &fBottom );

	ComposeProjectionTransform( fLeft, fRight, fTop, fBottom, zNear, zFar, fovScale, pResult );
	return true;
}


VMatrix VRSystem::GetMidEyeFromEye( VREye eEye )
{
	if( g_pOpenVR )
	{
		vr::HmdMatrix34_t matMidEyeFromEye = g_pOpenVR->GetEyeToHeadTransform( ToOVREye( eEye ) );
		return OpenVRToSourceCoordinateSystem( VMatrixFrom34( matMidEyeFromEye.m ), false );
	}
	else
	{
		VMatrix mat;
		mat.Identity();
		return mat;
	}
}


VRSystem::VRSystem(): CAutoGameSystemPerFrame("vr_system")
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

	UpdateViewParams();
	OVR_UpdatePosesAndActions();
	UpdateTrackers();
	UpdateActions();
}


void VRSystem::LevelInitPostEntity()
{
	if ( vr_autostart.GetBool() )
		Enable();
}

void VRSystem::LevelShutdownPostEntity()
{
	// what if this is a level transition? shutting down would probably be stupid, right?
	if ( active )
		Disable();
}


struct VRHostTracker* VRSystem::GetTrackerByName( const char* name )
{
	for ( int i = 0; i < m_currentTrackers.Count(); i++ )
	{
		if ( V_strcmp(m_currentTrackers[i]->name, name) == 0 )
		{
			return m_currentTrackers[i];
		}
	}

	return NULL;
}

struct VRBaseAction* VRSystem::GetActionByName( const char* name )
{
	for ( int i = 0; i < m_currentActions.Count(); i++ )
	{
		if ( V_strcmp(m_currentActions[i]->name, name) == 0 )
		{
			return m_currentActions[i];
		}
	}

	return NULL;
}


void VRSystem::UpdateTrackers()
{
	m_currentTrackers.PurgeAndDeleteElements();
	OVR_GetPoses( m_currentTrackers );
}


void VRSystem::UpdateActions()
{
	m_currentActions.PurgeAndDeleteElements();
	OVR_GetActions( m_currentActions );
}


void VRSystem::UpdateViewParams()
{
	VRViewParams viewParams;
    viewParams.horizontalFOVLeft = g_horizontalFOVLeft;
    viewParams.horizontalFOVRight = g_horizontalFOVRight;
    viewParams.aspectRatioLeft = g_aspectRatioLeft;
    viewParams.aspectRatioRight = g_aspectRatioRight;

    uint32_t recommendedWidth = 0;
    uint32_t recommendedHeight = 0;
    g_pOpenVR->GetRecommendedRenderTargetSize(&recommendedWidth, &recommendedHeight);

    viewParams.rtWidth = recommendedWidth;
    viewParams.rtHeight = recommendedHeight;

    vr::HmdMatrix34_t eyeToHeadLeft = g_pOpenVR->GetEyeToHeadTransform(vr::Eye_Left);
    vr::HmdMatrix34_t eyeToHeadRight = g_pOpenVR->GetEyeToHeadTransform(vr::Eye_Right);
    Vector eyeToHeadTransformPos;
    eyeToHeadTransformPos.x = eyeToHeadLeft.m[0][3];
    eyeToHeadTransformPos.y = eyeToHeadLeft.m[1][3];
    eyeToHeadTransformPos.z = eyeToHeadLeft.m[2][3];

    viewParams.eyeToHeadTransformPosLeft = eyeToHeadTransformPos;

    eyeToHeadTransformPos.x = eyeToHeadRight.m[0][3];
    eyeToHeadTransformPos.y = eyeToHeadRight.m[1][3];
    eyeToHeadTransformPos.z = eyeToHeadRight.m[2][3];

    viewParams.eyeToHeadTransformPosRight = eyeToHeadTransformPos;

	m_currentViewParams = viewParams;
}


VRViewParams VRSystem::GetViewParams()
{
	return m_currentViewParams;
}


bool VRSystem::Enable()
{
	// in case we're retrying after an error and shutdown wasn't called
	Disable();

	vr::HmdError error = vr::VRInitError_None;

	g_pOpenVR = vr::VR_Init(&error, vr::VRApplication_Scene);
	if (error != vr::VRInitError_None)
	{
		// would be nice if i could print out the enum name
		Warning("[VR] VR_Init failed - Error Code %d\n", error);
		return false;
	}

	if (!vr::VRCompositor())
	{
		Warning("[VR] VRCompositor failed\n");
		return false;
	}

	SetupFOVOffset();

	// copied right from gmod vr
	OVR_SetActionManifest("vr/vrmod_action_manifest.txt");

	OVR_ResetActiveActionSets();
	OVR_AddActiveActionSet("/actions/base");
	OVR_AddActiveActionSet("/actions/main");

	active = true;

	Update( 0.0 );
	return true;
}


bool VRSystem::Disable()
{
	active = false;
	OVR_Shutdown();
	return true;
}


// TODO: move OVR_SetupFOVOffset into here
void VRSystem::SetupFOVOffset()
{
	OVR_SetupFOVOffset();
}


bool VRSystem::NeedD3DInit()
{
	return OVR_NeedD3DInit();
}


// TODO: move OVR_Submit into this
void VRSystem::Submit( ITexture* rtEye, VREye eye )
{
#if ENGINE_QUIVER || ENGINE_CSGO
	OVR_Submit( materials->VR_GetSubmitInfo( rtEye ), ToOVREye( eye ) );
#else
	// TODO: use the hack gmod vr uses
	OVR_Submit( NULL, ToOVREye( eye ) );
#endif
}


void VRSystem::Submit( ITexture* leftEye, ITexture* rightEye )
{
	Submit( leftEye, VREye::Left );
	Submit( rightEye, VREye::Right );
}


