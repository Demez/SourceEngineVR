#include "cbase.h"

#include "vr_openvr.h"
#include "vr_util.h"
#include "vr.h"

#include "usermessages.h"
#include "IGameUIFuncs.h"
#include "vgui_controls/Controls.h"
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include "filesystem.h"
#include "igamesystem.h"
#include "rendertexture.h"
#include "materialsystem/itexture.h"
#include "inputsystem/iinputsystem.h"

#include "view.h"
#include "viewrender.h"

#include <openvr.h>
#include <mathlib/vector.h>

#include <d3d9.h>
#include <d3d11.h>

#ifdef GetObject
#undef GetObject
#endif

#ifdef CreateEvent
#undef CreateEvent
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar vr_autostart("vr_autostart", "0", FCVAR_ARCHIVE, "auto start vr on level load");
ConVar vr_active_hack("vr_active_hack", "0", FCVAR_CLIENTDLL, "lazy hack for anything that needs to know if vr is enabled outside of the game dlls (mouse lock)");


#define MAX_STR_LEN 4096
#define MAX_ACTIONS 64
#define MAX_ACTIONSETS 16

struct savedAction
{
	vr::VRActionHandle_t handle;
	char fullname[MAX_STR_LEN];
	char type[MAX_STR_LEN];
	char* name;
};

struct actionSet
{
	vr::VRActionSetHandle_t handle;
	char name[MAX_STR_LEN];
};


// these are separate from the VRSystem class because these are only used internally in this file
vr::TrackedDevicePose_t g_OVR_poses[vr::k_unMaxTrackedDeviceCount];
actionSet               g_OVR_actionSets[MAX_ACTIONSETS];
int                     g_OVR_actionSetCount = 0;
vr::VRActiveActionSet_t g_OVR_activeActionSets[MAX_ACTIONSETS];
int                     g_OVR_activeActionSetCount = 0;
savedAction             g_OVR_actions[MAX_ACTIONS];
int                     g_OVR_actionCount = 0;


struct OVRDirectX
{
	// directx
	ID3D11Device*           d3d11Device = NULL;
	ID3D11Texture2D*        d3d11TextureL = NULL;
	ID3D11Texture2D*        d3d11TextureR = NULL;
	HANDLE                  sharedTextureL = NULL;
	HANDLE                  sharedTextureR = NULL;
	IDirect3DDevice9*       d3d9Device = NULL;
};


VRSystem        g_VR;

vr::IVRSystem*  g_pOVR = NULL;
vr::IVRInput*   g_pOVRInput = NULL;

OVRDirectX      g_OVRDX;
// OVRInputInfo    g_InputInfo;


int     OVR_SetActionManifest( const char* fileName );
int     OVR_TriggerHaptic( const char* actionName );

void    OVR_AddActiveActionSet(const char* actionSetName);
void    OVR_ResetActiveActionSets();
void    OVR_UpdatePosesAndActions();

void    OVR_GetPoses( CUtlVector< VRHostTracker* > &trackers );
void    OVR_GetActions( CUtlVector< VRBaseAction* > &actions );


CON_COMMAND(vr_enable, "")   { g_VR.Enable(); }
CON_COMMAND(vr_disable, "")  { g_VR.Disable();  }


void ListTrackers()
{
	if ( !g_VR.active )
	{
		Msg("[VR] VR is not enabled\n");
		return;
	}

	Msg("[VR] Current Trackers: \n");

	for ( int i = 0; i < g_VR.m_currentTrackers.Count(); i++ )
	{
		Msg(" - %s\n", g_VR.m_currentTrackers[i]->name);
	}
}


void ListActions()
{
	if ( !g_VR.active )
	{
		Msg("[VR] VR is not enabled\n");
		return;
	}

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
	if ( !g_VR.active )
	{
		Msg("[VR] VR is not enabled\n");
		return;
	}

	Msg( "[VR] SteamVR Runtime Version: %s\n", g_pOVR->GetRuntimeVersion() );

	// Msg( "[VR] %s\n", g_pOpenVR->GetStringTrackedDeviceProperty() );

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
	if( !pResult || !g_pOVR || !active )
		return false;

	float fLeft, fRight, fTop, fBottom;
    g_pOVR->GetProjectionRaw( ToOVREye( eEye ), &fLeft, &fRight, &fTop, &fBottom );

	ComposeProjectionTransform( fLeft, fRight, fTop, fBottom, zNear, zFar, fovScale, pResult );
	return true;
}


VMatrix VRSystem::GetMidEyeFromEye( VREye eEye )
{
	if( g_pOVR )
	{
		vr::HmdMatrix34_t matMidEyeFromEye = g_pOVR->GetEyeToHeadTransform( ToOVREye( eEye ) );
		return OpenVRToSourceCoordinateSystem( VMatrixFrom34( matMidEyeFromEye.m ), true );
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

    // hmmmm
    if ( !vr_active_hack.GetBool() )
        vr_active_hack.SetValue("1");
}


void VRSystem::LevelInitPostEntity()
{
	if ( vr_autostart.GetBool() )
    {
        Enable();
        inputsystem->DisableMouseCapture();
    }
}

void VRSystem::LevelShutdownPostEntity()
{
	// what if this is a level transition? shutting down would probably be stupid, right?
	if ( active )
    {
        Disable();
    }
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


void VRSystem::GetFOVOffset( VREye eye, int &hFov, float &hOffset, float &vOffset, float &aspectRatio )
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

	hFov = atan(w / 2.0f) * 180 / 3.141592654 * 2;
	//g_verticalFOV = atan(h / 2.0f) * 180 / 3.141592654 * 2;
	aspectRatio = w / h;
	hOffset = xoffset;
	vOffset = yoffset;
}


void VRSystem::UpdateBaseViewParams()
{
	VRViewParams view;

	// aspect ratio seems to be the same on each eye
	GetFOVOffset( VREye::Left, view.left.hFOV, view.left.hOffset, view.left.vOffset, view.aspectRatio );
	GetFOVOffset( VREye::Right, view.right.hFOV, view.right.hOffset, view.right.vOffset, view.aspectRatio );

    view.rtWidth = 0;
    view.rtHeight = 0;

	view.left.eyeToHeadTransformPos.Init();
	view.right.eyeToHeadTransformPos.Init();

	m_baseViewParams = view;
}


void VRSystem::UpdateViewParams()
{
	VRViewParams viewParams = m_baseViewParams;

    uint32_t width = 0;
    uint32_t height = 0;
    g_pOVR->GetRecommendedRenderTargetSize(&width, &height);

    viewParams.rtWidth = width;
    viewParams.rtHeight = height;

    vr::HmdMatrix34_t eyeToHeadLeft = g_pOVR->GetEyeToHeadTransform(vr::Eye_Left);
    vr::HmdMatrix34_t eyeToHeadRight = g_pOVR->GetEyeToHeadTransform(vr::Eye_Right);
    Vector eyeToHeadTransformPos;
    eyeToHeadTransformPos.x = eyeToHeadLeft.m[0][3];
    eyeToHeadTransformPos.y = eyeToHeadLeft.m[1][3];
    eyeToHeadTransformPos.z = eyeToHeadLeft.m[2][3];

    viewParams.left.eyeToHeadTransformPos = eyeToHeadTransformPos;

    eyeToHeadTransformPos.x = eyeToHeadRight.m[0][3];
    eyeToHeadTransformPos.y = eyeToHeadRight.m[1][3];
    eyeToHeadTransformPos.z = eyeToHeadRight.m[2][3];

    viewParams.right.eyeToHeadTransformPos = eyeToHeadTransformPos;

	m_currentViewParams = viewParams;
}


VRViewParams VRSystem::GetViewParams()
{
	return m_currentViewParams;
}


VREyeViewParams VRSystem::GetEyeViewParams( VREye eye )
{
	if ( eye == VREye::Left )
		return m_currentViewParams.left;
	else
		return m_currentViewParams.right;
}


bool VRSystem::Enable()
{
	// in case we're retrying after an error and shutdown wasn't called
	Disable();

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

	UpdateBaseViewParams();

	// copied right from gmod vr
	OVR_SetActionManifest("vr/vrmod_action_manifest.txt");

	OVR_ResetActiveActionSets();
	OVR_AddActiveActionSet("/actions/base");
	OVR_AddActiveActionSet("/actions/main");

	active = true;

    // convienence
    engine->ClientCmd_Unrestricted("engine_no_focus_sleep 0");
    vr_active_hack.SetValue("1");

	Update( 0.0 );
	return true;
}


bool VRSystem::Disable()
{
	active = false;

    if (g_pOVR != NULL)
    {
        vr::VR_Shutdown();
        g_pOVR = NULL;
    }

    if (g_OVRDX.d3d11Device != NULL)
    {
        g_OVRDX.d3d11Device->Release();
        g_OVRDX.d3d11Device = NULL;
    }

    g_OVRDX.d3d11TextureL = NULL;
    g_OVRDX.d3d11TextureR = NULL;
    g_OVRDX.sharedTextureL = NULL;
    g_OVRDX.sharedTextureR = NULL;
    g_OVRDX.d3d9Device = NULL;

    g_OVR_actionCount = 0;
    g_OVR_actionSetCount = 0;
    g_OVR_activeActionSetCount = 0;

    // i would save the old value, but nobody changes this anyway
    engine->ClientCmd_Unrestricted("engine_no_focus_sleep 50");
    vr_active_hack.SetValue("0");

	return true;
}


bool VRSystem::NeedD3DInit()
{
	return (g_OVRDX.d3d9Device == NULL);
}


ConVar vr_hoffset_mult("vr_hoffset_mult", "0.5");   // ideal value would be 0
ConVar vr_voffset_mult("vr_voffset_mult", "0.25");  // anything lower than 0.25 doesn't look right in the headset


// TODO: move OVR_Submit into this
void VRSystem::Submit( ITexture* rtEye, VREye eye )
{
	vr::VRTextureBounds_t textureBounds;
	VREyeViewParams viewParams = GetEyeViewParams( eye );

	float hMult = vr_hoffset_mult.GetFloat();
	float vMult = vr_voffset_mult.GetFloat();

	// TODO: set this properly and figure out wtf is going on with the ipd
#if 1
	textureBounds.uMin = 0.0f + viewParams.hOffset * hMult;
	textureBounds.uMax = 1.0f + viewParams.hOffset * hMult;
	textureBounds.vMin = 0.0f - viewParams.vOffset * vMult;
	textureBounds.vMax = 1.0f - viewParams.vOffset * vMult;
#else
	textureBounds.uMin = 0.0f + viewParams.hOffset * 0.5; // * 0.25f;
	textureBounds.uMax = 1.0f + viewParams.hOffset * 0.5; // * 0.25f;
	textureBounds.vMin = 0.0f - viewParams.vOffset * 0.25; // * 0.5f;
	textureBounds.vMax = 1.0f - viewParams.vOffset * 0.25; // * 0.5f;
#endif

#if ENGINE_QUIVER || ENGINE_CSGO
	SubmitInternal( materials->VR_GetSubmitInfo( rtEye ), ToOVREye( eye ), textureBounds );
#else
	// TODO: use the hack gmod vr uses
	SubmitInternal( NULL, ToOVREye( eye ), textureBounds );
#endif
}


void VRSystem::Submit( ITexture* leftEye, ITexture* rightEye )
{
	Submit( leftEye, VREye::Left );
	Submit( rightEye, VREye::Right );
}


void VRSystem::SubmitInternal( void* submitData, vr::EVREye eye, vr::VRTextureBounds_t &textureBounds )
{
	vr::Texture_t vrTexture;

	IDirect3DQuery9* pEventQuery = nullptr;
	g_OVRDX.d3d9Device->CreateQuery(D3DQUERYTYPE_EVENT, &pEventQuery);

	if (pEventQuery != nullptr)
	{
		pEventQuery->Issue(D3DISSUE_END);
		while (pEventQuery->GetData(nullptr, 0, D3DGETDATA_FLUSH) != S_OK);
		pEventQuery->Release();
	}

	vrTexture = {
		eye == vr::EVREye::Eye_Left ? g_OVRDX.d3d11TextureL : g_OVRDX.d3d11TextureR,
		vr::TextureType_DirectX,
		vr::ColorSpace_Auto
	};

	vr::EVRCompositorError error = vr::VRCompositor()->Submit( eye, &vrTexture, &textureBounds );

	if ( error != vr::VRCompositorError_None )
	{
		Warning("[VR] vr::VRCompositor() failed to submit!\n");
	}
}


// ======================================================================================


void OVR_InitDX9Device( void* deviceData )
{
    IDXGIFactory1* pFactory = nullptr;
    IDXGIAdapter1* pRecommendedAdapter = nullptr;
    HRESULT hr = CreateDXGIFactory1( __uuidof(IDXGIFactory1), (void**)(&pFactory) );

    if (SUCCEEDED(hr))
    {
        IDXGIAdapter1* pAdapter;
        UINT index = 0;
        while ( pFactory->EnumAdapters1(index, &pAdapter) != DXGI_ERROR_NOT_FOUND )
        {
            DXGI_ADAPTER_DESC1 desc;
            pAdapter->GetDesc1(&desc);
            if (desc.VendorId == 0x10de || desc.VendorId == 0x1002)
            {
                pRecommendedAdapter = pAdapter;
                pRecommendedAdapter->AddRef();
                DevMsg("Found adapter %ws, GPU Mem: %zu MiB, Sys Mem: %zu MiB, Shared Mem: %zu MiB",
                       desc.Description,
                       desc.DedicatedVideoMemory / (1024 * 1024),
                       desc.DedicatedSystemMemory / (1024 * 1024),
                       desc.SharedSystemMemory / (1024 * 1024));
            }
            // pAdapter->Release();
            // index++;
            break;
        }
        pFactory->Release();
    }

    // if ( D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, NULL, D3D11_SDK_VERSION, &g_d3d11Device, NULL, NULL) != S_OK )
    if ( D3D11CreateDevice(pRecommendedAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, NULL, D3D11_SDK_VERSION, &g_OVRDX.d3d11Device, NULL, NULL) != S_OK )
    {
        Warning("D3D11CreateDevice failed\n");
    }

    g_OVRDX.d3d9Device = (IDirect3DDevice9*)deviceData;
}



void OVR_DX9EXToDX11( void* leftEyeData, void* rightEyeData )
{
    HANDLE leftEyeHandle = (HANDLE)leftEyeData;
    HANDLE rightEyeHandle = (HANDLE)rightEyeData;

    // left eye
    {
        ID3D11Resource* leftRes;
        if ( FAILED(g_OVRDX.d3d11Device->OpenSharedResource(leftEyeHandle, __uuidof(ID3D11Resource), (void**)&leftRes)) )
        {
            return;
            Warning("OpenSharedResource failed\n");
        }

        if ( FAILED(leftRes->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&g_OVRDX.d3d11TextureL)))
        {
            Warning("QueryInterface failed\n");
        }
    }

    // right eye
    {
        ID3D11Resource* rightRes;
        if ( FAILED(g_OVRDX.d3d11Device->OpenSharedResource(rightEyeHandle, __uuidof(ID3D11Resource), (void**)&rightRes)) )
        {
            Warning("OpenSharedResource failed\n");
        }

        if ( FAILED(rightRes->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&g_OVRDX.d3d11TextureR)) )
        {
            Warning("QueryInterface failed\n");
        }
    }
}


// ========================================================================
// 
// ========================================================================
int OVR_SetActionManifest(const char* fileName)
{
    char currentDir[MAX_STR_LEN] = "\0";

    // maybe loop through these paths for all possible action sets?
    // having a specific search path key is a bit shit, but i can't use multiple with openvr
    g_pFullFileSystem->GetSearchPath( "VR", false, currentDir, MAX_STR_LEN );

    if ( V_strcmp(currentDir, "") == 0 )
    {
        // TODO: iterate through the search paths until we find one?
        g_pFullFileSystem->GetSearchPath( "MOD", false, currentDir, MAX_STR_LEN );
    }

    char *pSeperator = strchr( currentDir, ';' );
    if ( pSeperator )
        *pSeperator = '\0';

    char path[MAX_STR_LEN];

    // for ( path = strtok( searchPaths, ";" ); path; path = strtok( NULL, ";" ) )

    sprintf_s(path, MAX_STR_LEN, "%sresource%c%s", currentDir, CORRECT_PATH_SEPARATOR, fileName);

    g_pOVRInput = vr::VRInput();
    if (g_pOVRInput->SetActionManifestPath(path) != vr::VRInputError_None)
    {
        Warning("[VR] SetActionManifestPath failed\n");
        return -1;
    }

    FILE* file = NULL;
    fopen_s(&file, path, "r");
    if (file == NULL)
    {
        Warning("[VR] failed to open action manifest\n");
        return -1;
    }

    memset(g_OVR_actions, 0, sizeof(g_OVR_actions));

    char word[MAX_STR_LEN];
    while (fscanf_s(file, "%*[^\"]\"%[^\"]\"", word, MAX_STR_LEN) == 1 && strcmp(word, "actions") != 0);
    while (fscanf_s(file, "%[^\"]\"", word, MAX_STR_LEN) == 1)
    {
        if (strchr(word, ']') != nullptr)
            break;

        if (strcmp(word, "name") == 0)
        {
            if (fscanf_s(file, "%*[^\"]\"%[^\"]\"", g_OVR_actions[g_OVR_actionCount].fullname, MAX_STR_LEN) != 1)
                break;

            g_OVR_actions[g_OVR_actionCount].name = g_OVR_actions[g_OVR_actionCount].fullname;
            for (int i = 0; i < strlen(g_OVR_actions[g_OVR_actionCount].fullname); i++)
            {
                if (g_OVR_actions[g_OVR_actionCount].fullname[i] == '/')
                    g_OVR_actions[g_OVR_actionCount].name = g_OVR_actions[g_OVR_actionCount].fullname + i + 1;
            }

            g_pOVRInput->GetActionHandle(g_OVR_actions[g_OVR_actionCount].fullname, &(g_OVR_actions[g_OVR_actionCount].handle));
        }

        if (strcmp(word, "type") == 0)
        {
            if (fscanf_s(file, "%*[^\"]\"%[^\"]\"", g_OVR_actions[g_OVR_actionCount].type, MAX_STR_LEN) != 1)
                break;
        }

        if (g_OVR_actions[g_OVR_actionCount].fullname[0] && g_OVR_actions[g_OVR_actionCount].type[0])
        {
            g_OVR_actionCount++;
            if (g_OVR_actionCount == MAX_ACTIONS)
                break;
        }
    }

    fclose(file);

    return 0;
}

// ========================================================================
// 
// ========================================================================
void OVR_AddActiveActionSet(const char* actionSetName)
{
    int actionSetIndex = -1;
    for (int j = 0; j < g_OVR_actionSetCount; j++)
    {
        if (strcmp(actionSetName, g_OVR_actionSets[j].name) == 0)
        {
            actionSetIndex = j;
            break;
        }
    }
    if (actionSetIndex == -1)
    {
        g_pOVRInput->GetActionSetHandle(actionSetName, &g_OVR_actionSets[g_OVR_actionSetCount].handle);
        memcpy(g_OVR_actionSets[g_OVR_actionSetCount].name, actionSetName, strlen(actionSetName));
        actionSetIndex = g_OVR_actionSetCount;
        g_OVR_actionSetCount++;
    }

    g_OVR_activeActionSets[g_OVR_activeActionSetCount].ulActionSet = g_OVR_actionSets[actionSetIndex].handle;
    g_OVR_activeActionSetCount++;
}


void OVR_ResetActiveActionSets()
{
    g_OVR_activeActionSetCount = 0;

    // clear g_OVR_activeActionSets?
}


// ========================================================================
// 
// ========================================================================
void OVR_UpdatePosesAndActions()
{
    vr::EVRCompositorError error = vr::VRCompositor()->WaitGetPoses( g_OVR_poses, vr::k_unMaxTrackedDeviceCount, NULL, 0 );

    if ( error != vr::VRCompositorError_None )
    {
        Warning( "[VR] vr::VRCompositor()->WaitGetPoses failed!\n" );
    }

    g_pOVRInput->UpdateActionState( g_OVR_activeActionSets, sizeof(vr::VRActiveActionSet_t), g_OVR_activeActionSetCount );
}

// ========================================================================
// dumped into g_VR.m_currentTrackers
// ========================================================================
void OVR_GetPoses( CUtlVector< VRHostTracker* > &trackers )
{
    vr::InputPoseActionData_t poseActionData;
    vr::TrackedDevicePose_t pose;
    char poseName[MAX_STR_LEN];

    for (int i = -1; i < g_OVR_actionCount; i++)
    {
        // select a pose
        // why is this false on all of them? ffs, whatever
        poseActionData.pose.bPoseIsValid = 0;
        pose.bPoseIsValid = 0;

        if (i == -1)
        {
            pose = g_OVR_poses[0];
            memcpy(poseName, "hmd", 4);
        }
        else if (strcmp(g_OVR_actions[i].type, "pose") == 0)
        {
            g_pOVRInput->GetPoseActionDataRelativeToNow(g_OVR_actions[i].handle, vr::TrackingUniverseStanding, 0, &poseActionData, sizeof(poseActionData), vr::k_ulInvalidInputValueHandle);
            pose = poseActionData.pose;
            strcpy_s(poseName, MAX_STR_LEN, g_OVR_actions[i].name);
        }
        else
        {
            continue;
        }

        if (pose.bPoseIsValid)
        {
            vr::HmdMatrix34_t mat = pose.mDeviceToAbsoluteTracking;

            struct VRHostTracker* tracker = (struct VRHostTracker*) malloc(sizeof(struct VRHostTracker));
            tracker->name = strdup(poseName);

            tracker->mat = OpenVRToSourceCoordinateSystem( VMatrixFrom34( mat.m ) );

            tracker->pos.x = tracker->mat[0][3];
            tracker->pos.y = tracker->mat[1][3];
            tracker->pos.z = tracker->mat[2][3];

            tracker->ang.x = asin(mat.m[1][2]) * (180.0 / 3.141592654);
            tracker->ang.y = atan2f(mat.m[0][2], mat.m[2][2]) * (180.0 / 3.141592654);
            tracker->ang.z = atan2f(-mat.m[1][0], mat.m[1][1]) * (180.0 / 3.141592654);

            tracker->vel.x = -pose.vVelocity.v[2];
            tracker->vel.y = -pose.vVelocity.v[0];
            tracker->vel.z = pose.vVelocity.v[1];

            tracker->angvel.x = -pose.vAngularVelocity.v[2] * (180.0 / 3.141592654);
            tracker->angvel.y = -pose.vAngularVelocity.v[0] * (180.0 / 3.141592654);
            tracker->angvel.z = pose.vAngularVelocity.v[1] * (180.0 / 3.141592654);

            trackers.AddToTail( tracker );
        }
    }
}



// ========================================================================
// dumped into g_VR.m_currentActions
// ========================================================================
void OVR_GetActions( CUtlVector< VRBaseAction* > &actions )
{
    vr::InputDigitalActionData_t digitalActionData;
    vr::InputAnalogActionData_t analogActionData;
    vr::VRSkeletalSummaryData_t skeletalSummaryData;

    struct VRBaseAction* prevAction = NULL;

    for (int i = 0; i < g_OVR_actionCount; i++)
    {
        VRBaseAction* currentAction = NULL; 

        // this is probably pretty slow and stupid
        // allocating memory and freeing it every frame
        // just for something this simple

        if (strcmp(g_OVR_actions[i].type, "boolean") == 0)
        {
            bool value = (g_pOVRInput->GetDigitalActionData(g_OVR_actions[i].handle, &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && digitalActionData.bState);

            VRBoolAction* tempAction = new VRBoolAction;
            tempAction->value = value;

            currentAction = tempAction;
        }
        else if (strcmp(g_OVR_actions[i].type, "vector1") == 0)
        {
            g_pOVRInput->GetAnalogActionData(g_OVR_actions[i].handle, &analogActionData, sizeof(analogActionData), vr::k_ulInvalidInputValueHandle);

            VRVector1Action* tempAction = new VRVector1Action;
            tempAction->value = analogActionData.x;

            currentAction = tempAction;
        }
        else if (strcmp(g_OVR_actions[i].type, "vector2") == 0)
        {
            g_pOVRInput->GetAnalogActionData(g_OVR_actions[i].handle, &analogActionData, sizeof(analogActionData), vr::k_ulInvalidInputValueHandle);
            VRVector2Action* tempAction = new VRVector2Action;
            tempAction->x = analogActionData.x;
            tempAction->y = analogActionData.y;

            currentAction = tempAction;
        }
        else if (strcmp(g_OVR_actions[i].type, "skeleton") == 0)
        {
            // VRSkeletonAction* tempAction = (struct VRSkeletonAction*) malloc(sizeof(struct VRSkeletonAction));
            VRSkeletonAction* tempAction = new VRSkeletonAction;

            g_pOVRInput->GetSkeletalSummaryData(g_OVR_actions[i].handle, vr::VRSummaryType_FromAnimation, &skeletalSummaryData);

            for (int j = 0; j < 5; j++)
            {
                tempAction->fingerCurls[j].x = j + 1;
                tempAction->fingerCurls[j].y = skeletalSummaryData.flFingerCurl[j];
            }

            currentAction = tempAction;
        }

        if ( currentAction != NULL )
        {
            currentAction->name = strdup(g_OVR_actions[i].name);
            actions.AddToTail( &(*currentAction) );
        }
    }
}


