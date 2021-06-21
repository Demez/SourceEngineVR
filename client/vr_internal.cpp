#include "cbase.h"
#include "vr_internal.h"
#include "vr.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class VRSystem;

extern VRSystem             g_VR;
extern vr::IVRSystem*       g_pOVR;
extern vr::IVRInput*        g_pOVRInput;

VRSystemInternal            g_VRInt;

IShaderAPI *g_pShaderAPI = 0;
/*IShaderDeviceMgr* g_pShaderDeviceMgr = 0;
IShaderDevice *g_pShaderDevice = 0;
IShaderShadow* g_pShaderShadow = 0;*/

static CSysModule* g_ShaderHInst;
static CreateInterfaceFn g_shaderApiFactory;
extern bool g_VRSupported;

//-----------------------------------------------------------------------------
// Creates/destroys the shader implementation for the selected API
//-----------------------------------------------------------------------------
CreateInterfaceFn CreateShaderAPI( char const* pShaderDLL )
{
    if ( !pShaderDLL )
        return 0;

    // Clean up the old shader
    // DestroyShaderAPI();

    // Load the new shader
    g_ShaderHInst = Sys_LoadModule( pShaderDLL );

    // Error loading the shader
    if ( !g_ShaderHInst )
        return 0;

    // Get our class factory methods...
    return Sys_GetFactory( g_ShaderHInst );
}

void DestroyShaderAPI()
{
    if (g_ShaderHInst)
    {
        // NOTE: By unloading the library, this will destroy m_pShaderAPI
        Sys_UnloadModule( g_ShaderHInst );
        g_pShaderAPI = 0;
        g_ShaderHInst = 0;
    }
}


bool VRSystemInternal::InitShaderAPI()
{
    // if ( !CommandLine()->FindParm("-vrapi") )
    //    return false;

    g_shaderApiFactory = CreateShaderAPI( "shaderapidx9.dll" );
    if ( !g_shaderApiFactory )
    {
        return false;
    }

    // g_pShaderDeviceMgr = (IShaderDeviceMgr*)g_shaderApiFactory( SHADER_DEVICE_MGR_INTERFACE_VERSION, 0 );
    // if ( !g_pShaderDeviceMgr )
    //     return false;

    // g_pHWConfig = (IHardwareConfigInternal*)shaderApiFn( MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION, 0 );
    // if ( !g_pHWConfig )
    //     return false;

    // TODO: use this to check if this is a vr supported shaderapi, if this exists, it does support vr
    /*g_pShaderAPIVR = (IShaderAPIVR*)g_shaderApiFactory( SHADERAPI_VR_INTERFACE_VERSION, 0 );
    if ( !g_pShaderAPIVR )
        return false;*/

    g_pShaderAPI = (IShaderAPI*)g_shaderApiFactory( SHADERAPI_INTERFACE_VERSION, 0 );
    if ( !g_pShaderAPI )
        return false;

    /*g_pShaderDevice = (IShaderDevice*)g_shaderApiFactory( SHADER_DEVICE_INTERFACE_VERSION, 0 );
    if ( !g_pShaderDevice )
        return false;

    g_pShaderShadow = (IShaderShadow*)g_shaderApiFactory( SHADERSHADOW_INTERFACE_VERSION, 0 );
    if ( !g_pShaderShadow )
        return false;*/

    return true;
}


void VRSystemInternal::OpenSharedResource( HANDLE eyeHandle, VREye eye )
{
    ID3D11Resource* eyeResource;
    if ( FAILED(d3d11Device->OpenSharedResource(eyeHandle, __uuidof(ID3D11Resource), (void**)&eyeResource)) )
    {
        return;
        Warning("OpenSharedResource failed\n");
    }

    if ( FAILED(eyeResource->QueryInterface(__uuidof(ID3D11Texture2D), (eye == VREye::Left) ? (void**)&d3d11TextureL : (void**)&d3d11TextureR)) )
    {
        Warning("QueryInterface failed\n");
    }
}


void VRSystemInternal::InitDX9Device( void* deviceData )
{
    /*IDXGIFactory1* pFactory = nullptr;
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
    }*/

    // if ( D3D11CreateDevice(pRecommendedAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, NULL, D3D11_SDK_VERSION, &d3d11Device, NULL, NULL) != S_OK )
    if ( D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, NULL, D3D11_SDK_VERSION, &d3d11Device, NULL, NULL) != S_OK )
    {
        Warning("D3D11CreateDevice failed\n");
    }

    d3d9Device = (IDirect3DDevice9*)deviceData;
}


void VRSystemInternal::WaitGetPoses()
{
    vr::EVRCompositorError error = vr::VRCompositor()->WaitGetPoses( poses, vr::k_unMaxTrackedDeviceCount, NULL, 0 );

    if ( error != vr::VRCompositorError_None )
    {
        Warning( "[VR] vr::VRCompositor()->WaitGetPoses failed!\n" );
    }

    g_pOVRInput->UpdateActionState( activeActionSets, sizeof(vr::VRActiveActionSet_t), activeActionSetCount );
}


// GetComponentStateForBinding
int VRSystemInternal::SetActionManifest( const char* fileName )
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

    memset(actions, 0, sizeof(actions));

    char word[MAX_STR_LEN];
    while (fscanf_s(file, "%*[^\"]\"%[^\"]\"", word, MAX_STR_LEN) == 1 && strcmp(word, "actions") != 0);
    while (fscanf_s(file, "%[^\"]\"", word, MAX_STR_LEN) == 1)
    {
        if (strchr(word, ']') != nullptr)
            break;

        if (strcmp(word, "name") == 0)
        {
            if (fscanf_s(file, "%*[^\"]\"%[^\"]\"", actions[actionCount].fullname, MAX_STR_LEN) != 1)
                break;

            actions[actionCount].name = actions[actionCount].fullname;
            for (int i = 0; i < strlen(actions[actionCount].fullname); i++)
            {
                if (actions[actionCount].fullname[i] == '/')
                    actions[actionCount].name = actions[actionCount].fullname + i + 1;
            }

            g_pOVRInput->GetActionHandle(actions[actionCount].fullname, &(actions[actionCount].handle));
        }

        if (strcmp(word, "type") == 0)
        {
            if (fscanf_s(file, "%*[^\"]\"%[^\"]\"", actions[actionCount].type, MAX_STR_LEN) != 1)
                break;
        }

        if (actions[actionCount].fullname[0] && actions[actionCount].type[0])
        {
            actionCount++;
            if (actionCount == MAX_ACTIONS)
                break;
        }
    }

    fclose(file);

    return 0;
}


void VRSystemInternal::AddActiveActionSet( const char* actionSetName )
{
    int actionSetIndex = -1;
    for (int j = 0; j < actionSetCount; j++)
    {
        if (strcmp(actionSetName, actionSets[j].name) == 0)
        {
            actionSetIndex = j;
            break;
        }
    }
    if (actionSetIndex == -1)
    {
        g_pOVRInput->GetActionSetHandle(actionSetName, &actionSets[actionSetCount].handle);
        memcpy(actionSets[actionSetCount].name, actionSetName, strlen(actionSetName));
        actionSetIndex = actionSetCount;
        actionSetCount++;
    }

    activeActionSets[activeActionSetCount].ulActionSet = actionSets[actionSetIndex].handle;
    activeActionSetCount++;
}


void VRSystemInternal::ResetActiveActionSets()
{
    activeActionSetCount = 0;
    // clear activeActionSets?
}


extern ConVar vr_one_rt_test;


void VRSystemInternal::Submit( bool isDX11, void* submitData, vr::EVREye eye )
{
    vr::Texture_t vrTexture;
    vr::VRTextureBounds_t textureBounds = eye == vr::Eye_Left ? g_VRInt.textureBoundsLeft : g_VRInt.textureBoundsRight;

    if ( isDX11 )
    {
        vrTexture = {
            (ID3D11Texture2D*)submitData,
            vr::TextureType_DirectX,
            vr::ColorSpace_Auto
        };
    }
    else
    {
#if 1 //!ENGINE_ASW
        if ( g_VRSupported )
        {
            IDirect3DQuery9* pEventQuery = nullptr;
            d3d9Device->CreateQuery(D3DQUERYTYPE_EVENT, &pEventQuery);

            if (pEventQuery != nullptr)
            {
                pEventQuery->Issue(D3DISSUE_END);
                while (pEventQuery->GetData(nullptr, 0, D3DGETDATA_FLUSH) != S_OK);
                pEventQuery->Release();
            }
        }
#endif

        vrTexture = {
            // eye == vr::EVREye::Eye_Left ? d3d11TextureL : d3d11TextureR,
            eye == vr::EVREye::Eye_Left ? d3d11TextureL : (vr_one_rt_test.GetBool() ? d3d11TextureL : d3d11TextureR),
            // (eye == vr::EVREye::Eye_Right && !vr_one_rt_test.GetBool()) ? d3d11TextureR : d3d11TextureL,
            // d3d11TextureL,
            vr::TextureType_DirectX,
            vr::ColorSpace_Auto
        };
    }

    vr::EVRCompositorError error = vr::VRCompositor()->Submit( eye, &vrTexture, &textureBounds );

    if ( error != vr::VRCompositorError_None )
    {
        Warning("[VR] vr::VRCompositor() failed to submit - Error %d\n", error);
    }
}


void VRSystemInternal::CalcTextureBounds( float &aspect, float &fov )
{
    float l_left, l_right, l_top, l_bottom;
    g_pOVR->GetProjectionRaw( vr::Eye_Left, &l_left, &l_right, &l_top, &l_bottom );

    float r_left, r_right, r_top, r_bottom;
    g_pOVR->GetProjectionRaw( vr::Eye_Right, &r_left, &r_right, &r_top, &r_bottom );

    float l_hMax = MAX( -l_left, l_right );
    float r_hMax = MAX( -r_left, r_right );

    float l_vMax = MAX( -l_top, l_bottom );
    float r_vMax = MAX( -r_top, r_bottom );

    Vector2D tanHalfFov(
        MAX(l_hMax, r_hMax),
        MAX(l_vMax, r_vMax)
    );

    textureBoundsLeft.uMin = 0.5f + 0.5f * l_left / tanHalfFov.x;
    textureBoundsLeft.uMax = 0.5f + 0.5f * l_right / tanHalfFov.x;
    textureBoundsLeft.vMin = 0.5f - 0.5f * l_bottom / tanHalfFov.y;
    textureBoundsLeft.vMax = 0.5f - 0.5f * l_top / tanHalfFov.y;

    textureBoundsRight.uMin = 0.5f + 0.5f * r_left / tanHalfFov.x;
    textureBoundsRight.uMax = 0.5f + 0.5f * r_right / tanHalfFov.x;
    textureBoundsRight.vMin = 0.5f - 0.5f * r_bottom / tanHalfFov.y;
    textureBoundsRight.vMax = 0.5f - 0.5f * r_top / tanHalfFov.y;

    // fov on rift cv1 comes out to be 96 here, a bit too high, 90 seems to be the right value though
    // also aspect is just calculated at the moment, comes to 0.86 or something here, when diving w/h gets me 0.84
    // gmod vr returns different values for each (the fov one seems much more accurate, right around 90 fov)
    // gmod vr also returns like 0.83 for aspect

    aspect = tanHalfFov.x / tanHalfFov.y;
    // fov = 2.0f * RAD2DEG(atan(tanHalfFov.y));

    // fov = RAD2DEG(2.0f * atan(tanHalfFov.y));
    // fov = RAD2DEG(atan(tanHalfFov.y / 2.0f));
}

