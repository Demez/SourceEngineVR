// #include "basetypes.h"

#include <Windows.h>

// #include "tier1/interface.h"
#include "tier1/tier1.h"
#include "mathlib/vector.h"
#include "filesystem.h"

#include "vr_openvr.h"

#include <sstream>
#include <stdio.h>

#include <d3d9.h>
#include <d3d11.h>
#include <openvr.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// =====================================================================================
// TODO: merge this into CVRSystem
// =====================================================================================

// ========================================================================
// All Credit for this goes to catsethecat:
// https://steamcommunity.com/sharedfiles/filedetails/?id=1678408548
// https://github.com/catsethecat/vrmod-module - original version of this file
// ========================================================================


// https://github.com/Facepunch/gmod-module-base/blob/development/include/GarrysMod/Lua/LuaBase.h

// ========================================================================
//  Globals
// ========================================================================

#ifdef GetCurrentDirectory
#undef GetCurrentDirectory
#endif

#define MAX_STR_LEN 4096
#define MAX_ACTIONS 64
#define MAX_ACTIONSETS 16

//openvr related
typedef struct {
    vr::VRActionHandle_t handle;
    char fullname[MAX_STR_LEN];
    char type[MAX_STR_LEN];
    char* name;
}action;

typedef struct {
    vr::VRActionSetHandle_t handle;
    char name[MAX_STR_LEN];
}actionSet;

vr::IVRSystem*          g_pOpenVR = NULL;
vr::IVRInput*           g_pInput = NULL;
vr::TrackedDevicePose_t g_poses[vr::k_unMaxTrackedDeviceCount];
actionSet               g_actionSets[MAX_ACTIONSETS];
int                     g_actionSetCount = 0;
vr::VRActiveActionSet_t g_activeActionSets[MAX_ACTIONSETS];
int                     g_activeActionSetCount = 0;
action                  g_actions[MAX_ACTIONS];
int                     g_actionCount = 0;

// directx
ID3D11Device*           g_d3d11Device = NULL;
ID3D11Texture2D*        g_d3d11TextureL = NULL;
ID3D11Texture2D*        g_d3d11TextureR = NULL;
HANDLE                  g_sharedTextureL = NULL;
HANDLE                  g_sharedTextureR = NULL;
IDirect3DDevice9*       g_d3d9Device = NULL;

// other
float                   g_horizontalFOVLeft = 0;
float                   g_horizontalFOVRight = 0;
float                   g_aspectRatioLeft = 0;
float                   g_aspectRatioRight = 0;
float                   g_horizontalOffsetLeft = 0;
float                   g_horizontalOffsetRight = 0;
float                   g_verticalOffsetLeft = 0;
float                   g_verticalOffsetRight = 0;


bool OVR_NeedD3DInit()
{
    return (g_d3d9Device == NULL);
}


void OVR_SetupFOVOffset()
{
    vr::HmdMatrix44_t proj = g_pOpenVR->GetProjectionMatrix(vr::EVREye::Eye_Left, 1, 10);
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
    g_horizontalFOVLeft = atan(w / 2.0f) * 180 / 3.141592654 * 2;
    //g_verticalFOV = atan(h / 2.0f) * 180 / 3.141592654 * 2;
    g_aspectRatioLeft = w / h;
    g_horizontalOffsetLeft = xoffset;
    g_verticalOffsetLeft = yoffset;

    proj = g_pOpenVR->GetProjectionMatrix(vr::EVREye::Eye_Right, 1, 10);
    xscale = proj.m[0][0];
    xoffset = proj.m[0][2];
    yscale = proj.m[1][1];
    yoffset = proj.m[1][2];
    tan_px = fabsf((1.0f - xoffset) / xscale);
    tan_nx = fabsf((-1.0f - xoffset) / xscale);
    tan_py = fabsf((1.0f - yoffset) / yscale);
    tan_ny = fabsf((-1.0f - yoffset) / yscale);
    w = tan_px + tan_nx;
    h = tan_py + tan_ny;
    g_horizontalFOVRight = atan(w / 2.0f) * 180 / 3.141592654 * 2;
    g_aspectRatioRight = w / h;
    g_horizontalOffsetRight = xoffset;
    g_verticalOffsetRight = yoffset;
}


// ========================================================================
// 
// ========================================================================
int OVR_Init()
{
    vr::HmdError error = vr::VRInitError_None;

    g_pOpenVR = vr::VR_Init(&error, vr::VRApplication_Scene);
    if (error != vr::VRInitError_None)
    {
        // would be nice if i could print out the enum name
        Warning("[VR] VR_Init failed - Error Code %d\n", error);
        return -1;
    }

    if (!vr::VRCompositor())
    {
        Warning("[VR] VRCompositor failed\n");
        return -1;
    }

    OVR_SetupFOVOffset();

    return 0;
}


void OVR_Submit( void* submitData, vr::EVREye eye )
{
    ConVarRef mat_dxlevel( "mat_dxlevel" );
    int dxlevel = mat_dxlevel.GetInt();

    vr::Texture_t vrTexture;

    // TODO: add d3d9 shared texture support here
    if ( dxlevel == 110 )
    {
        vrTexture = {
            (ID3D11Texture2D*)submitData,
            vr::TextureType_DirectX,
            vr::ColorSpace_Auto
        };
    }
    else
    {
        IDirect3DQuery9* pEventQuery = nullptr;
        g_d3d9Device->CreateQuery(D3DQUERYTYPE_EVENT, &pEventQuery);
        if (pEventQuery != nullptr)
        {
            pEventQuery->Issue(D3DISSUE_END);
            while (pEventQuery->GetData(nullptr, 0, D3DGETDATA_FLUSH) != S_OK);
            pEventQuery->Release();
        }

        vrTexture = {
            eye == vr::EVREye::Eye_Left ? g_d3d11TextureL : g_d3d11TextureR,
            vr::TextureType_DirectX,
            vr::ColorSpace_Auto
        };
    }

    vr::VRTextureBounds_t textureBounds;

    // TODO: use this one here and figure out wtf is going on with the ipd

   /* if ( eye == vr::EVREye::Eye_Left )
    {
        textureBounds.uMin = 0.0f + g_horizontalOffsetLeft * 0.25f;
        textureBounds.uMax = 1.0f + g_horizontalOffsetLeft * 0.25f;
        textureBounds.vMin = 0.0f - g_verticalOffsetLeft * 0.5f;
        textureBounds.vMax = 1.0f - g_verticalOffsetLeft * 0.5f;
    }
    else
    {
        // textureBounds.uMin = 0.5f + g_horizontalOffsetRight * 0.25f;
        // textureBounds.uMax = 1.0f + g_horizontalOffsetRight * 0.25f;

        textureBounds.uMin = 0.0f + g_horizontalOffsetRight * 0.25f;
        textureBounds.uMax = 1.0f + g_horizontalOffsetRight * 0.25f;
        textureBounds.vMin = 0.0f - g_verticalOffsetRight * 0.5f;
        textureBounds.vMax = 1.0f - g_verticalOffsetRight * 0.5f;
    }*/
    
    if ( eye == vr::EVREye::Eye_Left )
    {
        textureBounds.uMin = 0.0f + (g_horizontalOffsetLeft * 0.5); // * 0.5f;
        textureBounds.uMax = 1.0f + (g_horizontalOffsetLeft * 0.5); // * 0.5f;
        textureBounds.vMin = 0.0f - g_verticalOffsetLeft; // * 0.5f;
        textureBounds.vMax = 1.0f - g_verticalOffsetLeft; // * 0.5f;
    }
    else
    {
        // textureBounds.uMin = 0.5f + g_horizontalOffsetRight * 0.25f;
        // textureBounds.uMax = 1.0f + g_horizontalOffsetRight * 0.25f;

        textureBounds.uMin = 0.0f + (g_horizontalOffsetRight * 0.5); // * 0.5f;
        textureBounds.uMax = 1.0f + (g_horizontalOffsetRight * 0.5); // * 0.5f;
        textureBounds.vMin = 0.0f - g_verticalOffsetRight; // * 0.5f;
        textureBounds.vMax = 1.0f - g_verticalOffsetRight; // * 0.5f;
    }

    vr::EVRCompositorError error = vr::VRCompositor()->Submit( eye, &vrTexture, &textureBounds );

    if ( error != vr::VRCompositorError_None )
    {
        Warning("[VR] vr::VRCompositor() failed to submit!\n");
    }
}


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
    if ( D3D11CreateDevice(pRecommendedAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, NULL, D3D11_SDK_VERSION, &g_d3d11Device, NULL, NULL) != S_OK )
    {
        Warning("D3D11CreateDevice failed\n");
    }

    g_d3d9Device = (IDirect3DDevice9*)deviceData;
}



void OVR_DX9EXToDX11( void* leftEyeData, void* rightEyeData )
{
    HANDLE leftEyeHandle = (HANDLE)leftEyeData;
    HANDLE rightEyeHandle = (HANDLE)rightEyeData;

    // left eye
    {
        ID3D11Resource* leftRes;
        if ( FAILED(g_d3d11Device->OpenSharedResource(leftEyeHandle, __uuidof(ID3D11Resource), (void**)&leftRes)) )
        {
            return;
            Warning("OpenSharedResource failed\n");
        }

        if ( FAILED(leftRes->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&g_d3d11TextureL)))
        {
            Warning("QueryInterface failed\n");
        }
    }

    // right eye
    {
        ID3D11Resource* rightRes;
        if ( FAILED(g_d3d11Device->OpenSharedResource(rightEyeHandle, __uuidof(ID3D11Resource), (void**)&rightRes)) )
        {
            Warning("OpenSharedResource failed\n");
        }

        if ( FAILED(rightRes->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&g_d3d11TextureR)) )
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

    g_pInput = vr::VRInput();
    if (g_pInput->SetActionManifestPath(path) != vr::VRInputError_None)
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

    memset(g_actions, 0, sizeof(g_actions));

    char word[MAX_STR_LEN];
    while (fscanf_s(file, "%*[^\"]\"%[^\"]\"", word, MAX_STR_LEN) == 1 && strcmp(word, "actions") != 0);
    while (fscanf_s(file, "%[^\"]\"", word, MAX_STR_LEN) == 1)
    {
        if (strchr(word, ']') != nullptr)
            break;

        if (strcmp(word, "name") == 0)
        {
            if (fscanf_s(file, "%*[^\"]\"%[^\"]\"", g_actions[g_actionCount].fullname, MAX_STR_LEN) != 1)
                break;

            g_actions[g_actionCount].name = g_actions[g_actionCount].fullname;
            for (int i = 0; i < strlen(g_actions[g_actionCount].fullname); i++)
            {
                if (g_actions[g_actionCount].fullname[i] == '/')
                    g_actions[g_actionCount].name = g_actions[g_actionCount].fullname + i + 1;
            }

            g_pInput->GetActionHandle(g_actions[g_actionCount].fullname, &(g_actions[g_actionCount].handle));
        }

        if (strcmp(word, "type") == 0)
        {
            if (fscanf_s(file, "%*[^\"]\"%[^\"]\"", g_actions[g_actionCount].type, MAX_STR_LEN) != 1)
                break;
        }

        if (g_actions[g_actionCount].fullname[0] && g_actions[g_actionCount].type[0])
        {
            g_actionCount++;
            if (g_actionCount == MAX_ACTIONS)
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
    for (int j = 0; j < g_actionSetCount; j++)
    {
        if (strcmp(actionSetName, g_actionSets[j].name) == 0)
        {
            actionSetIndex = j;
            break;
        }
    }
    if (actionSetIndex == -1)
    {
        g_pInput->GetActionSetHandle(actionSetName, &g_actionSets[g_actionSetCount].handle);
        memcpy(g_actionSets[g_actionSetCount].name, actionSetName, strlen(actionSetName));
        actionSetIndex = g_actionSetCount;
        g_actionSetCount++;
    }

    g_activeActionSets[g_activeActionSetCount].ulActionSet = g_actionSets[actionSetIndex].handle;
    g_activeActionSetCount++;
}


void OVR_ResetActiveActionSets()
{
    g_activeActionSetCount = 0;
    
    // clear g_activeActionSets?
}


// ========================================================================
// 
// ========================================================================
VRViewParams OVR_GetViewParameters()
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

    return viewParams;
}

// ========================================================================
// 
// ========================================================================
void OVR_UpdatePosesAndActions()
{
	vr::EVRCompositorError error = vr::VRCompositor()->WaitGetPoses( g_poses, vr::k_unMaxTrackedDeviceCount, NULL, 0 );
    
    if ( error != vr::VRCompositorError_None )
	{
		Warning( "[VR] vr::VRCompositor()->WaitGetPoses failed!\n" );
	}

    g_pInput->UpdateActionState( g_activeActionSets, sizeof(vr::VRActiveActionSet_t), g_activeActionSetCount );
}

// ========================================================================
// dumped into g_VR.m_currentTrackers
// ========================================================================
void OVR_GetPoses( CUtlVector< VRHostTracker* > &trackers )
{
    vr::InputPoseActionData_t poseActionData;
    vr::TrackedDevicePose_t pose;
    char poseName[MAX_STR_LEN];

    for (int i = -1; i < g_actionCount; i++)
    {
        // select a pose
        // why is this false on all of them? ffs, whatever
        poseActionData.pose.bPoseIsValid = 0;
        pose.bPoseIsValid = 0;

        if (i == -1)
        {
            pose = g_poses[0];
            memcpy(poseName, "hmd", 4);
        }
        else if (strcmp(g_actions[i].type, "pose") == 0)
        {
            g_pInput->GetPoseActionDataRelativeToNow(g_actions[i].handle, vr::TrackingUniverseStanding, 0, &poseActionData, sizeof(poseActionData), vr::k_ulInvalidInputValueHandle);
            pose = poseActionData.pose;
            strcpy_s(poseName, MAX_STR_LEN, g_actions[i].name);
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

            tracker->mat = VMatrixFrom34( mat.m );
            tracker->matconv = OpenVRToSourceCoordinateSystem( tracker->mat );

            tracker->pos.x = tracker->matconv[0][3];
            tracker->pos.y = tracker->matconv[1][3];
            tracker->pos.z = tracker->matconv[2][3];

            // should i used the converted coordinates for this? maybe
            tracker->posraw.x = -mat.m[2][3];
            tracker->posraw.y = -mat.m[0][3];
            tracker->posraw.z = mat.m[1][3];

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

    for (int i = 0; i < g_actionCount; i++)
    {
        VRBaseAction* currentAction = NULL; 

        // this is probably pretty slow and stupid
        // allocating memory and freeing it every frame
        // just for something this simple

        if (strcmp(g_actions[i].type, "boolean") == 0)
        {
            bool value = (g_pInput->GetDigitalActionData(g_actions[i].handle, &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && digitalActionData.bState);
            
            VRBoolAction* tempAction = new VRBoolAction;
            tempAction->value = value;

            currentAction = tempAction;
        }
        else if (strcmp(g_actions[i].type, "vector1") == 0)
        {
            g_pInput->GetAnalogActionData(g_actions[i].handle, &analogActionData, sizeof(analogActionData), vr::k_ulInvalidInputValueHandle);
            
            VRVector1Action* tempAction = new VRVector1Action;
            tempAction->value = analogActionData.x;

            currentAction = tempAction;
        }
        else if (strcmp(g_actions[i].type, "vector2") == 0)
        {
            g_pInput->GetAnalogActionData(g_actions[i].handle, &analogActionData, sizeof(analogActionData), vr::k_ulInvalidInputValueHandle);
            VRVector2Action* tempAction = new VRVector2Action;
            tempAction->x = analogActionData.x;
            tempAction->y = analogActionData.y;

            currentAction = tempAction;
        }
        else if (strcmp(g_actions[i].type, "skeleton") == 0)
        {
            // VRSkeletonAction* tempAction = (struct VRSkeletonAction*) malloc(sizeof(struct VRSkeletonAction));
            VRSkeletonAction* tempAction = new VRSkeletonAction;
            
            g_pInput->GetSkeletalSummaryData(g_actions[i].handle, vr::VRSummaryType_FromAnimation, &skeletalSummaryData);

            for (int j = 0; j < 5; j++)
            {
                tempAction->fingerCurls[j].x = j + 1;
                tempAction->fingerCurls[j].y = skeletalSummaryData.flFingerCurl[j];
            }

            currentAction = tempAction;
        }

        if ( currentAction != NULL )
        {
            currentAction->name = strdup(g_actions[i].name);
            actions.AddToTail( &(*currentAction) );
        }
    }

    // return prevAction;
}


// ========================================================================
//  
// ========================================================================
int OVR_Shutdown()
{
    if (g_pOpenVR != NULL) {
        vr::VR_Shutdown();
        g_pOpenVR = NULL;
    }
    if (g_d3d11Device != NULL) {
        g_d3d11Device->Release();
        g_d3d11Device = NULL;
    }
    g_d3d11TextureL = NULL;
    g_d3d11TextureR = NULL;
    g_sharedTextureL = NULL;
    g_sharedTextureR = NULL;
    g_actionCount = 0;
    g_actionSetCount = 0;
    g_activeActionSetCount = 0;
    g_d3d9Device = NULL;

    return 0;
}

// ========================================================================
//  OVR_TriggerHaptic(actionName, delay, duration, frequency, amplitude)
// ========================================================================
int OVR_TriggerHaptic( const char* actionName )
{
    /*unsigned int nameLen = strlen(actionName);
    for (int i = 0; i < g_actionCount; i++) {
        if (strlen(g_actions[i].name) == nameLen && memcmp(g_actions[i].name, actionName, nameLen) == 0) {
            g_pInput->TriggerHapticVibrationAction(g_actions[i].handle, LUA->CheckNumber(2), LUA->CheckNumber(3), LUA->CheckNumber(4), LUA->CheckNumber(5), vr::k_ulInvalidInputValueHandle);
            break;
        }
    }*/
    return 0;
}

