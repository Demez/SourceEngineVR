#include "cbase.h"
#include "vr.h"
#include "vr_viewrender.h"
#include "rendertexture.h"
#include "iclientmode.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar vr_fov_override;

ConVar vr_desktop_simulate_perf("vr_desktop_simulate_perf", "0", FCVAR_CLIENTDLL);
ConVar vr_debug_rt("vr_debug_rt", "0", FCVAR_CLIENTDLL);
ConVar vr_desktop_eye("vr_desktop_eye", "3", FCVAR_CLIENTDLL, "0 - no screen view, 1 - crop left eye, 2 - the right eye, 3 - rerender the desktop view");
ConVar vr_test("vr_test", "0", FCVAR_CLIENTDLL);
// ConVar vr_res("vr_res", "0", FCVAR_CLIENTDLL, "Override the render target resolution, 0 to disable");

ConVar vr_nearz("vr_nearz", "5", FCVAR_CLIENTDLL);
ConVar vr_autoupdate_rt("vr_autoupdate_rt", "1", FCVAR_CLIENTDLL);
ConVar vr_scale_old("vr_scale_old", "42.5", FCVAR_CLIENTDLL);  // why is this in viewrender of all places?


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


void CVRViewRender::LevelInit()
{
	BaseViewRender::LevelInit();
	InitEyeRenderTargets();
}


void CVRViewRender::LevelShutdown()
{
	BaseViewRender::LevelShutdown();

	if ( leftEye != NULL )
		leftEye->Release();

	if ( rightEye != NULL )
		rightEye->Release();

	leftEye = NULL;
	rightEye = NULL;
}

// not sure if RT_SIZE_LITERAL is even needed tbh
#if !ENGINE_CSGO && !ENGINE_2013
#define RT_SIZE_LITERAL RT_SIZE_NO_CHANGE
#endif


void CVRViewRender::InitEyeRenderTargets()
{
	if ( !g_VR.active )
		return;

	// if we are hackily resizing the render targets, free the existing ones
	// Demez TODO: there is an issue here with this: if we change the eye resolution and make new render textures when already active,
	// the render targets sent to the headset just lock up and is never updated after the first time it's sent,
	// not even restarting openvr fixes it, so im not sure what's going on for that to happen yet
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
		RT_SIZE_LITERAL, // RT_SIZE_NO_CHANGE,
		materials->GetBackBufferFormat(),// IMAGE_FORMAT_RGBA8888,
		MATERIAL_RT_DEPTH_SHARED, // MATERIAL_RT_DEPTH_SEPARATE,
	    TEXTUREFLAGS_RENDERTARGET /*| TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_NOMIP*/,
	    0 );

	rightEye = materials->CreateNamedRenderTargetTextureEx2(
	    "_rt_vr_right",
		lastWidth, lastHeight,
		RT_SIZE_LITERAL, // RT_SIZE_NO_CHANGE,
		IMAGE_FORMAT_RGBA8888, // materials->GetBackBufferFormat(),
		MATERIAL_RT_DEPTH_SHARED, // MATERIAL_RT_DEPTH_NONE,
	    TEXTUREFLAGS_RENDERTARGET /*| TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_NOMIP*/,
	    0 );

	/*leftEyeDepth = materials->CreateNamedRenderTargetTextureEx2(
	    "_rt_vr_left_depth",
		lastWidth, lastHeight,
		RT_SIZE_LITERAL,
		materials->GetBackBufferFormat(),
		MATERIAL_RT_DEPTH_ONLY,
	    TEXTUREFLAGS_RENDERTARGET | TEXTUREFLAGS_DEPTHRENDERTARGET | TEXTUREFLAGS_NOMIP,
	    0 );

	rightEyeDepth = materials->CreateNamedRenderTargetTextureEx2(
	    "_rt_vr_right_depth",
		lastWidth, lastHeight,
		RT_SIZE_LITERAL,
		IMAGE_FORMAT_RGBA8888,
		MATERIAL_RT_DEPTH_ONLY,
	    TEXTUREFLAGS_RENDERTARGET | TEXTUREFLAGS_DEPTHRENDERTARGET | TEXTUREFLAGS_NOMIP,
	    0 );*/

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

	/*leftEyeDepthRef.InitRenderTarget( lastWidth, lastHeight, RT_SIZE_DEFAULT, materials->GetBackBufferFormat(), MATERIAL_RT_DEPTH_NONE, true, "_rt_vr_left_depth" );
	// leftEyeDepth = leftEyeDepthRef;

	rightEyeDepthRef.InitRenderTarget( lastWidth, lastHeight, RT_SIZE_DEFAULT, materials->GetBackBufferFormat(), MATERIAL_RT_DEPTH_NONE, true, "_rt_vr_right_depth" );
	rightEyeDepth = rightEyeDepthRef;*/

	// leftEyeRef.Init( leftEye );
	// rightEyeRef.Init( rightEye );

	// leftEyeDepthRef.Init( leftEye );
	// rightEyeDepthRef.Init( rightEye );

	materials->EndRenderTargetAllocation();

#if ENGINE_CSGO || ENGINE_ASW
	materials->FinishRenderTargetAllocation();
#endif

#if ENGINE_QUIVER || ENGINE_CSGO 
	// uint dxlevel, recommended_dxlevel;
	// materials->GetDXLevelDefaults( dxlevel, recommended_dxlevel );

	if ( g_VR.NeedD3DInit() )
	{
		OVR_InitDX9Device( materials->VR_GetDevice() );
	}

	// if ( recommended_dxlevel >= 90 && recommended_dxlevel != 110 )
	{
		OVR_DX9EXToDX11( materials->VR_GetSubmitInfo( leftEye ), materials->VR_GetSubmitInfo( rightEye ) );
	}
#endif
}


void CVRViewRender::UpdateEyeRenderTargets()
{
	VRViewParams viewParams = g_VR.GetViewParams();
	if ( leftEye == NULL || rightEye == NULL || lastWidth != viewParams.rtWidth || lastHeight != viewParams.rtHeight || g_VR.NeedD3DInit() )
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
	_rt_right_kv->SetString( "$basetexture", "_rt_vr_right" );

	leftEyeMat  = materials->CreateMaterial( "vrmod_left_rt", _rt_left_kv );
	rightEyeMat = materials->CreateMaterial( "vrmod_right_rt", _rt_right_kv );
}


vrect_t* GetEyeVRect()
{
	if ( !g_VR.active )
		return NULL;

	vrect_t* eyeRect = new vrect_t;

	VRViewParams viewParams = g_VR.GetViewParams();
	
	eyeRect->x = 0;
	eyeRect->y = 0;
	eyeRect->width = viewParams.rtWidth;
	eyeRect->height = viewParams.rtHeight;
	return eyeRect;
}


void CVRViewRender::Render( vrect_t *rect )
{
	VPROF_BUDGET( "CVRViewRender::Render", "CVRViewRender::Render" );

	// see view.cpp in source 2013

	// dumb
	/*if ( g_VR.active )
	{
		vrect_t* eyeRect = GetEyeVRect();
		// m_bIsInEyeRender = true;
		BaseViewRender::Render( eyeRect );
		// m_bIsInEyeRender = false;
	}
	else
	{*/
		BaseViewRender::Render( rect );
	// }
}


ConVar vr_ipd_test("vr_ipd_test", "1.0");


void CVRViewRender::PrepareEyeViewSetup( CViewSetup &eyeView, const CViewSetup &screenView, VREye eye )
{
	if ( g_VR.active )
	{
		VRViewParams viewParams = g_VR.GetViewParams();
		// VRTracker* hmd = g_VR.GetTrackerByName("hmd");

		VMatrix matOffset = g_VR.GetMidEyeFromEye( eye );


		vr::HmdMatrix34_t transformVR = g_pOVR->GetEyeToHeadTransform( ToOVREye(eye) );
		VMatrix transform = VMatrixFrom34( transformVR.m );
		// VMatrix transform = OpenVRToSourceCoordinateSystem( VMatrixFrom34( transformVR.m ), false );

		float ipd = transform[0][3] * 2;
		float eyez = transform[2][3] * 39.37012415030996;

		float ipdTest = vr_ipd_test.GetFloat();
		if (eye == VREye::Left)
			ipdTest = -ipdTest;

		// origin = origin + g_VR.view.angles:Forward()*-(eyez*g_VR.scale)

		
		Vector forward, right, up;
		AngleVectors(screenView.angles, &forward, &right, &up);

		Vector eyeOriginOffset = screenView.origin + forward * -(ipd);

		Vector testOffset = matOffset.GetTranslation();
		testOffset *= ipdTest;
		// matOffset.SetTranslation(testOffset);

		if (eye == VREye::Left)
		{
			// matOffset[1][3] = matOffset[1][3] * -(ipd * 0.5 * 39.37012415030996);
			eyeOriginOffset = right * -(ipd * 0.5 * 39.37012415030996);
		}
		else
		{
			// matOffset[1][3] = matOffset[1][3] * (ipd * 0.5 * 39.37012415030996);
			eyeOriginOffset = right * (ipd * 0.5 * 39.37012415030996);
		}

		// matOffset[1][3] = ipdTest;
		// matOffset[1][3] = (ipd * 39.37012415030996) * ipdTest;
		// matOffset[1][3] *= ipdTest;


		// TODO: this might not be correct?
		// Move eyes to IPD positions.
		VMatrix worldMatrix;
		worldMatrix.SetupMatrixOrgAngles( screenView.origin, screenView.angles );

		VMatrix worldFromEye = worldMatrix * matOffset;

		// Finally convert back to origin+angles.
		MatrixAngles( worldFromEye.As3x4(), eyeView.angles, eyeView.origin );

		eyeView.width = viewParams.rtWidth;
		eyeView.height = viewParams.rtHeight;

		if ( vr_fov_override.GetInt() != 0 )
			eyeView.fov = vr_fov_override.GetInt();
		else
			eyeView.fov = viewParams.left.hFOV;

		eyeView.m_flAspectRatio = viewParams.aspectRatio;
	}
	else if ( vr_test.GetBool() )
	{
		eyeView.width = 512;
		eyeView.height = 512;
		eyeView.m_flAspectRatio = 1.0;
	}
}


void CVRViewRender::RenderView( const CViewSetup &view, int nClearFlags, int whatToDraw )
{
	RenderView( view, view, nClearFlags, whatToDraw );
}

void CVRViewRender::RenderView( const CViewSetup &view, const CViewSetup &hudView, int nClearFlags, int whatToDraw )
{
#if ENGINE_QUIVER || ENGINE_CSGO
	if ( /*m_bIsInEyeRender &&*/ (vr_test.GetBool() || g_VR.active) )
	{
		VPROF_BUDGET( "CVRViewRender::RenderView", "CVRViewRender::RenderView" );

		UpdateEyeRenderTargets();

		if ( leftEyeMat == NULL || rightEyeMat == NULL )
		{
			InitEyeMats();
		}

		CMatRenderContextPtr pRenderContext( materials );

		// draw main screen again
		if ( vr_desktop_eye.GetInt() == 3 )
		{
			BaseViewRender::RenderView( view, hudView, nClearFlags, whatToDraw );
		}

		/*Rect_t src, dst;

		src.x = 0;
		src.y = 0;
		src.width  = view.width;
		src.height = view.height;

		dst.x = 500;
		dst.y = 0;
		dst.width  = leftEye->GetActualWidth();
		dst.height = leftEye->GetActualHeight();*/

		// pRenderContext->GetViewport( src.x, src.y, src.width, src.height );
		// pRenderContext->SetRenderTarget( materials->FindTexture( "_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET ) );
		// pRenderContext->Viewport( dst.x, dst.y, dst.width, dst.height );

		// left eye
		{
			CViewSetup eyeView(view);
			PrepareEyeViewSetup( eyeView, view, VREye::Left );
			int rtWidth = leftEye->GetActualWidth();
			int rtHeight = leftEye->GetActualHeight();

			// pRenderContext->SetRenderTarget( leftEye );

			render->Push3DView( pRenderContext, eyeView, 0, leftEye, GetFrustum() );

			pRenderContext->Viewport( eyeView.x, eyeView.y, eyeView.width, eyeView.height );

			// pRenderContext->PushRenderTargetAndViewport( leftEye, leftEyeDepth, 0, 0, rtWidth, rtHeight );
			BaseViewRender::RenderView( eyeView, eyeView, nClearFlags, RENDERVIEW_UNSPECIFIED );

			g_VR.Submit( leftEye, VREye::Left );

			render->PopView( pRenderContext, GetFrustum() );
			// pRenderContext->PopRenderTargetAndViewport();
		}

		// right eye
		{
			CViewSetup eyeView(view);
			PrepareEyeViewSetup( eyeView, view, VREye::Right );
			int rtWidth = rightEye->GetActualWidth();
			int rtHeight = rightEye->GetActualHeight();

			render->Push3DView( pRenderContext, eyeView, 0, rightEye, GetFrustum() );

			pRenderContext->Viewport( eyeView.x, eyeView.y, eyeView.width, eyeView.height );

			// Rect_t eyeRect = {0, 0, rtWidth, rtHeight};
			// pRenderContext->SetRenderTarget( rightEye );

			// pRenderContext->PushRenderTargetAndViewport( rightEye, rightEyeDepth, 0, 0, rtWidth, rtHeight );
			// pRenderContext->SetRenderTarget( rightEye );
			BaseViewRender::RenderView( eyeView, eyeView, nClearFlags, RENDERVIEW_UNSPECIFIED );

			g_VR.Submit( rightEye, VREye::Right );

			render->PopView( pRenderContext, GetFrustum() );
			// pRenderContext->PopRenderTargetAndViewport();
		}

		// pRenderContext->SetRenderTarget( materials->FindTexture( "_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET ) );
		// pRenderContext->Viewport( src.x, src.y, src.width, src.height );

		if ( !vr_test.GetBool() )
		{
			// g_VR.Submit( leftEye, rightEye );
		}

		if ( vr_debug_rt.GetBool() )
		{
			if ( leftEyeMat )
			{
				// draw left eye
				int width = leftEye->GetActualWidth() / 2;
				int height = leftEye->GetActualHeight() / 2;

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
			}

			if ( rightEyeMat )
			{
				// draw right eye
				int width = rightEye->GetActualWidth() / 2;
				int height = rightEye->GetActualHeight() / 2;

				// int posX = ScreenWidth() / 2 - width / 2;
				// int posY = ScreenHeight() / 2 - height / 2;

				int posX = width;
				int posY = 0;

				pRenderContext->DrawScreenSpaceRectangle(
					rightEyeMat,
					posX, posY,
					width,
					height,
					0, 0,
					width-1,
					height-1,
					width,
					height
				);
			}
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
	}
	else
	#endif
	{
		#if ENGINE_OLD
			BaseViewRender::RenderView( view, nClearFlags, whatToDraw );

			if ( vr_desktop_simulate_perf.GetBool() )
				BaseViewRender::RenderView( view, nClearFlags, whatToDraw );
		#else
			BaseViewRender::RenderView( view, view, nClearFlags, whatToDraw );

			if ( vr_desktop_simulate_perf.GetBool() )
				BaseViewRender::RenderView( view, view, nClearFlags, whatToDraw );
		#endif
	}
}


