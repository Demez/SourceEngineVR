#pragma once

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


enum class VREye;


// these are separate from the VRSystem class because these are only used internally in this file
class VRSystemInternal
{
public:

    void        OpenSharedResource( HANDLE eyeHandle, VREye eye );
    void        InitDX9Device( void* deviceData );
    void        WaitGetPoses();
    int         SetActionManifest( const char* fileName );
    void        AddActiveActionSet( const char* actionSetName );
    void        ResetActiveActionSets();
    void        Submit( bool isDX11, void* submitData, vr::EVREye eye );
    void        CalcTextureBounds( float &aspect, float &fov );


    // -------------------------------------------------------------------------------------

    // openvr saved stuff
    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
    actionSet               actionSets[MAX_ACTIONSETS];
    int                     actionSetCount = 0;
    vr::VRActiveActionSet_t activeActionSets[MAX_ACTIONSETS];
    int                     activeActionSetCount = 0;
    savedAction             actions[MAX_ACTIONS];
    int                     actionCount = 0;

    vr::VRTextureBounds_t   textureBoundsLeft;
    vr::VRTextureBounds_t   textureBoundsRight;

    // directx
    ID3D11Device*           d3d11Device = NULL;
    ID3D11Texture2D*        d3d11TextureL = NULL;
    ID3D11Texture2D*        d3d11TextureR = NULL;
    HANDLE                  sharedTextureL = NULL;
    HANDLE                  sharedTextureR = NULL;
    IDirect3DDevice9*       d3d9Device = NULL;
};

