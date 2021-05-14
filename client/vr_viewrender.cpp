#include "cbase.h"
#include "vr.h"
#include "vr_viewrender.h"
#include "vr_cl_player.h"
#include "rendertexture.h"
#include "iclientmode.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar vr_fov_override;

ConVar vr_desktop_eye("vr_desktop_eye", "1", FCVAR_ARCHIVE, "0 - no screen view, 1 - crop left eye, 2 - the right eye, 3 - rerender the desktop view");
ConVar vr_test_cam("vr_test_cam", "1", FCVAR_ARCHIVE);
ConVar vr_nearz("vr_nearz", "5", FCVAR_CLIENTDLL);
ConVar vr_autoupdate_rt("vr_autoupdate_rt", "1", FCVAR_CLIENTDLL);

ConVar vr_dbg_rt("vr_dbg_rt", "0", FCVAR_CLIENTDLL);
ConVar vr_dbg_rt_test("vr_dbg_rt_test", "0", FCVAR_CLIENTDLL);
ConVar vr_dbg_rt_scale("vr_dbg_rt_scale", "0.5", FCVAR_CLIENTDLL);


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
		Warning("[VR] failed to create viewrender??? wtf\n");
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
	if ( !g_VR.active && !vr_dbg_rt_test.GetBool() )
		return;

	// if we are resizing the render targets, free the existing ones
	// Demez TODO: there is an issue here with this: if we change the eye resolution and make new render textures when already active,
	// the render targets sent to the headset just lock up and is never updated after the first time it's sent,
	// not even restarting openvr fixes it, so im not sure what's going on for that to happen yet
	if ( leftEye != NULL )
		leftEye->Release();

	if ( rightEye != NULL )
		rightEye->Release();

	VRViewParams viewParams = g_VR.GetViewParams();
	lastWidth = viewParams.width;
	lastHeight = viewParams.height;

	// needed if alien swarm or csgo
#if ENGINE_NEW
	// i don't care
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

	materials->EndRenderTargetAllocation();

#if ENGINE_NEW
	materials->FinishRenderTargetAllocation();
#endif

#if ENGINE_QUIVER || ENGINE_CSGO 
	// uint dxlevel, recommended_dxlevel;
	// materials->GetDXLevelDefaults( dxlevel, recommended_dxlevel );

	if ( !g_VR.IsDX11() )
	{
		if ( g_VR.NeedD3DInit() )
		{
			g_VR.InitDX9Device( materials->VR_GetDevice() );
		}

		g_VR.DX9EXToDX11( materials->VR_GetSubmitInfo( leftEye ), materials->VR_GetSubmitInfo( rightEye ) );
	}
	
#endif
}


void CVRViewRender::UpdateEyeRenderTargets()
{
	int width, height;
	
	VRViewParams viewParams = g_VR.GetViewParams();
	width = viewParams.width;
	height = viewParams.height;

	if ( leftEye == NULL || rightEye == NULL || lastWidth != width || lastHeight != height || g_VR.NeedD3DInit() )
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
	eyeRect->width = viewParams.width;
	eyeRect->height = viewParams.height;
	return eyeRect;
}


void CVRViewRender::Render( vrect_t *rect )
{
	// VPROF_BUDGET( "CVRViewRender::Render", "CVRViewRender::Render" );

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

#define VR_SCALE 39.37012415030996

void CVRViewRender::PrepareEyeViewSetup( CViewSetup &eyeView, const CViewSetup &screenView, VREye eye )
{
	if ( g_VR.active )
	{
		// probably not going to do anything
		VRHostTracker* hmd = g_VR.GetHeadset();
		if ( hmd )
		{
			eyeView.angles = hmd->ang;
			eyeView.angles.y += GetLocalVRPlayer()->viewOffset;
			NormalizeAngles( eyeView.angles );
		}

		VRViewParams viewParams = g_VR.GetViewParams();

		VMatrix matOffset = g_VR.GetMidEyeFromEye( eye );
		
		// Move eyes to IPD positions.
		VMatrix worldMatrix;
		worldMatrix.SetupMatrixOrgAngles( screenView.origin, screenView.angles );

		VMatrix worldFromEye = worldMatrix * matOffset;

		// Finally convert back to origin+angles.
		MatrixAngles( worldFromEye.As3x4(), eyeView.angles, eyeView.origin );

		eyeView.width = viewParams.width;
		eyeView.height = viewParams.height;

		if ( vr_fov_override.GetInt() != 0 )
			eyeView.fov = vr_fov_override.GetInt();
		else
			eyeView.fov = viewParams.fov;

		eyeView.m_flAspectRatio = viewParams.aspect;
	}
	else if ( vr_dbg_rt_test.GetBool() )
	{
		VRViewParams viewParams = g_VR.GetViewParams();
		eyeView.width = viewParams.width;
		eyeView.height = viewParams.height;
		eyeView.m_flAspectRatio = (float)eyeView.width / (float)eyeView.height;
	}

	eyeView.zNear = vr_nearz.GetFloat();

	// eyeView.m_nUnscaledWidth = eyeView.width;
	// eyeView.m_nUnscaledHeight = eyeView.height;
}


// will improve this later, just thrown together for easier ik debugging
void CVRViewRender::SetupCameraView( CViewSetup &camView )
{
	// temporarily set a fixed position based on the player position
	C_VRBasePlayer* pPlayer = GetLocalVRPlayer();

	if ( !pPlayer )
		return;

	Vector pos = pPlayer->GetAbsOrigin();
	Vector offset(64.0, 0.0, 40.0);

	camView.origin = pos + offset;
	camView.angles = QAngle(0, 180, 0);
}


void CVRViewRender::RenderView( const CViewSetup &view, int nClearFlags, int whatToDraw )
{
	RenderView( view, view, nClearFlags, whatToDraw );
}


#if ENGINE_NEW
#define BASE_RENDER_VIEW( view, hudView, nClearFlags, whatToDraw ) BaseViewRender::RenderView( view, hudView, nClearFlags, whatToDraw )
#else
#define BASE_RENDER_VIEW( view, hudView, nClearFlags, whatToDraw ) BaseViewRender::RenderView( view, nClearFlags, whatToDraw )
#endif

#if ENGINE_CSGO
#define PUSH_VIEW( pRenderContext, eyeView, nFlags, eyeTex, frustum ) render->Push3DView( pRenderContext, eyeView, nFlags, eyeTex, frustum )
#define POP_VIEW( pRenderContext, frustum ) render->PopView( pRenderContext, frustum )
#else
#define PUSH_VIEW( pRenderContext, eyeView, nFlags, eyeTex, frustum ) render->Push3DView( eyeView, nFlags, eyeTex, frustum )
#define POP_VIEW( pRenderContext, frustum ) render->PopView( frustum )
#endif


void CVRViewRender::RenderViewEye( CMatRenderContextPtr &pRenderContext, const CViewSetup &view, int nClearFlags, ITexture* eyeTex, VREye eye )
{
	// wtf??
	if ( eyeTex == NULL )
		return;

	CViewSetup eyeView(view);
	PrepareEyeViewSetup( eyeView, view, eye );
	int rtWidth = eyeTex->GetActualWidth();
	int rtHeight = eyeTex->GetActualHeight();

	PUSH_VIEW( pRenderContext, eyeView, 0, eyeTex, GetFrustum() );

	pRenderContext->ClearColor3ub( 0, 0, 0 );
	pRenderContext->ClearBuffers( true, true );

	BASE_RENDER_VIEW( eyeView, eyeView, nClearFlags, RENDERVIEW_UNSPECIFIED );

	if ( !vr_dbg_rt_test.GetBool() )
		g_VR.Submit( eyeTex, eye );

	POP_VIEW( pRenderContext, GetFrustum() );
}


void DrawDebugEyeRenderTarget( CMatRenderContextPtr &pRenderContext, IMaterial* eyeMat, VREye eye )
{
	if ( eyeMat == NULL )
		return;

	VRViewParams viewParams = g_VR.GetViewParams();

	// draw right eye
	int width = viewParams.width * vr_dbg_rt_scale.GetFloat();
	int height = viewParams.height * vr_dbg_rt_scale.GetFloat();

	int posX = eye == VREye::Left ? 0 : width;
	int posY = 0;

	pRenderContext->DrawScreenSpaceRectangle(
		eyeMat,
		posX, posY,
		width, height,
		0, 0,
		width-1, height-1,
		width, height
	);
}


void CVRViewRender::RenderView( const CViewSetup &view, const CViewSetup &hudView, int nClearFlags, int whatToDraw )
{
	if ( vr_dbg_rt_test.GetBool() || g_VR.active )
	{
		// VPROF_BUDGET( "CVRViewRender::RenderView", "CVRViewRender::RenderView" );

		UpdateEyeRenderTargets();

		if ( leftEyeMat == NULL || rightEyeMat == NULL )
		{
			InitEyeMats();
		}

		CMatRenderContextPtr pRenderContext( materials );

		RenderViewEye( pRenderContext, view, nClearFlags, leftEye, VREye::Left );
		RenderViewEye( pRenderContext, view, nClearFlags, rightEye, VREye::Right );

		// draw main screen
		if ( vr_desktop_eye.GetInt() == 3 )
		{
			CViewSetup desktopView(view);

			if ( vr_test_cam.GetBool() )
				SetupCameraView( desktopView );

			BASE_RENDER_VIEW( desktopView, desktopView, nClearFlags, whatToDraw );
		}
		else if ( vr_desktop_eye.GetInt() == 1 || vr_desktop_eye.GetInt() == 2 )
		{
			// crop one of the eyes onto the screen (TODO: setup cropping for width just in case)
			IMaterial* eyeMat = vr_desktop_eye.GetInt() == 1 ? leftEyeMat : rightEyeMat;
			VRViewParams viewParams = g_VR.GetViewParams();

			int scrWidth, scrHeight;
			engine->GetScreenSize( scrWidth, scrHeight );

			int eyeWidth = rightEye->GetActualWidth();
			int eyeHeight = rightEye->GetActualHeight();

			float scrAspect = (float)scrWidth / (float)scrHeight;
			float eyeAspect = (float)eyeWidth / (float)eyeHeight;

			float heightOffsetMult = (1.0 - (scrAspect * eyeAspect)); //  / 2.0;
			float heightOffset = (eyeHeight *- heightOffsetMult) / 2.0;

			// now crop this to the entire screen
			pRenderContext->DrawScreenSpaceRectangle(
				eyeMat,
				0, 0,
				scrWidth, scrHeight,
				0, heightOffset,
				eyeWidth-1, eyeHeight-1 - heightOffset,
				eyeWidth, eyeHeight
			);
		}

		if ( vr_dbg_rt.GetBool() )
		{
			DrawDebugEyeRenderTarget( pRenderContext, leftEyeMat, VREye::Left );
			DrawDebugEyeRenderTarget( pRenderContext, rightEyeMat, VREye::Right );
		}
	}
	else
	{
		BASE_RENDER_VIEW( view, view, nClearFlags, whatToDraw );
	}
}

