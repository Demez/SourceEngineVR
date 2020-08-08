#include "cbase.h"
#include "vr.h"
#include "vr_viewrender.h"
#include "rendertexture.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar vr_fov_override;

ConVar vr_desktop_simulate_perf("vr_desktop_simulate_perf", "0", FCVAR_CLIENTDLL);
ConVar vr_debug_rt("vr_debug_rt", "1", FCVAR_CLIENTDLL);
ConVar vr_desktop_eye("vr_desktop_eye", "3", FCVAR_CLIENTDLL, "0 - no screen view, 1 - crop left eye, 2 - the right eye, 3 - rerender the desktop view");
ConVar vr_test("vr_test", "0", FCVAR_CLIENTDLL);
// ConVar vr_res("vr_res", "0", FCVAR_CLIENTDLL, "Override the render target resolution, 0 to disable");

ConVar vr_nearz("vr_nearz", "5", FCVAR_CLIENTDLL);
ConVar vr_autoupdate_rt("vr_autoupdate_rt", "1", FCVAR_CLIENTDLL);
ConVar vr_scale("vr_scale", "42.5", FCVAR_CLIENTDLL);


// issue: nothing can inherit this with this function, kind of a major issue
static CVRViewRender g_ViewRender;
IViewRender *GetViewRenderInstance()
{
	return &g_ViewRender;
}


CON_COMMAND_F(vr_update_rt, "Force recreate render targets for a different resolution", FCVAR_CLIENTDLL)
{
	g_ViewRender.InitEyeRenderTargets();
}


CVRViewRender::CVRViewRender()
{
	if (!view)
	{
		view = (IViewRender *)&g_ViewRender;
	}
	else
	{
		Warning("[VR] failed to override viewrender??? wtf\n");
	}

	leftEye = NULL;
	rightEye = NULL;

	leftEyeMat = NULL;
	rightEyeMat = NULL;
}


void CVRViewRender::InitEyeRenderTargets()
{
	// if we are hackily resizing the render targets, free the existing ones
	if ( leftEye != NULL )
		leftEye->Release();

	if ( rightEye != NULL )
		rightEye->Release();

	// setup with override convars
	VRViewParams viewParams = g_VR.GetViewParams();
	lastWidth = viewParams.rtWidth;
	lastHeight = viewParams.rtHeight;

	// needed if alien swarm or csgo
#if ENGINE_CSGO || ENGINE_ASW
	materials->ReEnableRenderTargetAllocation_IRealizeIfICallThisAllTexturesWillBeUnloadedAndLoadTimeWillSufferHorribly();
#endif

	materials->BeginRenderTargetAllocation();

	leftEye = materials->CreateNamedRenderTargetTextureEx2(
	    "_rt_vr_left",
		lastWidth, lastHeight,
		RT_SIZE_NO_CHANGE,
		IMAGE_FORMAT_RGBA8888,
		MATERIAL_RT_DEPTH_SEPARATE, // MATERIAL_RT_DEPTH_NONE,
	    TEXTUREFLAGS_RENDERTARGET | TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_NOMIP,
	    0 );

	rightEye = materials->CreateNamedRenderTargetTextureEx2(
	    "_rt_vr_right",
		lastWidth, lastHeight,
		RT_SIZE_NO_CHANGE,
		materials->GetBackBufferFormat(),
		MATERIAL_RT_DEPTH_NONE,
	    TEXTUREFLAGS_RENDERTARGET | TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_NOMIP,
	    0 );

	/*leftEyeDepthRef.InitRenderTarget(
		lastWidth, lastHeight,
		RT_SIZE_NO_CHANGE,
		IMAGE_FORMAT_NV_DST24,
		MATERIAL_RT_DEPTH_NONE,
		false
	);

	leftEyeDepth = leftEyeDepthRef;*/

	/*leftEyeDepth = materials->CreateNamedRenderTargetTextureEx2(
	    "_rt_vr_left_depth",
		lastWidth, lastHeight,
		RT_SIZE_NO_CHANGE,
		dstFormat, // IMAGE_FORMAT_NV_DST24, // materials->GetBackBufferFormat(),
		MATERIAL_RT_DEPTH_NONE, // MATERIAL_RT_DEPTH_ONLY - crashes dx9? wtf
		TEXTUREFLAGS_DEPTHRENDERTARGET | TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_POINTSAMPLE,
	    0 );

	rightEyeDepth = materials->CreateNamedRenderTargetTextureEx2(
	    "_rt_vr_right_depth",
		lastWidth, lastHeight,
		RT_SIZE_NO_CHANGE,
	    materials->GetBackBufferFormat(),
		MATERIAL_RT_DEPTH_ONLY,
		// TEXTUREFLAGS_DEPTHRENDERTARGET | TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_NOMIP,
		TEXTUREFLAGS_DEPTHRENDERTARGET | TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_POINTSAMPLE,
	    0 );*/

	leftEyeDepthRef.InitRenderTarget( lastWidth, lastHeight, RT_SIZE_DEFAULT, materials->GetBackBufferFormat(), MATERIAL_RT_DEPTH_NONE, true, "_rt_vr_left_depth" );
	leftEyeDepth = leftEyeDepthRef;

	rightEyeDepthRef.InitRenderTarget( lastWidth, lastHeight, RT_SIZE_DEFAULT, materials->GetBackBufferFormat(), MATERIAL_RT_DEPTH_NONE, true, "_rt_vr_right_depth" );
	rightEyeDepth = leftEyeDepthRef;

	leftEyeRef.Init( leftEye );
	rightEyeRef.Init( rightEye );

	// leftEyeDepthRef.Init( leftEye );
	// rightEyeDepthRef.Init( rightEye );

	materials->EndRenderTargetAllocation();

#if ENGINE_CSGO || ENGINE_ASW
	materials->FinishRenderTargetAllocation();
#endif

	uint dxlevel, recommended_dxlevel;
	materials->GetDXLevelDefaults( dxlevel, recommended_dxlevel );

	/*if ( recommended_dxlevel >= 90 && recommended_dxlevel != 110 )
	{
		OVR_DX9EXToDX11(
			materials->VR_GetDevice(),
			materials->VR_GetSubmitInfo( leftEye ),
			materials->VR_GetSubmitInfo( rightEye )
		);
	}*/
}


void CVRViewRender::UpdateEyeRenderTargets()
{
	VRViewParams viewParams = g_VR.GetViewParams();
	if ( leftEye == NULL || rightEye == NULL || lastWidth != viewParams.rtWidth || lastHeight != viewParams.rtHeight )
	{
		InitEyeRenderTargets();
	}
}


void CVRViewRender::InitEyeMats()
{
	// for displaying on the screen
	KeyValues *_rt_left_kv = new KeyValues( "UnlitGeneric" );
	_rt_left_kv->SetString( "$basetexture", "_rt_vr_left" );

	KeyValues *_rt_right_kv = new KeyValues( "UnlitGeneric" );
	_rt_right_kv->SetString( "$basetexture", "_rt_vr_left_depth" );

	leftEyeMat  = materials->CreateMaterial( "vrmod_left_rt", _rt_left_kv );
	rightEyeMat = materials->CreateMaterial( "vrmod_right_rt", _rt_right_kv );
}


void CVRViewRender::Render( vrect_t *rect )
{
	// see view.cpp in source 2013
	BaseViewRender::Render( rect );
}


void CVRViewRender::PrepareEyeViewSetup( CViewSetup &eyeView, const CViewSetup &screenView, VREye eye )
{
	if ( g_VR.active )
	{
		VRViewParams viewParams = g_VR.GetViewParams();
		VRTracker* hmd = g_VR.GetTrackerByName("hmd");

		eyeView.origin = screenView.origin;

		eyeView.width = viewParams.rtWidth;
		eyeView.height = viewParams.rtHeight;

		if ( vr_fov_override.GetInt() != 0 )
			eyeView.fov = vr_fov_override.GetInt();
		else
			eyeView.fov = viewParams.horizontalFOVLeft;

		eyeView.m_flAspectRatio = VREye::EyeLeft == eye ? viewParams.aspectRatioLeft : viewParams.aspectRatioRight;
	}
	else
	{
		// for testing
		eyeView.width = 512; // screenView.width;
		eyeView.height = 512; // screenView.height;
		eyeView.fov = screenView.fov;
		eyeView.m_flAspectRatio = 1.0; // screenView.m_flAspectRatio;

		eyeView.origin = screenView.origin;
	}

	eyeView.x = screenView.x;
	eyeView.y = screenView.y;

	eyeView.zNear = 7;
	eyeView.zFar = 25000;
	eyeView.m_bOrtho = false;

	eyeView.fovViewmodel = screenView.fovViewmodel;
	eyeView.m_bCacheFullSceneState = screenView.m_bCacheFullSceneState;
	eyeView.m_bRenderToSubrectOfLargerScreen = screenView.m_bRenderToSubrectOfLargerScreen;
	eyeView.m_bOffCenter = screenView.m_bOffCenter;
	eyeView.zNearViewmodel = screenView.zNearViewmodel;
	eyeView.zFarViewmodel = screenView.zFarViewmodel;

	eyeView.angles = screenView.angles;

	eyeView.m_flOffCenterBottom = screenView.m_flOffCenterBottom;
	eyeView.m_flOffCenterLeft   = screenView.m_flOffCenterLeft;
	eyeView.m_flOffCenterRight  = screenView.m_flOffCenterRight;
	eyeView.m_flOffCenterTop    = screenView.m_flOffCenterTop;
}


void CVRViewRender::RenderView( const CViewSetup &view, int nClearFlags, int whatToDraw )
{
	if ( vr_test.GetBool() || g_VR.active )
	{
		UpdateEyeRenderTargets();

		if ( leftEyeMat == NULL || rightEyeMat == NULL )
		{
			InitEyeMats();
		}

		CMatRenderContextPtr pRenderContext( materials );

		// draw main screen again
		if ( vr_desktop_eye.GetInt() == 3 )
		{
			BaseViewRender::RenderView( view, nClearFlags, whatToDraw );
		}

		// left eye
		{
			CViewSetup eyeView;
			PrepareEyeViewSetup( eyeView, view, VREye::EyeLeft );
			pRenderContext->PushRenderTargetAndViewport( leftEye, leftEyeDepth, 0, 0, leftEye->GetActualWidth(), leftEye->GetActualWidth() );
			BaseViewRender::RenderView( eyeView, nClearFlags, RENDERVIEW_DRAWVIEWMODEL );
			pRenderContext->PopRenderTargetAndViewport();
		}

		// right eye
		{
			CViewSetup eyeView;
			PrepareEyeViewSetup( eyeView, view, VREye::EyeRight );
			pRenderContext->PushRenderTargetAndViewport( rightEye, rightEyeDepth, 0, 0, rightEye->GetActualWidth(), rightEye->GetActualWidth() );
			BaseViewRender::RenderView( eyeView, nClearFlags, RENDERVIEW_DRAWVIEWMODEL );
			pRenderContext->PopRenderTargetAndViewport();
		}
		
		if ( !vr_test.GetBool() )
		{
			OVR_Submit( materials->VR_GetSubmitInfo( leftEye ), vr::EVREye::Eye_Left );
			OVR_Submit( materials->VR_GetSubmitInfo( rightEye ), vr::EVREye::Eye_Right );
		}

		if ( vr_debug_rt.GetBool() )
		{
			// draw left eye
			int width = leftEye->GetActualWidth();
			int height = leftEye->GetActualHeight();

			pRenderContext->DrawScreenSpaceRectangle(
				leftEyeMat,
				0, 0,
				width,
				height,
				0, 0,
				width-1,
				height-1,
				width,
				height
			);

			// draw right eye
			/*width = rightEye->GetActualWidth();
			height = rightEye->GetActualHeight();

			pRenderContext->DrawScreenSpaceRectangle(
				rightEyeMat,
				width, 0,
				width,
				height,
				0, 0,
				width-1,
				height-1,
				width,
				height
			);*/
		}

		// crop one of the eyes onto the screen
#if 0
		if ( vr_desktop_eye.GetInt() == 1 || vr_desktop_eye.GetInt() == 2 )
		{
			IMaterial* eyeMat = vr_desktop_eye.GetInt() == 1 ? leftEyeMat : rightEyeMat;
			VRViewParams viewParams = g_VR.GetViewParams();

			int scrWidth, scrHeight;
			engine->GetScreenSize( scrWidth, scrHeight );

			int eyeWidth = rightEye->GetActualWidth();
			int eyeHeight = rightEye->GetActualHeight();

			// definitely wrong, only accouning if the eye resolution is lower than the screen
			// int widthOffset = (scrWidth - eyeWidth) / 2;
			// int heightOffset = 0;

			float widthOffset = (1 - (scrWidth/scrHeight * viewParams.rtWidth / viewParams.rtHeight)) / 2;
			// float heightOffset = desktopView == 3 ? 0.5 : 0;
			float heightOffset = 0.0f;

			// now draw over entire screen
			pRenderContext->DrawScreenSpaceRectangle(
				eyeMat,
				0, 0,
				scrWidth, scrHeight,
				widthOffset, heightOffset,
				eyeWidth-1, eyeHeight-1,
				eyeWidth, eyeHeight
			);
		}
#endif

		// pRenderContext->CullMode( MATERIAL_CULLMODE_CCW );
	}
	else
	{
		BaseViewRender::RenderView( view, nClearFlags, whatToDraw );

		if ( vr_desktop_simulate_perf.GetBool() )
			BaseViewRender::RenderView( view, nClearFlags, whatToDraw );
	}
}


