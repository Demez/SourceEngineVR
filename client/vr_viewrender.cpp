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

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar vr_desktop_eye("vr_desktop_eye", "1", FCVAR_ARCHIVE, "0 - no screen view, 1 - crop left eye, 2 - the right eye, 3 - rerender the desktop view", true, 0.0f, true, 3.0f);
ConVar vr_test_cam("vr_test_cam", "1", FCVAR_ARCHIVE);
ConVar vr_autoupdate_rt("vr_autoupdate_rt", "1", FCVAR_CLIENTDLL);

ConVar vr_dbg_rt("vr_dbg_rt", "0", FCVAR_CLIENTDLL);
ConVar vr_dbg_rt_test("vr_dbg_rt_test", "0", FCVAR_CLIENTDLL);
ConVar vr_dbg_rt_scale("vr_dbg_rt_scale", "0.5", FCVAR_CLIENTDLL);
ConVar vr_one_rt_test("vr_one_rt_test", "0", FCVAR_CLIENTDLL);

extern ConVar vr_waitgetposes_test;


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
		Warning("[VR] failed to create viewrender??? wtf\n");
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

	// g_VR.WaitGetPoses();

	BaseViewRender::Render( rect );

	// TODO: copy stuff here for the entire screen next frame?
	// or figure out where the ui stuff is drawn and override that, then submit?

	// g_VRRenderer.PostRender();

	// }

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


void CVRViewRender::RenderViewEye( CMatRenderContextPtr &pRenderContext, const CViewSetup &view, int nClearFlags, ITexture* eyeTex, VREye eye )
{
	// wtf??
	if ( eyeTex == NULL || g_VRRenderer.m_bUpdateRT )
		return;

	g_VRRenderer.m_bInEyeRender = true;

	CViewSetup eyeView(view);
	g_VRRenderer.PrepareEyeViewSetup( eyeView, eye, view.origin, view.angles );
	int rtWidth = eyeTex->GetActualWidth();
	int rtHeight = eyeTex->GetActualHeight();

	// ITexture* eyeTexDepth = (eye == VREye::Left) ? g_VRRenderer.leftEyeDepth : g_VRRenderer.rightEyeDepth;

	PUSH_VIEW( pRenderContext, eyeView, VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH, eyeTex, GetFrustum() );

	BASE_RENDER_VIEW( eyeView, eyeView, nClearFlags, vr_dbg_rt_test.GetBool() ? RENDERVIEW_DRAWVIEWMODEL : RENDERVIEW_UNSPECIFIED );
	// BASE_RENDER_VIEW( eyeView, eyeView, nClearFlags, RENDERVIEW_DRAWVIEWMODEL );
	g_VRRenderer.RenderShared();

	g_VRRenderer.PostEyeRender( eye );

	POP_VIEW( pRenderContext, GetFrustum() );

	// if ( !vr_dbg_rt_test.GetBool() )
	// 	g_VR.Submit( eyeTex, eye );

	g_VRRenderer.m_bInEyeRender = false;
}


void CVRViewRender::RenderViewDesktop( const CViewSetup &view, const CViewSetup &hudView, int nClearFlags, int whatToDraw )
{
	if ( vr_desktop_eye.GetInt() == 3 )
	{
		CViewSetup desktopView(view);

		if ( vr_test_cam.GetBool() )
			SetupCameraView( desktopView );

		BASE_RENDER_VIEW( desktopView, desktopView, nClearFlags, whatToDraw );
		g_VRRenderer.RenderShared();
	}
}


void CVRViewRender::RenderView( const CViewSetup &view, const CViewSetup &hudView, int nClearFlags, int whatToDraw )
{
	if ( vr_dbg_rt_test.GetBool() || g_VR.active )
	{
		// should be used here?
		if ( vr_waitgetposes_test.GetBool() )
			g_VR.WaitGetPoses();

		g_VRRenderer.UpdateEyeRenderTargets();

		CMatRenderContextPtr pRenderContext( materials );

		g_VRRenderer.PreRender();

		// draw main screen
		RenderViewDesktop( view, hudView, nClearFlags, whatToDraw );
		
		if ( vr_desktop_eye.GetInt() != 3 )
		{
			g_VRRenderer.m_bInTestRender = true;
			RenderViewTest( view, hudView, nClearFlags, whatToDraw );
			g_VRRenderer.m_bInTestRender = false;
		}
		// else
		// {
		// }

		// SetupMain3DView( 0, view, view, nClearFlags, pRenderContext->GetRenderTarget() );
		// CleanupMain3DView( view );

		RenderViewEye( pRenderContext, view, nClearFlags, vr_one_rt_test.GetBool() ? g_VRRenderer.leftEye : g_VRRenderer.rightEye, VREye::Right );
		RenderViewEye( pRenderContext, view, nClearFlags, g_VRRenderer.leftEye, VREye::Left );

		/*g_VRRenderer.m_bInTestRender = true;
		RenderViewTest( view, hudView, nClearFlags, whatToDraw );
		g_VRRenderer.m_bInTestRender = false;*/


		// broken for some reason
		// RenderViewDesktop( view, hudView, nClearFlags, whatToDraw );

		g_VRRenderer.PostRender();

		pRenderContext.SafeRelease();
	}
	else
	{
		BASE_RENDER_VIEW( view, hudView, nClearFlags, whatToDraw );
	}
}


extern ConVar building_cubemaps;


void CVRViewRender::RenderViewTest( const CViewSetup &view, const CViewSetup &hudViewSetup, int nClearFlags, int whatToDraw )
{
	m_UnderWaterOverlayMaterial.Shutdown();					// underwater view will set

	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int slot = GET_ACTIVE_SPLITSCREEN_SLOT();

	m_CurrentView = view;

	C_BaseAnimating::AutoAllowBoneAccess boneaccess( true, true );
	VPROF( "CViewRender::RenderViewTest" );

	/*{
		// HACK: server-side weapons use the viewmodel model, and client-side weapons swap that out for
		// the world model in DrawModel.  This is too late for some bone setup work that happens before
		// DrawModel, so here we just iterate all weapons we know of and fix them up ahead of time.
		MDLCACHE_CRITICAL_SECTION();
		CUtlLinkedList< CBaseCombatWeapon * > &weaponList = C_BaseCombatWeapon::GetWeaponList();
		FOR_EACH_LL( weaponList, it )
		{
			C_BaseCombatWeapon *weapon = weaponList[it];
			if ( !weapon->IsDormant() )
			{
				weapon->EnsureCorrectRenderingModel();
			}
		}
	}*/

	CMatRenderContextPtr pRenderContext( materials );
	ITexture *saveRenderTarget = pRenderContext->GetRenderTarget();
	pRenderContext.SafeRelease(); // don't want to hold for long periods in case in a locking active share thread mode

	/*if ( !m_FreezeParams[ slot ].m_bTakeFreezeFrame && m_FreezeParams[ slot ].m_flFreezeFrameUntil > gpGlobals->curtime )
	{
		CRefPtr<CFreezeFrameView> pFreezeFrameView = new CFreezeFrameView( this );
		pFreezeFrameView->Setup( view );
		AddViewToScene( pFreezeFrameView );

		g_bRenderingView = true;
		AllowCurrentViewAccess( true );
	}
	else*/
	{
		// g_flFreezeFlash[ slot ] = 0.0f;

		// g_bRenderingView = true;

		// RenderPreScene( view );

		// Must be first 
		render->SceneBegin();

		/*if ( !InVRRender() )
		{
			g_pColorCorrectionMgr->UpdateColorCorrection();

			// Send the current tonemap scalar to the material system
			UpdateMaterialSystemTonemapScalar();
		}*/

		// clear happens here probably
		SetupMain3DView( slot, view, hudViewSetup, nClearFlags, saveRenderTarget );

		// g_pClientShadowMgr->UpdateSplitscreenLocalPlayerShadowSkip();

		bool bDrew3dSkybox = false;
		SkyboxVisibility_t nSkyboxVisible = SKYBOX_NOT_VISIBLE;

		// Don't bother with the skybox if we're drawing an ND buffer for the SFM
		/*if ( !view.m_bDrawWorldNormal )
		{
			// if the 3d skybox world is drawn, then don't draw the normal skybox
			if ( true ) // For pix event
			{
				#if PIX_ENABLE
				{
					CMatRenderContextPtr pRenderContext( materials );
					PIXEVENT( pRenderContext, "Skybox Rendering" );
				}
				#endif

				CSkyboxView *pSkyView = new CSkyboxView( this );
				if ( ( bDrew3dSkybox = pSkyView->Setup( view, &nClearFlags, &nSkyboxVisible ) ) != false )
				{
					AddViewToScene( pSkyView );
				}
				SafeRelease( pSkyView );
			}
		}*/

		// Force it to clear the framebuffer if they're in solid space.
		if ( ( nClearFlags & VIEW_CLEAR_COLOR ) == 0 )
		{
			MDLCACHE_CRITICAL_SECTION();
			if ( enginetrace->GetPointContents( view.origin ) == CONTENTS_SOLID )
			{
				nClearFlags |= VIEW_CLEAR_COLOR;
			}
		}

		// PreViewDrawScene( view );

		// Render world and all entities, particles, etc.
		if( !g_pIntroData )
		{
			#if PIX_ENABLE
			{
				CMatRenderContextPtr pRenderContext( materials );
				PIXEVENT( pRenderContext, "ViewDrawScene()" );
			}
			#endif
			ViewDrawSceneTest( bDrew3dSkybox, nSkyboxVisible, view, nClearFlags, VIEW_MAIN, whatToDraw & RENDERVIEW_DRAWVIEWMODEL );
		}
		else
		{
			#if PIX_ENABLE
			{
				CMatRenderContextPtr pRenderContext( materials );
				PIXEVENT( pRenderContext, "ViewDrawScene_Intro()" );
			}
			#endif
			ViewDrawScene_Intro( view, nClearFlags, *g_pIntroData );
		}

		// We can still use the 'current view' stuff set up in ViewDrawScene
		// AllowCurrentViewAccess( true );

		// PostViewDrawScene( view );

		engine->DrawPortals();

		DisableFog();

		// Finish scene
		render->SceneEnd();

		// Draw lightsources if enabled
		render->DrawLights();

		// RenderPlayerSprites();

		/*if ( !building_cubemaps.GetBool() )
		{
			if ( IsDepthOfFieldEnabled() )
			{
				pRenderContext.GetFrom( materials );
				{
					PIXEVENT( pRenderContext, "DoDepthOfField()" );
					DoDepthOfField( view );
				}
				pRenderContext.SafeRelease();
			}

			if ( ( view.m_nMotionBlurMode != MOTION_BLUR_DISABLE ) && ( mat_motion_blur_enabled.GetInt() ) )
			{
				pRenderContext.GetFrom( materials );
				{
					PIXEVENT( pRenderContext, "DoImageSpaceMotionBlur()" );

					if ( !InVRRender() )
					{
						DoImageSpaceMotionBlur( view );
					}
				}
				pRenderContext.SafeRelease();
			}
		}*/

		// Now actually draw the viewmodel
		// DrawViewModels( view, whatToDraw & RENDERVIEW_DRAWVIEWMODEL );

		// DrawUnderwaterOverlay();

		PixelVisibility_EndScene();

		// Draw fade over entire screen if needed
		byte color[4];
		bool blend;
		// GetViewEffects()->GetFadeParams( &color[0], &color[1], &color[2], &color[3], &blend );

		// Store off color fade params to be applied in fullscreen postprocess pass
		// SetViewFadeParams( color[0], color[1], color[2], color[3], blend );

		// Draw an overlay to make it even harder to see inside smoke particle systems.
		// DrawSmokeFogOverlay();

		// Overlay screen fade on entire screen
		// PerformScreenOverlay( view.x, view.y, view.width, view.height );

		// Prevent sound stutter if going slow
		engine->Sound_ExtraUpdate();	

		/*if ( g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE )
		{
			pRenderContext.GetFrom( materials );
			pRenderContext->SetToneMappingScaleLinear(Vector(1,1,1));
			pRenderContext.SafeRelease();
		}*/

		// And here are the screen-space effects

		// Grab the pre-color corrected frame for editing purposes
		// engine->GrabPreColorCorrectedFrame( view.x, view.y, view.width, view.height );

		if ( !InVRRender() )
		{
			// PerformScreenSpaceEffects( view.x, view.y, view.width, view.height );
		}

		// GetClientMode()->DoPostScreenSpaceEffects( &view );

		CleanupMain3DView( view );

		if ( m_FreezeParams[ slot ].m_bTakeFreezeFrame )
		{
			pRenderContext = materials->GetRenderContext();
			pRenderContext->CopyRenderTargetToTextureEx( GetFullscreenTexture(), 0, NULL, NULL );
			pRenderContext.SafeRelease();
			m_FreezeParams[ slot ].m_bTakeFreezeFrame = false;
		}

		pRenderContext = materials->GetRenderContext();
		pRenderContext->SetRenderTarget( saveRenderTarget );
		pRenderContext.SafeRelease();

		// Draw the overlay
		/*if ( m_bDrawOverlay )
		{	   
			// This allows us to be ok if there are nested overlay views
			CViewSetup currentView = m_CurrentView;
			CViewSetup tempView = m_OverlayViewSetup;
			tempView.fov = ScaleFOVByWidthRatio( tempView.fov, tempView.m_flAspectRatio / ( 4.0f / 3.0f ) );
			tempView.m_bDoBloomAndToneMapping = false;				// FIXME: Hack to get Mark up and running
			tempView.m_nMotionBlurMode = MOTION_BLUR_DISABLE;		// FIXME: Hack to get Mark up and running
			m_bDrawOverlay = false;
			RenderView( tempView, hudViewSetup, m_OverlayClearFlags, m_OverlayDrawFlags );
			m_CurrentView = currentView;
		}*/
	}

	// Draw the 2D graphics
	/*m_CurrentView = hudViewSetup;
	pRenderContext = materials->GetRenderContext();
	if ( true )
	{
		PIXEVENT( pRenderContext, "2D Client Rendering" );

		render->Push2DView( hudViewSetup, 0, saveRenderTarget, GetFrustum() );

		Render2DEffectsPreHUD( hudViewSetup );

		if ( whatToDraw & RENDERVIEW_DRAWHUD )
		{
			VPROF_BUDGET( "VGui_DrawHud", VPROF_BUDGETGROUP_OTHER_VGUI );
			// paint the vgui screen
			VGui_PreRender();

			CUtlVector< vgui::VPANEL > vecHudPanels;

			vecHudPanels.AddToTail( VGui_GetClientDLLRootPanel() );

			// This block is suspect - why are we resizing fullscreen panels to be the size of the hudViewSetup
			// which is potentially only half the screen
			if ( GET_ACTIVE_SPLITSCREEN_SLOT() == 0 )
			{
				vecHudPanels.AddToTail( VGui_GetFullscreenRootVPANEL() );

				#if defined( TOOLFRAMEWORK_VGUI_REFACTOR )
				vecHudPanels.AddToTail( enginevgui->GetPanel( PANEL_GAMEUIDLL ) );
				#endif
				vecHudPanels.AddToTail( enginevgui->GetPanel( PANEL_CLIENTDLL_TOOLS ) );
			}

			PositionHudPanels( vecHudPanels, hudViewSetup );

			// The crosshair, etc. needs to get at the current setup stuff
			AllowCurrentViewAccess( true );

			// Draw the in-game stuff based on the actual viewport being used
			render->VGui_Paint( PAINT_INGAMEPANELS );

			AllowCurrentViewAccess( false );

			VGui_PostRender();

			GetClientMode()->PostRenderVGui();
			pRenderContext->Flush();
		}

		CDebugViewRender::Draw2DDebuggingInfo( hudViewSetup );

		Render2DEffectsPostHUD( hudViewSetup );

		// g_bRenderingView = false;

		// We can no longer use the 'current view' stuff set up in ViewDrawScene
		AllowCurrentViewAccess( false );

		CDebugViewRender::GenerateOverdrawForTesting();

		render->PopView( GetFrustum() );
	}*/
	pRenderContext.SafeRelease();

	// g_WorldListCache.Flush();

	m_CurrentView = view;
}


extern void SetupCurrentView( const Vector &vecOrigin, const QAngle &angles, view_id_t viewID, bool bDrawWorldNormal = false, bool bCullFrontFaces = false );
// extern void SetClearColorToFogColor();
extern void AllowCurrentViewAccess( bool allow );
extern void FinishCurrentView();


void CVRViewRender::ViewDrawSceneTest( bool bDrew3dSkybox, SkyboxVisibility_t nSkyboxVisible, const CViewSetup &view, 
								int nClearFlags, view_id_t viewID, bool bDrawViewModel, int baseDrawFlags, ViewCustomVisibility_t *pCustomVisibility )
{
	VPROF( "CViewRender::ViewDrawScene" );

	// this allows the refract texture to be updated once per *scene* on 360
	// (e.g. once for a monitor scene and once for the main scene)
	// g_viewscene_refractUpdateFrame = gpGlobals->framecount - 1;

	g_pClientShadowMgr->PreRender();

	// Shadowed flashlights supported on ps_2_b and up...
	if ( ( viewID == VIEW_MAIN ) && ( !view.m_bDrawWorldNormal ) )
	{
		// On the 360, we call this even when we don't have shadow depth textures enabled, so that
		// the flashlight state gets set up properly
		g_pClientShadowMgr->ComputeShadowDepthTextures( view );
	}

	m_BaseDrawFlags = baseDrawFlags;

	SetupCurrentView( view.origin, view.angles, viewID, view.m_bDrawWorldNormal, view.m_bCullFrontFaces );

	// Invoke pre-render methods
	IGameSystem::PreRenderAllSystems();

	// Start view
	unsigned int visFlags;
	SetupVis( view, visFlags, pCustomVisibility );

	if ( !bDrew3dSkybox && 
		( nSkyboxVisible == SKYBOX_NOT_VISIBLE ) && ( visFlags & IVRenderView::VIEW_SETUP_VIS_EX_RETURN_FLAGS_USES_RADIAL_VIS ) )
	{
		// This covers the case where we don't see a 3dskybox, yet radial vis is clipping
		// the far plane.  Need to clear to fog color in this case.
		nClearFlags |= VIEW_CLEAR_COLOR;
		// SetClearColorToFogColor( );
	}

	bool drawSkybox = false; // r_skybox.GetBool();
	if ( bDrew3dSkybox || ( nSkyboxVisible == SKYBOX_NOT_VISIBLE ) )
	{
		drawSkybox = false;
	}

	ParticleMgr()->IncrementFrameCode();

	// DEMEZ: next step
	DrawWorldAndEntities( drawSkybox, view, nClearFlags, pCustomVisibility );

	// Disable fog for the rest of the stuff
	DisableFog();

	// UNDONE: Don't do this with masked brush models, they should probably be in a separate list
	// render->DrawMaskEntities()

	// Here are the overlays...

	if ( !view.m_bDrawWorldNormal )
	{
		// CGlowOverlay::DrawOverlays( view.m_bCacheFullSceneState );
	}

	// issue the pixel visibility tests
	// if ( IsMainView( CurrentViewID() ) && !view.m_bDrawWorldNormal )
	if ( ( CurrentViewID() == VIEW_MAIN ) && !view.m_bDrawWorldNormal )
	{
		PixelVisibility_EndCurrentView();
	}

	// Draw rain..
	// DrawPrecipitation();


	// Draw volumetrics for shadowed flashlights
	/*if ( r_flashlightvolumetrics.GetBool() && (viewID != VIEW_SHADOW_DEPTH_TEXTURE) && !view.m_bDrawWorldNormal )
	{
		g_pClientShadowMgr->DrawVolumetrics( view );
	}*/


	// Make sure sound doesn't stutter
	engine->Sound_ExtraUpdate();

	// Debugging info goes over the top
	// CDebugViewRender::Draw3DDebuggingInfo( view );

	// Draw client side effects
	// NOTE: These are not sorted against the rest of the frame
	if ( !InVRRender() )
	{
		// clienteffects->DrawEffects( gpGlobals->frametime );	
	}

	// Mark the frame as locked down for client fx additions
	// SetFXCreationAllowed( false );

	// Invoke post-render methods
	IGameSystem::PostRenderAllSystems();

	FinishCurrentView();

	// Free shadow depth textures for use in future view
	if ( ( viewID == VIEW_MAIN ) && ( !view.m_bDrawWorldNormal ) )
	{
		g_pClientShadowMgr->UnlockAllShadowDepthTextures();
	}

	// Set int rendering parameters back to defaults
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_ENABLE_FIXED_LIGHTING, 0 );

	if ( view.m_bCullFrontFaces )
	{
		pRenderContext->FlipCulling( false );
	}
}



