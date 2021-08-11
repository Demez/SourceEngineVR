#pragma once


#ifdef CLIENT_DLL
#include "openvr.h"
#else
#include "../openvr/headers/openvr.h"
#endif


// interface for dxvk to interact with the client's vr system
class IVRInterface
{
public:
	virtual void WaitGetPoses() = 0;
	virtual void UpdatePoses() = 0;
	virtual void HandleSubmitError( vr::EVRCompositorError error ) = 0;
	virtual vr::VRTextureBounds_t GetTextureBounds( vr::EVREye eye ) = 0;
	virtual vr::IVRCompositor* GetCompositor() = 0;
};


// interface to access the vr system in dxvk from the client vr system
class IDXVK_VRSystem
{
public:
	virtual bool Init( IVRInterface* system ) = 0;
	virtual bool Shutdown() = 0;

	virtual void SetRenderTargetSize( uint32_t width, uint32_t height ) = 0;
	virtual void SetRenderTargetActive( bool enabled ) = 0;

	virtual void NextCreateTextureIsEye( vr::EVREye eye ) = 0;
};

#define DXVK_VR_API __declspec(dllexport)

extern "C" int DXVK_VR_API GetVRSystem( IDXVK_VRSystem** system );

