#include "cbase.h"

#include "vr_util.h"
#include "vr.h"
#include "vr_internal.h"
#include "vr_renderer.h"
#include "vr_ik.h"
#include "vr_cl_player.h"
#include "vr_shared.h"

#include <KeyValues.h>
#include "IGameUIFuncs.h"
#include "vgui_controls/Controls.h"
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include "filesystem.h"
#include "igamesystem.h"
#include "rendertexture.h"
#include "materialsystem/itexture.h"
#include "inputsystem/iinputsystem.h"
#include "tier0/ICommandLine.h"

#include "view.h"
#include "viewrender.h"

#include <openvr.h>
#include <mathlib/vector.h>

#include <d3d9.h>
#include <d3d11.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar vr_autostart("vr_autostart", "0", FCVAR_ARCHIVE, "auto start vr on level load");
ConVar vr_mainmenu("vr_mainmenu", "1", FCVAR_ARCHIVE);
ConVar vr_renderthread("vr_renderthread", "0", FCVAR_ARCHIVE, "use a separate thread for rendering in vr on the main menu or while loading a level");
ConVar vr_active_hack("vr_active_hack", "0", FCVAR_CLIENTDLL, "lazy hack for anything that needs to know if vr is enabled outside of the game dlls (mouse lock)");

ConVar vr_clamp_res("vr_clamp_res", "1", FCVAR_CLIENTDLL, "clamp the resolution to the screen size until the render clamping issue is figured out");
ConVar vr_scale_override("vr_scale_override", "42.5");  // anything lower than 0.25 doesn't look right in the headset
ConVar vr_dbg_rt_res("vr_dbg_rt_res", "2048", FCVAR_CLIENTDLL);
ConVar vr_eye_height("vr_eye_h", "0", FCVAR_CLIENTDLL, "Override the render target height, 0 to disable");
ConVar vr_eye_width("vr_eye_w", "0", FCVAR_CLIENTDLL, "Override the render target width, 0 to disable");

ConVar vr_waitgetposes_test("vr_waitgetposes_test", "0", FCVAR_CLIENTDLL, "testing where WaitGetPoses is called");

extern ConVar vr_dbg_rt_test;
extern ConVar vr_scale;

#if ENGINE_NEW
extern ConVar mat_postprocess_enable;
#endif


VRSystem                    g_VR;
extern VRSystemInternal     g_VRInt;

vr::IVRSystem*              g_pOVR = NULL;
vr::IVRInput*               g_pOVRInput = NULL;

const double DEFAULT_VR_SCALE = 39.37012415030996;

bool g_VRSupported;

CON_COMMAND(vr_enable, "")   { g_VR.Enable(); }
CON_COMMAND(vr_disable, "")  { g_VR.Disable();  }


void CC_SwapHands( IConVar *var, const char *pOldValue, float flOldValue );

ConVar vr_lefthand("vr_lefthand", "0", FCVAR_ARCHIVE, "", CC_SwapHands);

void CC_SwapHands( IConVar *var, const char *pOldValue, float flOldValue )
{
    if ( g_pOVRInput )
        g_pOVRInput->SetDominantHand( vr_lefthand.GetBool() ? vr::TrackedControllerRole_LeftHand : vr::TrackedControllerRole_RightHand );
}


// TODO:
//  use ShowKeyboard somehow


void ListTrackers()
{
	if ( !g_VR.active )
	{
		Msg("VR is not enabled!\n");
		return;
	}

	Msg("Current Trackers: \n");

	for ( int i = 0; i < g_VR.m_currentTrackers.Count(); i++ )
	{
		Msg(" - %s\n", g_VR.m_currentTrackers[i]->name);
	}
}


void ListActions()
{
	if ( !g_VR.active )
	{
		Msg("VR is not enabled!\n");
		return;
	}

	Msg("Available Actions: \n");

	for ( int i = 0; i < g_VR.m_currentActions.Count(); i++ )
	{
		Msg(" - %s\n", g_VR.m_currentActions[i]->name);
	}
}


/*CON_COMMAND( vr_list_trackers, "Lists all trackers" )
{
    Msg("[VR] ");
    ListTrackers();
}

CON_COMMAND( vr_list_actions, "Lists all actions" )
{
    Msg("[VR] ");
    ListActions();
}*/


ConCommand vr_list_trackers("vr_list_trackers", ListTrackers, "Lists all trackers");
ConCommand vr_list_actions("vr_list_actions", ListActions, "Lists all actions");


CON_COMMAND(vr_info, "Lists all information")
{
	if ( !g_VR.active )
	{
		Msg("[VR] VR is not enabled\n");
		return;
	}

	Msg( "SteamVR Runtime Version: %s\n", g_pOVR->GetRuntimeVersion() );

    Msg( "Headset Tracking System: %s\n", g_VR.GetHeadsetTrackingSystemName() );
    Msg( "Headset Model Number: %s\n", g_VR.GetHeadsetModelNumber() );
    Msg( "Headset Serial Number: %s\n", g_VR.GetHeadsetSerialNumber() );
    Msg( "Headset Type: %s\n", g_VR.GetHeadsetType() );

	ListTrackers();
	ListActions();
}


vr::EVREye ToOVREye( VREye eye )
{
	return (eye == VREye::Left) ? vr::EVREye::Eye_Left : vr::EVREye::Eye_Right;
}


bool InVR()
{
    return g_VR.active;
}


// -----------------------------------------------------------------------------


VRHostTracker::VRHostTracker()
{
    device = nullptr;
}

VRHostTracker::~VRHostTracker()
{
}


// DEMEZ: TEMPORARY PLEASE CHANGE
ConVar vr_devicehack("vr_devicehack", "0", FCVAR_ARCHIVE, "0 - oculus_cv1, 1 - valve_index");

const char* VRHostTracker::GetModelName()
{
    if ( !device )
    {
        // DEMEZ: TEMPORARY PLEASE CHANGE
        // i just need to figure out how to get the device type from openvr, and then i can do this properly
        // i could probably set this up in usercmd now, and move the hack there
        // just adding a hack convar in for now for a portal vr demo
        device = g_VRShared.GetDeviceType( vr_devicehack.GetInt() == 1 ? "valve_index" : "oculus_cv1" );
    }

    if ( device == nullptr )
    {
        Warning("[VR] No Device Type for tracker \"%s\"", name);
        return "";
    }

    return device->GetTrackerModelName(type);
}


// -----------------------------------------------------------------------------


/*
public float hmd_SecondsFromVsyncToPhotons { get { return GetFloatProperty(ETrackedDeviceProperty.Prop_SecondsFromVsyncToPhotons_Float); } }
public float hmd_DisplayFrequency { get { return GetFloatProperty(ETrackedDeviceProperty.Prop_DisplayFrequency_Float); } }
*/

// this actually works just fine so
#pragma warning( push )
#pragma warning( disable : 4172 )

const char* VRSystem::GetTrackingPropString( vr::ETrackedDeviceProperty prop, uint deviceId )
{
    vr::ETrackedPropertyError error = vr::TrackedProp_Success;
    uint32_t capacity = g_pOVR->GetStringTrackedDeviceProperty( deviceId, prop, NULL, 0, &error );

    if ( capacity > 1 )
    {
        char value[256];
        g_pOVR->GetStringTrackedDeviceProperty( vr::k_unTrackedDeviceIndex_Hmd, prop, value, 256, &error );
        return value;
    }

    char value[256];
    V_snprintf( value, 256, "[ERROR - %s]", g_pOVR->GetPropErrorNameFromEnum( error ) );
    return value;
}

#pragma warning( pop )

const char* VRSystem::GetHeadsetTrackingSystemName()
{
    return GetTrackingPropString( vr::Prop_TrackingSystemName_String );
}
const char* VRSystem::GetHeadsetModelNumber()
{
    return GetTrackingPropString( vr::Prop_ModelNumber_String );
}
const char* VRSystem::GetHeadsetSerialNumber()
{
    return GetTrackingPropString( vr::Prop_SerialNumber_String );
}
const char* VRSystem::GetHeadsetType()
{
    return GetTrackingPropString( vr::Prop_ControllerType_String );
}


bool VRSystem::GetEyeProjectionMatrix( VMatrix *pResult, VREye eEye, float zNear, float zFar, float fovScale )
{
	Assert ( pResult != NULL );
	if( !pResult || !g_pOVR || !active )
		return false;

	float fLeft, fRight, fTop, fBottom;
    g_pOVR->GetProjectionRaw( ToOVREye( eEye ), &fLeft, &fRight, &fTop, &fBottom );

	ComposeProjectionTransform( fLeft, fRight, fTop, fBottom, zNear, zFar, fovScale, pResult );
	return true;
}


VMatrix VRSystem::OVRToSrcCoords( const VMatrix& vortex, bool scale )
{
    // const float inchesPerMeter = (float)(39.3700787);

    // 10.0f/0.254f;
    // 39.37012415030996
    // float inchesPerMeter = (float)(39.3700787);
    // double inchesPerMeter = (double)(39.37012415030996);
    double inchesPerMeter = m_scale;

    if ( vr_scale_override.GetFloat() > 0 )
        inchesPerMeter = vr_scale_override.GetFloat();

    if ( scale )
        inchesPerMeter *= vr_scale.GetFloat();

    // From Vortex: X=right, Y=up, Z=backwards, scale is meters.
    // To Source: X=forwards, Y=left, Z=up, scale is inches.
    //
    // s_from_v = [ 0 0 -1 0
    //             -1 0 0 0
    //              0 1 0 0
    //              0 0 0 1];
    //
    // We want to compute vmatrix = s_from_v * vortex * v_from_s; v_from_s = s_from_v'
    // Given vortex =
    // [00    01    02    03
    //  10    11    12    13
    //  20    21    22    23
    //  30    31    32    33]
    //
    // s_from_v * vortex * s_from_v' =
    //  22    20   -21   -23
    //  02    00   -01   -03
    // -12   -10    11    13
    // -32   -30    31    33
    //
    const vec_t (*v)[4] = vortex.m;
    VMatrix result(
        v[2][2],  v[2][0], -v[2][1], -v[2][3] * inchesPerMeter,
        v[0][2],  v[0][0], -v[0][1], -v[0][3] * inchesPerMeter,
        -v[1][2], -v[1][0],  v[1][1],  v[1][3] * inchesPerMeter,
        -v[3][2], -v[3][0],  v[3][1],  v[3][3]);

    return result;
}


VMatrix VRSystem::GetMidEyeFromEye( VREye eEye )
{
	if( g_pOVR )
	{
		vr::HmdMatrix34_t matMidEyeFromEye = g_pOVR->GetEyeToHeadTransform( ToOVREye( eEye ) );
		return OVRToSrcCoords( VMatrixFrom34( matMidEyeFromEye.m ), true );
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
    m_inMap = false;
}


VRSystem::~VRSystem()
{
}


bool VRSystem::Init()
{
#if ENGINE_ASW
    if ( CommandLine()->FindParm("-vrapi") )
    {
        if ( !g_VRInt.InitShaderAPI() )
        {
            Warning("[VR] Failed to init shaderapi\n");
            // return false;
        }
        else
        {
            g_VRSupported = true;
        }

        engine->ClientCmd("exec vr\n");
    }
    else
    {
        g_VRSupported = false;
    }
#elif ENGINE_CSGO || ENGINE_QUIVER
    g_VRSupported = true;
#endif

    g_VRRenderer.Init();

    if ( vr_autostart.GetBool() && vr_mainmenu.GetBool() )
        Enable();

	return true;
}


void VRSystem::StartThread()
{
    if ( g_VRRenderThread == NULL )
    {
        g_VRRenderThread = new CVRRenderThread;
        g_VRRenderThread->SetName( "CVRRenderThread" );
    }

    g_VRRenderThread->shouldStop = false;

#if ENGINE_CSGO
    g_VRRenderThread->Start( 1024, TP_PRIORITY_HIGHEST );
#else
    g_VRRenderThread->Start( 1024 );
    g_VRRenderThread->SetPriority( THREAD_PRIORITY_HIGHEST );
#endif
}


void VRSystem::StopThread()
{
    if ( g_VRRenderThread )
    {
        g_VRRenderThread->shouldStop = true;
    }
}


void VRSystem::Update( float frametime )
{
	if ( !active )
    {
        if ( vr_dbg_rt_test.GetBool() )
            UpdateViewParams();

        return;
    }

    // DEMEZ TODO: should probably be moved to viewrender?
    if ( !vr_waitgetposes_test.GetBool() )
        WaitGetPoses();

    if ( m_scale <= 0.0f )
        m_scale = DEFAULT_VR_SCALE;

    // hmmmm
    if ( !vr_active_hack.GetBool() )
        vr_active_hack.SetValue("1");

    if ( !m_inMap )
        g_VRRenderer.Render();
}


void VRSystem::WaitGetPoses()
{
	UpdateViewParams();
    g_VRInt.WaitGetPoses();
	UpdateTrackers();
	UpdateActions();
}


void VRSystem::LevelInitPostEntity()
{
    m_inMap = true;

	if ( vr_autostart.GetBool() && !vr_renderthread.GetBool() && !active )
    {
        Enable();
    }

    if ( vr_renderthread.GetBool() )
        StopThread();
}

void VRSystem::LevelShutdownPostEntity()
{
	// what if this is a level transition? shutting down would probably be stupid, right?
	if ( active )
    {
        if ( vr_renderthread.GetBool() )
            StartThread();
        else
            Disable();
    }

    m_inMap = false;
}


VRHostTracker* VRSystem::GetTracker( EVRTracker tracker )
{
	for ( int i = 0; i < m_currentTrackers.Count(); i++ )
	{
        /*if ( !m_currentTrackers[i]->valid )
        {
            continue;
        }*/

		if ( m_currentTrackers[i]->type == tracker )
		{
			return m_currentTrackers[i];
		}
	}

	return NULL;
}


struct VRHostTracker* VRSystem::GetTrackerByName( const char* name )
{
	for ( int i = 0; i < m_currentTrackers.Count(); i++ )
	{
        /*if ( !m_currentTrackers[i]->valid )
        {
            continue;
        }*/

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


extern EVRTracker GetTrackerEnum( const char* name );


void VRSystem::UpdateTrackers()
{
	// m_currentTrackers.PurgeAndDeleteElements();

    CUtlVector< VRHostTracker* > tmpTrackers;

    /*for ( int i = 0; i < m_currentTrackers.Count(); i++ )
    {
        // m_currentTrackers[i]->valid = false;
        tmpTrackers[i] = m_currentTrackers[i];
    }*/

    // m_currentTrackers.Purge();

    vr::InputPoseActionData_t poseActionData;
    vr::TrackedDevicePose_t pose;
    char poseName[MAX_STR_LEN];
    vr::ETrackingUniverseOrigin trackingType = m_seatedMode ? vr::TrackingUniverseStanding : vr::TrackingUniverseStanding;

    for (int i = -1; i < g_VRInt.actionCount; i++)
    {
        poseActionData.pose.bPoseIsValid = 0;
        pose.bPoseIsValid = 0;

        if (i == -1)
        {
            pose = g_VRInt.poses[0];
            memcpy(poseName, "hmd", 4);
        }
        else if (strcmp(g_VRInt.actions[i].type, "pose") == 0)
        {
            g_pOVRInput->GetPoseActionDataForNextFrame(g_VRInt.actions[i].handle, trackingType, &poseActionData, sizeof(poseActionData), vr::k_ulInvalidInputValueHandle);
            pose = poseActionData.pose;
            strcpy_s(poseName, MAX_STR_LEN, g_VRInt.actions[i].name);
        }
        else
        {
            continue;
        }

        if (pose.bPoseIsValid)
        {
            vr::HmdMatrix34_t mat = pose.mDeviceToAbsoluteTracking;

            struct VRHostTracker* tracker = GetTrackerByName( poseName );
            if ( tracker == NULL )
            {
                // tracker = (struct VRHostTracker*) malloc(sizeof(struct VRHostTracker));
                tracker = new VRHostTracker;
                tracker->name = strdup(poseName);
                tracker->type = GetTrackerEnum( poseName );
                // tracker->device = GetDeviceType( tracker->type );
                // m_allTrackers.AddToTail( tracker );
            }

            tracker->mat = OVRToSrcCoords( VMatrixFrom34( mat.m ) );

            MatrixGetColumn( tracker->mat.As3x4(), 3, tracker->pos );
            MatrixToAngles( tracker->mat, tracker->ang );

            tracker->vel.x = -pose.vVelocity.v[2];
            tracker->vel.y = -pose.vVelocity.v[0];
            tracker->vel.z = pose.vVelocity.v[1];

            tracker->angvel.x = -pose.vAngularVelocity.v[2] * (180.0 / 3.141592654);
            tracker->angvel.y = -pose.vAngularVelocity.v[0] * (180.0 / 3.141592654);
            tracker->angvel.z = pose.vAngularVelocity.v[1] * (180.0 / 3.141592654);

            // tracker->valid = true;
            tmpTrackers.AddToTail( tracker );
        }
    }

    for ( int i = 0; i < m_currentTrackers.Count(); i++ )
    {
        bool foundMatch = false;
        for ( int j = 0; j < tmpTrackers.Count(); j++ )
        {
            // if ( V_strcmp( m_currentTrackers[i]->name, tmpTrackers[j]->name ) != 0 )
            if ( m_currentTrackers[i]->type == tmpTrackers[j]->type )
            {
                foundMatch = true;
                break;
            }
        }

        // deleted tracker, free it
        if ( !foundMatch )
        {
            // free(m_currentTrackers[i]);
            delete m_currentTrackers[i];
        }
    }

    m_currentTrackers.Purge();
    m_currentTrackers.CopyArray( tmpTrackers.Base(), tmpTrackers.Count() );

    // m_allTrackers.Purge();
    // m_allTrackers.CopyArray( tmpTrackers.Base(), tmpTrackers.Count() );

    /*for ( int i = 0; i < m_currentTrackers.Count(); i++ )
    {
        // m_currentTrackers[i]->valid = false;
        tmpTrackers[i] = m_currentTrackers[i];
    }*/
}


void VRSystem::UpdateActions()
{
	m_currentActions.PurgeAndDeleteElements();

    vr::InputDigitalActionData_t digitalActionData;
    vr::InputAnalogActionData_t analogActionData;
    vr::VRSkeletalSummaryData_t skeletalSummaryData;

    struct VRBaseAction* prevAction = NULL;

    for (int i = 0; i < g_VRInt.actionCount; i++)
    {
        VRBaseAction* currentAction = NULL; 
        savedAction action = g_VRInt.actions[i];

        // this is probably pretty slow and stupid
        // allocating memory and freeing it every frame

        if (strcmp(action.type, "boolean") == 0)
        {
            bool value = (g_pOVRInput->GetDigitalActionData(action.handle, &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && digitalActionData.bState);

            VRBoolAction* tempAction = new VRBoolAction;
            tempAction->value = value;

            currentAction = tempAction;
        }
        else if (strcmp(action.type, "vector1") == 0)
        {
            g_pOVRInput->GetAnalogActionData(action.handle, &analogActionData, sizeof(analogActionData), vr::k_ulInvalidInputValueHandle);

            VRVector1Action* tempAction = new VRVector1Action;
            tempAction->value = analogActionData.x;

            currentAction = tempAction;
        }
        else if (strcmp(action.type, "vector2") == 0)
        {
            g_pOVRInput->GetAnalogActionData(action.handle, &analogActionData, sizeof(analogActionData), vr::k_ulInvalidInputValueHandle);
            VRVector2Action* tempAction = new VRVector2Action;
            tempAction->x = analogActionData.x;
            tempAction->y = analogActionData.y;

            currentAction = tempAction;
        }
        else if (strcmp(action.type, "skeleton") == 0)
        {
            // VRSkeletonAction* tempAction = (struct VRSkeletonAction*) malloc(sizeof(struct VRSkeletonAction));
            VRSkeletonAction* tempAction = new VRSkeletonAction;

            g_pOVRInput->GetSkeletalSummaryData(action.handle, vr::VRSummaryType_FromAnimation, &skeletalSummaryData);

            for (int j = 0; j < 5; j++)
            {
                tempAction->fingerCurls[j].x = j + 1;
                tempAction->fingerCurls[j].y = skeletalSummaryData.flFingerCurl[j];
            }

            currentAction = tempAction;
        }

        if ( currentAction != NULL )
        {
            currentAction->name = strdup(action.name);
            m_currentActions.AddToTail( &(*currentAction) );
        }
    }
}


void VRSystem::GetFOVOffset( VREye eye, float &aspectRatio, float &hFov )
{
	vr::HmdMatrix44_t proj = g_pOVR->GetProjectionMatrix( ToOVREye(eye), 1, 10 );
	float xscale = proj.m[0][0];
	float xoffset = proj.m[0][2];
	float yscale = proj.m[1][1];
	float yoffset = proj.m[1][2];
	float tan_px = fabsf((1.0f - xoffset) / xscale);
	float tan_nx = fabsf((-1.0f - xoffset) / xscale);
	float tan_py = fabsf((1.0f - yoffset) / yscale);
	float tan_ny = fabsf((-1.0f - yoffset) / yscale);
	float w = tan_px + tan_nx;
	float h = tan_py + tan_ny;

	// hFov = atan(w / 2.0f) * 180 / 3.141592654 * 2;
	hFov = atan(w / 2.0f) * 180 / 3.141592654 * 2;
    hFov = RAD2DEG(2.0f * atan(h / 2.0f));
	//g_verticalFOV = atan(h / 2.0f) * 180 / 3.141592654 * 2;
	// aspectRatio = w / h;
}


void VRSystem::UpdateViewParams()
{
	VRViewParams viewParams;

    uint32_t width = 0;
    uint32_t height = 0;

    if ( active )
    {
        g_pOVR->GetRecommendedRenderTargetSize(&width, &height);
    }
    else
    {
        width = vr_dbg_rt_res.GetInt();
        height = vr_dbg_rt_res.GetInt();
    }

    // not needed anymore
    /*if ( vr_clamp_res.GetBool() )
    {
        int scrWidth, scrHeight;
        vgui::surface()->GetScreenSize( scrWidth, scrHeight );

        if ( scrWidth < width || scrHeight < height )
        {
            float widthRatio = (float)scrWidth / (float)width;
            float heightRatio = (float)scrHeight / (float)height;
            float bestRatio = MIN(widthRatio, heightRatio);

            height *= bestRatio;
            width *= bestRatio;

            if ( width % 2 != 0 )
            {
                width += 1;
            }
        }
    }*/

    if ( vr_eye_height.GetFloat() > 0 )
        viewParams.height = vr_eye_height.GetFloat();

    if ( vr_eye_width.GetFloat() > 0 )
        viewParams.width = vr_eye_width.GetFloat();

    if ( vr_eye_width.GetFloat() > 0 || vr_eye_height.GetFloat() > 0 )
        viewParams.aspect = (float)viewParams.width / (float)viewParams.height;

    viewParams.width = width;
    viewParams.height = height;

    if ( active )
    {
        // uhh
        g_VRInt.CalcTextureBounds( viewParams.aspect, viewParams.fov );
        GetFOVOffset( VREye::Left, viewParams.aspect, viewParams.fov );
    }

    // viewParams.aspect = (float)viewParams.width / (float)viewParams.height;

	m_currentViewParams = viewParams;
}


VRViewParams VRSystem::GetViewParams()
{
    if ( /*m_currentViewParams.fov == 0.0 ||*/ m_currentViewParams.aspect == 0.0 )
        UpdateViewParams();

	return m_currentViewParams;
}


static bool g_wasPostProcessingOn = false;


bool VRSystem::Enable()
{
	// in case we're retrying after an error and shutdown wasn't called
	Disable();

#if ENGINE_ASW
    if ( !CommandLine()->FindParm("-vrapi") )
    {
        Warning("[VR] did not load shaderapi (run with -vrapi)\n");
        return false;
    }
    else if ( g_pShaderAPI == NULL )
    {
        Warning("[VR] shaderapi failed to load earlier (somehow)\n");
        return false;
    }
#endif

	vr::HmdError error = vr::VRInitError_None;

	g_pOVR = vr::VR_Init(&error, vr::VRApplication_Scene);
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

	UpdateViewParams();

	g_VRInt.SetActionManifest("vr/vrmod_action_manifest.txt");
    g_VRInt.ResetActiveActionSets();
    g_VRInt.AddActiveActionSet("/actions/base");
    g_VRInt.AddActiveActionSet("/actions/main");

    g_pOVRInput->SetDominantHand( vr_lefthand.GetBool() ? vr::TrackedControllerRole_LeftHand : vr::TrackedControllerRole_RightHand );

    g_VRIK.Init();

    // g_VRShared.LoadDeviceTypes();
    // LoadDeviceTypes( "VR" );
    // LoadDeviceTypes( "MOD" );
    // LoadDeviceTypes( "GAME" );

    /*if ( m_deviceType == nullptr )
    {
        // load unknown device type
        m_deviceType = GetDeviceType( "unknown" );

        if ( m_deviceType == nullptr )
        {
            // technically not mandatory at the moment, so i don't need to really disable it, but this is easier
            Warning( "[VR] ERROR: no valid device type (not even unknown type!!!)\n" );
            Disable();
            return false;
        }
    }*/

	active = true;
    m_scale = DEFAULT_VR_SCALE;

    // convienence
#if ENGINE_CSGO
    engine->ClientCmd_Unrestricted("engine_no_focus_sleep 0");
#endif

	// DEMEZ TODO: not require prediction in singleplayer, this is awful
    engine->ClientCmd_Unrestricted("cl_localnetworkbackdoor 0");
    engine->ClientCmd_Unrestricted("cl_predict 1");
    vr_active_hack.SetValue("1");

#if ENGINE_NEW
    g_wasPostProcessingOn = mat_postprocess_enable.GetBool();
    mat_postprocess_enable.SetValue("0");
#endif

    /*C_VRBasePlayer* pPlayer = (C_VRBasePlayer*)C_BasePlayer::GetLocalPlayer();
    if ( pPlayer )
    {
        pPlayer->OnVREnabled();
    }*/

    if ( !m_inMap && vr_renderthread.GetBool() )
        StartThread();

	Update( 0.0 );
	return true;
}


bool VRSystem::Disable()
{
    if ( !m_inMap && vr_renderthread.GetBool() )
        StopThread();

	active = false;

    if (g_pOVR != NULL)
    {
        vr::VR_Shutdown();
        g_pOVR = NULL;
    }

    if (g_VRInt.d3d11Device != NULL)
    {
        g_VRInt.d3d11Device->Release();
        g_VRInt.d3d11Device = NULL;
    }

    g_VRInt.d3d11TextureL = NULL;
    g_VRInt.d3d11TextureR = NULL;
    g_VRInt.sharedTextureL = NULL;
    g_VRInt.sharedTextureR = NULL;
    g_VRInt.d3d9Device = NULL;

    g_VRInt.actionCount = 0;
    g_VRInt.actionSetCount = 0;
    g_VRInt.activeActionSetCount = 0;

    // i would save the old value, but nobody changes this anyway
#if ENGINE_CSGO
    engine->ClientCmd_Unrestricted("engine_no_focus_sleep 50");
#endif

    vr_active_hack.SetValue("0");
    
#if ENGINE_NEW
    mat_postprocess_enable.SetValue( g_wasPostProcessingOn );
#endif

    m_currentActions.PurgeAndDeleteElements();
    // m_allTrackers.PurgeAndDeleteElements();
    m_currentTrackers.Purge();

    /*C_VRBasePlayer* pPlayer = (C_VRBasePlayer*)C_BasePlayer::GetLocalPlayer();
    if ( pPlayer )
    {
        pPlayer->OnVRDisabled();
    }*/

	return true;
}


bool VRSystem::IsDX11()
{
#if ENGINE_QUIVER
    // static ConVarRef mat_dxlevel( "mat_dxlevel" );
    // return mat_dxlevel.GetInt() == 110;
    return CommandLine()->FindParm( "-dx11" );
#else
    return false;
#endif
}


bool VRSystem::NeedD3DInit()
{
#if ENGINE_ASW
    if ( !g_VRSupported )
        return false;
#endif

	return IsDX11() ? false : (g_VRInt.d3d9Device == NULL || g_VRInt.d3d11TextureL == NULL || g_VRInt.d3d11TextureR == NULL);
}


void VRSystem::DX9EXToDX11( void* leftEyeData, void* rightEyeData )
{
    g_VRInt.OpenSharedResource( (HANDLE)leftEyeData, VREye::Left );
    g_VRInt.OpenSharedResource( (HANDLE)rightEyeData, VREye::Right );
}


void VRSystem::InitDX9Device( void* deviceData )
{
    g_VRInt.InitDX9Device( deviceData );
}


void VRSystem::Submit( ITexture* rtEye, VREye eye )
{
#if ENGINE_QUIVER || ENGINE_CSGO
	g_VRInt.Submit( IsDX11(), materials->VR_GetSubmitInfo( rtEye ), ToOVREye( eye ) );

#elif ENGINE_ASW
    g_VRInt.Submit( IsDX11(), NULL, ToOVREye( eye ) );

#else
	// TODO: use the hack gmod vr uses
    g_VRInt.Submit( IsDX11(), NULL, ToOVREye( eye ) );
#endif
}


void VRSystem::Submit( ITexture* leftEye, ITexture* rightEye )
{
	Submit( leftEye, VREye::Left );
	Submit( rightEye, VREye::Right );
}


void VRSystem::SetSeatedMode( bool seated )
{
    m_seatedMode = seated;
}


void VRSystem::SetScale( double newScale )
{
    m_scale = newScale;
}

double VRSystem::GetScale()
{
    return m_scale;
}



