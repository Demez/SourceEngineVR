#include "cbase.h"
#include "vr.h"
#include "vr_viewrender.h"
#include "vr_renderer.h"
#include "vr_cl_player.h"
#include "rendertexture.h"
#include "iclientmode.h"
#include "renderparm.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "tier1/fmtstr.h"

#if DXVK_VR
#include "vr_dxvk.h"
extern IDXVK_VRSystem* g_pDXVK;
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


ConVar vr_desktop_eye("vr_desktop_eye", "1", FCVAR_ARCHIVE, "0 - no screen view, 1 - crop left eye, 2 - the right eye, 3 - rerender the desktop view", true, 0.0f, true, 3.0f);
ConVar vr_test_cam("vr_test_cam", "1", FCVAR_ARCHIVE);
ConVar vr_autoupdate_rt("vr_autoupdate_rt", "1", FCVAR_CLIENTDLL);

// enabled until i look into and figure out what causes this to break in release
ConVar vr_no_renderable_caching("vr_no_renderable_caching", "1", FCVAR_CLIENTDLL);
ConVar vr_no_worldlist_caching("vr_no_worldlist_caching", "0", FCVAR_CLIENTDLL);

ConVar vr_dbg_rt("vr_dbg_rt", "0", FCVAR_CLIENTDLL);
ConVar vr_dbg_rt_test("vr_dbg_rt_test", "0", FCVAR_CLIENTDLL);
// ConVar vr_dbg_rt_scale("vr_dbg_rt_scale", "0.5", FCVAR_CLIENTDLL);
ConVar vr_one_rt_test("vr_one_rt_test", "0", FCVAR_CLIENTDLL);

extern ConVar vr_waitgetposes_test;
extern ConVar vr_spew_timings;


static CVRViewRender g_ViewRender;
IViewRender *GetViewRenderInstance()
{
	return &g_ViewRender;
}


CVRViewRender::CVRViewRender()
{
	if (!view)
	{
		view = (IViewRender *)&g_ViewRender;
	}
	else
	{
		Warning("[VR] view is already defined to another instance of viewrender\n");
	}
}


void CVRViewRender::LevelInit()
{
	BaseViewRender::LevelInit();
	// InitEyeRenderTargets();
}


void CVRViewRender::LevelShutdown()
{
	BaseViewRender::LevelShutdown();
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


void CVRViewRender::RenderView( const CViewSetup &view, int nClearFlags, int whatToDraw )
{
	RenderView( view, view, nClearFlags, whatToDraw );
}


// will improve this later, just thrown together for easier ik debugging
void CVRViewRender::SetupCameraView( CViewSetup &camView )
{
	// temporarily set a fixed position based on the player position
	C_VRBasePlayer* pPlayer = GetLocalVRPlayer();

	if ( !pPlayer )
		return;

	Vector pos = pPlayer->GetOriginViewOffset();
	Vector offset(80.0, 0.0, 40.0);

	camView.origin = pos + offset;
	camView.angles = QAngle(0, 180, 0);
}


#if ENGINE_NEW
#define BASE_RENDER_VIEW( view, hudView, nClearFlags, whatToDraw ) BaseViewRender::RenderView( view, hudView, nClearFlags, whatToDraw )
#else
#define BASE_RENDER_VIEW( view, hudView, nClearFlags, whatToDraw ) BaseViewRender::RenderView( view, nClearFlags, whatToDraw )
#endif


#if VR_RENDER_TEST

void CVRViewRender::RenderViewEye( const CViewSetup &view, int nClearFlags )
{
	if ( g_VRRenderer.m_bUpdateRT )
		return;

	g_VRRenderer.m_bInEyeRender = true;
	g_VRRenderer.m_viewOrigin = view.origin;

	CViewSetup eyeView[2] = { view, view };
	g_VRRenderer.PrepareEyeViewSetup( eyeView[0], VREye::Left, view.origin, view.angles );
	g_VRRenderer.PrepareEyeViewSetup( eyeView[1], VREye::Right, view.origin, view.angles );

	CViewSetup hudViews[2] = { eyeView[0], eyeView[1] };

	// BASE_RENDER_VIEW( eyeView, eyeView, nClearFlags, flags );
	RenderViewEyeBase( eyeView, hudViews, nClearFlags, 0 );

	g_VRRenderer.RenderShared();

	g_VRRenderer.m_bInEyeRender = false;

	if ( vr_no_renderable_caching.GetBool() )
		g_VRRenderer.m_renderInfos.PurgeAndDeleteElements();
}

#else

void CVRViewRender::RenderViewEye( CMatRenderContextPtr &pRenderContext, const CViewSetup &view, int nClearFlags, ITexture* eyeTex, VREye eye )
{
	// wtf??
	if ( eyeTex == NULL || g_VRRenderer.m_bUpdateRT )
		return;

	g_VRRenderer.m_bInEyeRender = true;
	g_VRRenderer.m_viewOrigin = view.origin;

	CViewSetup eyeView(view);
	g_VRRenderer.PrepareEyeViewSetup( eyeView, eye, view.origin, view.angles );

	PUSH_VIEW( pRenderContext, eyeView, VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH, eyeTex, GetFrustum() );

	int flags = RENDERVIEW_UNSPECIFIED;

	if ( vr_dbg_rt_test.GetBool() )
		flags |= RENDERVIEW_DRAWVIEWMODEL;

	// use the monitor view from the first renderview call
	if ( g_VRRenderer.m_renderViewCount > 0 )
		flags |= RENDERVIEW_SUPPRESSMONITORRENDERING;

	// BASE_RENDER_VIEW( eyeView, eyeView, nClearFlags, flags );
	RenderViewEyeBase( eyeView, eyeView, nClearFlags, flags, eye );

	g_VRRenderer.RenderShared();

	g_VRRenderer.PostEyeRender( eye );

	POP_VIEW( pRenderContext, GetFrustum() );

	g_VRRenderer.m_bInEyeRender = false;
	g_VRRenderer.m_renderViewCount++;

	if ( vr_no_renderable_caching.GetBool() )
		g_VRRenderer.m_renderInfos.PurgeAndDeleteElements();
}

#endif

void CVRViewRender::RenderViewDesktop( const CViewSetup &view, const CViewSetup &hudView, int nClearFlags, int whatToDraw )
{
	if ( !g_VR.active && !vr_dbg_rt_test.GetBool() )
	{
		BASE_RENDER_VIEW( view, hudView, nClearFlags, whatToDraw );
		// RenderViewEyeBase( view, hudView, nClearFlags, nClearFlags, VREye::Left );
	}
	else if ( vr_desktop_eye.GetInt() == 3 )
	{
		CViewSetup desktopView(view);

		if ( vr_test_cam.GetBool() )
			SetupCameraView( desktopView );

		BASE_RENDER_VIEW( desktopView, desktopView, nClearFlags, whatToDraw );
		// RenderViewEyeBase( desktopView, desktopView, nClearFlags, nClearFlags, VREye::Left );
		g_VRRenderer.RenderShared();

		g_VRRenderer.m_renderViewCount++;
	}

	g_VRRenderer.m_renderInfos.PurgeAndDeleteElements();
	g_WorldListCache.Flush();
}


void CVRViewRender::RenderView( const CViewSetup &view, const CViewSetup &hudView, int nClearFlags, int whatToDraw )
{
	if ( vr_dbg_rt_test.GetBool() || g_VR.active )
	{
#if DXVK_VR
		if ( g_pDXVK ) g_pDXVK->SetRenderTargetActive( true );
#endif
		if ( vr_spew_timings.GetBool() )
			DevMsg("[VR] CALLED RENDERVIEW - HMD ANG: %s\n", VecToString(view.angles));

		g_VRRenderer.UpdateEyeRenderTargets();

		CMatRenderContextPtr pRenderContext( materials );

		g_VRRenderer.PreRender();

#if VR_RENDER_TEST
		RenderViewEye( view, nClearFlags );
#else
		RenderViewEye( pRenderContext, view, nClearFlags, g_VRRenderer.leftEye, VREye::Left );
		RenderViewEye( pRenderContext, view, nClearFlags, vr_one_rt_test.GetBool() ? g_VRRenderer.leftEye : g_VRRenderer.rightEye, VREye::Right );
#endif

		// if ( !vr_no_worldlist_caching.GetBool() )
		{
			g_VRRenderer.m_renderInfos.PurgeAndDeleteElements();
			g_WorldListCache.Flush();
		}

		RenderViewDesktop( view, hudView, nClearFlags, whatToDraw );

		g_VRRenderer.PostRender();

		pRenderContext.SafeRelease();

		// end of frame
		if ( vr_spew_timings.GetBool() )
		{
			DevMsg("---------------------------------------------------\n");
		}
	}
	else
	{
#if DXVK_VR
		if ( g_pDXVK ) g_pDXVK->SetRenderTargetActive( false );
#endif
		g_VRRenderer.PreRender();
		RenderViewDesktop( view, hudView, nClearFlags, whatToDraw );
		// BASE_RENDER_VIEW( view, hudView, nClearFlags, whatToDraw );
	}

}

