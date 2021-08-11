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
#include "inputsystem/iinputstacksystem.h"
#include "tier0/ICommandLine.h"
#include "tier1/fmtstr.h"

#include "view.h"
#include "viewrender.h"

#if DXVK_VR
#include "vr_dxvk.h"
#endif

#include <openvr.h>
#include <mathlib/vector.h>

#include <d3d9.h>
#include <d3d11.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar vr_autostart("vr_autostart", "0", FCVAR_ARCHIVE, "auto start vr on level load");
ConVar vr_mainmenu("vr_mainmenu", "1", FCVAR_ARCHIVE);
ConVar vr_renderthread("vr_renderthread", "0", FCVAR_ARCHIVE, "use a separate thread for rendering in vr on the main menu or while loading a level");

ConVar vr_clamp_res("vr_clamp_res", "0", FCVAR_CLIENTDLL, "clamp the resolution to the screen size until the render clamping issue is figured out");
ConVar vr_scale_override("vr_scale_override", "42.5");  // anything lower than 0.25 doesn't look right in the headset
ConVar vr_dbg_rt_res("vr_dbg_rt_res", "2048", FCVAR_CLIENTDLL);
ConVar vr_eye_height("vr_eye_h", "0", FCVAR_CLIENTDLL, "Override the render target height, 0 to disable");
ConVar vr_eye_width("vr_eye_w", "0", FCVAR_CLIENTDLL, "Override the render target width, 0 to disable");

ConVar vr_haptic("vr_haptic", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Enable/Disable Haptic Feedback/Vibrations");

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

#if DXVK_VR
IDXVK_VRSystem*             g_pDXVK = NULL;
#endif

static InputContextHandle_t g_vrInputContext = INPUT_CONTEXT_HANDLE_INVALID;

// 10.0f/0.254f;
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

    for ( int i = 0; i < g_VR.m_currentTrackers.Count(); i++ )
    {
        VRHostTracker* tracker = g_VR.m_currentTrackers[i];

        Msg( "Tracker/Pose \"%s\" Info:\n", tracker->name );

        char propName[256];
        g_VR.GetTrackingPropString( propName, 256, vr::Prop_ControllerType_String, tracker->deviceIndex );
        Msg(" - Prop_ControllerType  = %s\n", propName);

        g_VR.GetTrackingPropString( propName, 256, vr::Prop_TrackingSystemName_String, tracker->deviceIndex );
        Msg(" - Prop_TrackingSystemName = %s\n", propName);

        g_VR.GetTrackingPropString( propName, 256, vr::Prop_RenderModelName_String, tracker->deviceIndex );
        Msg(" - Prop_RenderModelName = %s\n", propName);

        if ( tracker->type == EVRTracker::HMD )
        {
            Msg(" - Prop_DisplayFrequency = %f\n", g_pOVR->GetFloatTrackedDeviceProperty( tracker->deviceIndex, vr::Prop_DisplayFrequency_Float, NULL ));
        }
    }

	ListTrackers();
	ListActions();
}


vr::EVREye ToOVREye( VREye eye )
{
	return (vr::EVREye)eye;
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


const char* GetTrackerModelName( uint32_t deviceIndex )
{
    char modelName[MAX_PATH];
    g_VR.GetTrackingPropString( modelName, 256, vr::Prop_RenderModelName_String, deviceIndex );

    // ex: "models/vr/oculus_cv1_controller_left.mdl"
    char modelPath[MAX_PATH];
    V_snprintf(modelPath, MAX_PATH, "models/vr/%s.mdl", modelName);

    return strdup(modelPath);
}

const char* VRHostTracker::GetModelName()
{
    return GetTrackerModelName( deviceIndex );

    // obsolete code path? could only be useful for networking it
    /*if ( !device )
    {
        device = g_VRShared.GetDeviceType( "oculus_cv1" );
    }

    if ( device == nullptr )
    {
        Warning("[VR] No Device Type for tracker \"%s\"", name);
        return "";
    }

    return device->GetTrackerModelName(type);*/
}


// -----------------------------------------------------------------------------


void VRSystem::GetTrackingPropString( char* value, int size, vr::ETrackedDeviceProperty prop, uint deviceId )
{
    vr::ETrackedPropertyError error = vr::TrackedProp_Success;
    uint32_t capacity = g_pOVR->GetStringTrackedDeviceProperty( deviceId, prop, NULL, 0, &error );

    if ( capacity > 1 )
    {
        g_pOVR->GetStringTrackedDeviceProperty( deviceId, prop, value, size, &error );
    }
    else
    {
        V_snprintf( value, size, "[ERROR - %s]", g_pOVR->GetPropErrorNameFromEnum( error ) );
    }
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
    double inchesPerMeter = GetScale();

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
    SetDefLessFunc( m_deviceClasses );

    m_inMap = false;
}


VRSystem::~VRSystem()
{
}


#if DXVK_VR
extern IDXVK_VRSystem* GetDXVK_VRSystem();
#endif


bool VRSystem::Init()
{
#if DXVK_VR

    g_pDXVK = GetDXVK_VRSystem();
    g_VRSupported = (g_pDXVK != NULL);

#else

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

#endif

    g_VRRenderer.Init();

    g_vrInputContext = g_pInputStackSystem->PushInputContext();
    g_pInputStackSystem->EnableInputContext( g_vrInputContext, false );

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
    if ( vr_waitgetposes_test.GetInt() == 0 )
        WaitGetPoses();

    if ( m_scale <= 0.0f )
        m_scale = DEFAULT_VR_SCALE;

    if ( !m_inMap )
        g_VRRenderer.Render();
}


// might be a good idea to rename this function, probably OpenVRUpdate, idk
void VRSystem::WaitGetPoses()
{
	UpdateViewParams();

#if !DXVK_VR
    g_VRInt.WaitGetPoses();
#endif

    UpdateDevices();
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
    // don't shutdown if this is just a level transition (does this work with server changelevel?)
	if ( active && !vr_mainmenu.GetBool() && !engine->IsTransitioningToLoad() )
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
		if ( m_currentTrackers[i]->type == tracker )
		{
			return m_currentTrackers[i];
		}
	}

	return NULL;
}


VRHostTracker* VRSystem::GetTrackerByName( const char* name )
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

VRBaseAction* VRSystem::GetActionByHandle( vr::VRActionHandle_t handle )
{
	for ( int i = 0; i < m_currentActions.Count(); i++ )
	{
		if ( m_currentActions[i]->handle == handle )
		{
			return m_currentActions[i];
		}
	}

	return NULL;
}

VRBaseAction* VRSystem::GetActionByName( const char* name )
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


void VRSystem::TriggerHapticFeedback( EVRTracker tracker, float amplitude, float duration, float freq, float delay )
{
    if ( !vr_haptic.GetBool() )
        return;

    VRHostTracker* hostTracker = GetTracker( tracker );
    if ( hostTracker == NULL )
        return;

    // NOTE: you can technically do haptic feedback with vive trackers with some output pins or something
    const char* actionName;
    if ( tracker == EVRTracker::LHAND )
    {
        actionName = "vibration_left";
    }
    else if ( tracker == EVRTracker::RHAND )
    {
        actionName = "vibration_right";
    }
    else
    {
        Msg( "[VR] Invalid device for Haptic Feedback - \"%s\"\n", hostTracker->name );
    }

    vr::EVRInputError ret = vr::VRInputError_None;

    for (int i = 0; i < g_VRInt.actionCount; i++)
    {
        if (V_strcmp(g_VRInt.actions[i].name, actionName) == 0)
        {
            ret = g_pOVRInput->TriggerHapticVibrationAction(g_VRInt.actions[i].handle, delay, duration, freq, amplitude, vr::k_ulInvalidInputValueHandle);
            break;
        }
    }

    if (ret != vr::EVRInputError::VRInputError_None)
    {
        Msg( "[VR] Haptic Feedback failed! Error code %d\n", ret );
    }
}


// slightly overkill lol, could be moved to vr_util maybe, idk
static float GetArgFloat( const CCommand& args, int i, float _default )
{
    if ( args.ArgC() <= i )
        return _default;

    char* end;
    double result = 0;

    result = strtod(args.Arg(i), &end);

    if ( end == args.Arg(i) )
    {
        Msg("Error: Arg %d - \"%s\" is not a valid float, defaulting to %.2f\n", i, args.Arg(i), _default);
        return _default;
    }

    return result;
}


CON_COMMAND(vr_test_haptics, "Test Haptics")
{
    if ( args.ArgC() <= 1 )
    {
        Msg("Pick a controller with L or R, or B for both\n");
        Msg("The next optional args are Amplitude, Duration, Frequency, and Delay, all floats\n");
        return;
    }

    const char* controllerChar = args.Arg(1);

    int controller = -1;
    if ( V_strcmp(controllerChar, "L") == 0 )
    {
        controller = 0;
    }
    else if ( V_strcmp(controllerChar, "R") == 0 )
    {
        controller = 1;
    }
    else if ( V_strcmp(controllerChar, "B") == 0 )
    {
        controller = 2;
    }
    else
    {
        Msg("Invalid controller specified: \"\"\n", controllerChar);
        return;
    }

    float amplitude = GetArgFloat(args, 4, 1.0f);
    float duration = GetArgFloat(args, 2, 1.0f);
    float freq = GetArgFloat(args, 3, 1.0f);
    float delay = GetArgFloat(args, 5, 0.0f);

    if (controller == 0 || controller == 2)
    {
        g_VR.TriggerHapticFeedback( EVRTracker::LHAND, amplitude, duration, freq, delay );
    }
    if (controller == 1 || controller == 2)
    {
        g_VR.TriggerHapticFeedback( EVRTracker::RHAND, amplitude, duration, freq, delay );
    }
}


// useless?
void VRSystem::UpdateDevices()
{
    /** Returns the device class of a tracked device. If there has not been a device connected in this slot
    * since the application started this function will return TrackedDevice_Invalid. For previous detected
    * devices the function will return the previously observed device class. 
    *
    * To determine which devices exist on the system, just loop from 0 to k_unMaxTrackedDeviceCount and check
    * the device class. Every device with something other than TrackedDevice_Invalid is associated with an 
    * actual tracked device. */
    // ETrackedDeviceClass GetTrackedDeviceClass( vr::TrackedDeviceIndex_t unDeviceIndex ) = 0;

    m_deviceClasses.Purge();

    for (uint i = 0; i < vr::k_unMaxTrackedDeviceCount; i++)
    {
        vr::ETrackedDeviceClass trackedDevice = g_pOVR->GetTrackedDeviceClass(i);

        if ( trackedDevice != vr::TrackedDeviceClass_Invalid )
        {
            m_deviceClasses.Insert( i, trackedDevice );
        }
    }

    /** Get a sorted array of device indices of a given class of tracked devices (e.g. controllers).  Devices are sorted right to left
    * relative to the specified tracked device (default: hmd -- pass in -1 for absolute tracking space).  Returns the number of devices
    * in the list, or the size of the array needed if not large enough. */
    // uint32_t GetSortedTrackedDeviceIndicesOfClass( ETrackedDeviceClass eTrackedDeviceClass, VR_ARRAY_COUNT(unTrackedDeviceIndexArrayCount) vr::TrackedDeviceIndex_t *punTrackedDeviceIndexArray, uint32_t unTrackedDeviceIndexArrayCount, vr::TrackedDeviceIndex_t unRelativeToTrackedDeviceIndex = k_unTrackedDeviceIndex_Hmd ) = 0;

    /*uint32_t ret;
    vr::TrackedDeviceIndex_t trackedControllers[4];
    vr::TrackedDeviceIndex_t baseStationIndices[4];

    ret = g_pOVR->GetSortedTrackedDeviceIndicesOfClass( vr::TrackedDeviceClass_Controller, trackedControllers, 4 );
    ret = g_pOVR->GetSortedTrackedDeviceIndicesOfClass( vr::TrackedDeviceClass_TrackingReference, baseStationIndices, 4 );

    Msg("funny\n");*/
}


extern EVRTracker GetTrackerEnum( const char* name );


void VRSystem::UpdateTrackers()
{
    CUtlVector< VRHostTracker* > tmpTrackers;

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
                tracker = new VRHostTracker;
                tracker->name = strdup(poseName);
                tracker->type = GetTrackerEnum( poseName );
                tracker->actionIndex = i;

                vr::TrackedDeviceIndex_t deviceIndex = vr::k_unTrackedDeviceIndexInvalid;

                // only ones with a deviceIndex right now, blech
                if ( tracker->type == EVRTracker::HMD )
                {
                    deviceIndex = 0;
                }
                else if ( tracker->type == EVRTracker::LHAND )
                {
                    // yeah yeah depriciated, whatever, idk what else to use for this
                    deviceIndex = g_pOVR->GetTrackedDeviceIndexForControllerRole( vr::TrackedControllerRole_LeftHand );
                }
                else if ( tracker->type == EVRTracker::RHAND )
                {
                    deviceIndex = g_pOVR->GetTrackedDeviceIndexForControllerRole( vr::TrackedControllerRole_RightHand );
                }

                tracker->deviceIndex = deviceIndex;

                Msg("[VR] Tracker connected: %s\n", tracker->name);

                // if ( tracker->type == EVRTracker::HMD || tracker->type == EVRTracker::LHAND || tracker->type == EVRTracker::RHAND )
                if ( deviceIndex != vr::k_unTrackedDeviceIndexInvalid )
                {
                    char propName[256];
                    GetTrackingPropString( propName, 256, vr::Prop_ControllerType_String, deviceIndex );
                    DevMsg(" - Prop_ControllerType  = %s\n", propName);

                    // GetTrackingPropString( propName, 256, vr::Prop_TrackingSystemName_String, deviceIndex );
                    // DevMsg(" - Prop_TrackingSystemName = %s\n", propName);

                    GetTrackingPropString( propName, 256, vr::Prop_RenderModelName_String, deviceIndex );
                    DevMsg(" - Prop_RenderModelName = %s\n", propName);
                }
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
            Msg("[VR] Tracker disconnected: %s\n", m_currentTrackers[i]->name);
            delete m_currentTrackers[i];
        }
    }

    m_currentTrackers.Purge();
    m_currentTrackers.CopyArray( tmpTrackers.Base(), tmpTrackers.Count() );
}


#define VR_NEW_ACTION(type, name) \
    type* name = (currentAction == NULL) ?  new type : (type*)currentAction

void VRSystem::UpdateActions()
{
    CUtlVector< VRBaseAction* > tmpActions;
	// m_currentActions.PurgeAndDeleteElements();

    vr::InputDigitalActionData_t digitalActionData;
    vr::InputAnalogActionData_t analogActionData;
    vr::VRSkeletalSummaryData_t skeletalSummaryData;

    struct VRBaseAction* prevAction = NULL;

    for (int i = 0; i < g_VRInt.actionCount; i++)
    {
        // VRBaseAction* currentAction = NULL; 
        savedAction action = g_VRInt.actions[i];

        VRBaseAction* currentAction = GetActionByName( action.name );

        // doesn't work apparently
        // VRBaseAction* currentAction = GetActionByHandle( action.handle );

        // this is probably pretty slow and stupid
        // allocating memory and freeing it every frame

        if (strcmp(action.type, "boolean") == 0)
        {
            bool value = (g_pOVRInput->GetDigitalActionData(action.handle, &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && digitalActionData.bState);

            VR_NEW_ACTION(VRBoolAction, tempAction);
            tempAction->value = value;

            currentAction = tempAction;
        }
        else if (strcmp(action.type, "vector1") == 0)
        {
            g_pOVRInput->GetAnalogActionData(action.handle, &analogActionData, sizeof(analogActionData), vr::k_ulInvalidInputValueHandle);

            VR_NEW_ACTION(VRVector1Action, tempAction);
            tempAction->value = analogActionData.x;

            currentAction = tempAction;
        }
        else if (strcmp(action.type, "vector2") == 0)
        {
            g_pOVRInput->GetAnalogActionData(action.handle, &analogActionData, sizeof(analogActionData), vr::k_ulInvalidInputValueHandle);

            VR_NEW_ACTION(VRVector2Action, tempAction);
            tempAction->x = analogActionData.x;
            tempAction->y = analogActionData.y;

            currentAction = tempAction;
        }
        else if (strcmp(action.type, "skeleton") == 0)
        {
            VR_NEW_ACTION(VRSkeletonAction, tempAction);

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
            if ( currentAction->handle == 0 )
            {
                currentAction->name = strdup(action.name);
                currentAction->handle = action.handle;
            }

            tmpActions.AddToTail( &(*currentAction) );
        }
    }

    // is this even needed for actions?
    for ( int i = 0; i < m_currentActions.Count(); i++ )
    {
        bool foundMatch = false;
        for ( int j = 0; j < tmpActions.Count(); j++ )
        {
            if ( V_strcmp( m_currentActions[i]->name, tmpActions[j]->name ) != 0 )
            {
                foundMatch = true;
                break;
            }
        }

        // deleted action, free it
        if ( !foundMatch )
        {
            Msg("[VR] deleting an action??? wtf?????\n");
            delete m_currentActions[i];
        }
    }

    m_currentActions.Purge();
    m_currentActions.CopyArray( tmpActions.Base(), tmpActions.Count() );
}

#undef VR_NEW_ACTION


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
    if ( vr_clamp_res.GetBool() )
    {
        int scrWidth, scrHeight;
        materials->GetBackBufferDimensions( scrWidth, scrHeight );

        if ( scrWidth < width || scrHeight < height )
        {
            float widthRatio = (float)scrWidth / (float)width;
            float heightRatio = (float)scrHeight / (float)height;
            float bestRatio = MIN(widthRatio, heightRatio);

            height *= bestRatio;
            width *= bestRatio;

            if ( width % 2 != 0 )
            {
                width -= 1;
            }
        }
    }

    if ( vr_eye_height.GetFloat() > 0 )
        viewParams.height = vr_eye_height.GetFloat();

    if ( vr_eye_width.GetFloat() > 0 )
        viewParams.width = vr_eye_width.GetFloat();

    viewParams.width = width;
    viewParams.height = height;

    if ( active )
    {
        // uhh
        g_VRInt.CalcTextureBounds( viewParams.aspect, viewParams.fov );
        GetFOVOffset( VREye::Left, viewParams.aspect, viewParams.fov );
    }

    if ( vr_eye_width.GetFloat() > 0 || vr_eye_height.GetFloat() > 0 )
        viewParams.aspect = (float)viewParams.width / (float)viewParams.height;

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
#if DXVK_VR
    if ( g_VRSupported == NULL )
    {
        Warning("[VR] cannot start, DXVK VR interface not found!\n");
        return false;
    }
#endif

	// in case we're retrying after an error and shutdown wasn't called
	Disable();

#if !DXVK_VR
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
#endif

	vr::HmdError error = vr::VRInitError_None;

	g_pOVR = vr::VR_Init(&error, vr::VRApplication_Scene);
	if (error != vr::VRInitError_None)
	{
        const char* errorMsg = "";

        // only the errors i've gotten so far
        switch (error)
        {
            case vr::VRInitError_Init_InterfaceNotFound:
                errorMsg = " - Init_InterfaceNotFound";
                break;

            case vr::VRInitError_Init_HmdNotFound:
                errorMsg = " - Init_HmdNotFound";
                break;

            default:
                break;
        }

		Warning("[VR] VR_Init failed - Error Code %d%s\n", error, errorMsg);
		return false;
	}

	if (!vr::VRCompositor())
	{
		Warning("[VR] VRCompositor failed\n");
		return false;
	}

	UpdateViewParams();

	if ( g_VRInt.SetActionManifest("resource/vr/vrmod_action_manifest.txt") != 0 )
    {
        // failed, shutdown vr
        Disable();
        return false;
    }

    g_VRInt.ResetActiveActionSets();
    g_VRInt.AddActiveActionSet("/actions/base");
    g_VRInt.AddActiveActionSet("/actions/main");

    g_pOVRInput->SetDominantHand( vr_lefthand.GetBool() ? vr::TrackedControllerRole_LeftHand : vr::TrackedControllerRole_RightHand );

#if DXVK_VR
    if ( g_pVRInterface == NULL )
        g_pVRInterface = new VRInterface;

    g_pDXVK->Init( g_pVRInterface );
#endif

	active = true;
    m_scale = DEFAULT_VR_SCALE;

    // convienence
#if ENGINE_CSGO
    engine->ClientCmd_Unrestricted("engine_no_focus_sleep 0");
#endif

#if ENGINE_NEW
    g_wasPostProcessingOn = mat_postprocess_enable.GetBool();
    mat_postprocess_enable.SetValue("0");
#endif

    // always have the cursor enabled when in vr, so no alt tabbing is needed
    // maybe hide the cursor after 5 seconds of it being inactive (SetCursorVisible)?
    g_pInputStackSystem->EnableInputContext( g_vrInputContext, true );

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

#if DXVK_VR
    g_pDXVK->Shutdown();
#endif

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
    
#if ENGINE_NEW
    mat_postprocess_enable.SetValue( g_wasPostProcessingOn );
#endif

    g_pInputStackSystem->EnableInputContext( g_vrInputContext, false );

    m_currentActions.PurgeAndDeleteElements();
    m_currentTrackers.Purge();

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
#if DXVK_VR
    return false;
#else

#if ENGINE_ASW
    if ( !g_VRSupported )
        return false;
#endif

    if ( vr_dbg_rt_test.GetBool() )
        return false;

	return IsDX11() ? false : (g_VRInt.d3d9Device == NULL || g_VRInt.d3d11TextureL == NULL || g_VRInt.d3d11TextureR == NULL);
#endif
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
#if !DXVK_VR
#if ENGINE_QUIVER || ENGINE_CSGO
	g_VRInt.Submit( IsDX11(), materials->VR_GetSubmitInfo( rtEye ), ToOVREye( eye ) );

#elif ENGINE_ASW
    g_VRInt.Submit( IsDX11(), NULL /*g_pShaderAPI->VR_GetSubmitInfo( rtEye )*/, ToOVREye( eye ) );

#else
	// TODO: use the hack gmod vr uses
    g_VRInt.Submit( IsDX11(), NULL, ToOVREye( eye ) );
#endif
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


// uh is this even useful right now?
void VRSystem::SetScale( double newScale )
{
    m_scale = newScale;
}

double VRSystem::GetScale()
{
    double scale = DEFAULT_VR_SCALE;

    if ( vr_scale_override.GetFloat() > 0 )
        scale = vr_scale_override.GetFloat();

    if ( scale )
        scale *= vr_scale.GetFloat();

    return scale;
}



