#include "cbase.h"
#include "vr.h"
#include "vr_viewrender.h"
#include "vr_renderer.h"
#include "vr_cl_player.h"
#include "rendertexture.h"
#include "iclientmode.h"

#include "ivieweffects.h"
#include "viewpostprocess.h"
#include "smoke_fog_overlay.h"
#include "view.h"
#include "view_scene.h"
#include "glow_overlay.h"
#include "clientsideeffects.h"
#include "viewdebug.h"

#include "renderparm.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"

#if DXVK_VR
#include "vr_dxvk.h"
extern IDXVK_VRSystem* g_pDXVK;
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


static ConVar cl_drawmonitors( "cl_drawmonitors", "1" );
static ConVar r_skybox( "r_skybox","1", FCVAR_CHEAT, "Enable the rendering of sky boxes" );


// ===========================================================
// GARBAGE



extern void SetupCurrentView( const Vector &vecOrigin, const QAngle &angles, view_id_t viewID, bool bDrawWorldNormal = false, bool bCullFrontFaces = false );
// extern void SetClearColorToFogColor();
// extern void AllowCurrentViewAccess( bool allow );
extern void FinishCurrentView();
extern bool IsMainView( view_id_t id );


static void SetClearColorToFogColor()
{
	unsigned char ucFogColor[3];
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->GetFogColor( ucFogColor );
	if( g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_INTEGER )
	{
		// @MULTICORE (toml 8/16/2006): Find a way to not do this twice in eye above water case
		float scale = LinearToGammaFullRange( pRenderContext->GetToneMappingScaleLinear().x );
		ucFogColor[0] *= scale;
		ucFogColor[1] *= scale;
		ucFogColor[2] *= scale;
	}
	pRenderContext->ClearColor4ub( ucFogColor[0], ucFogColor[1], ucFogColor[2], 255 );
}


void CVRViewRender::RenderViewEyeBase( const CViewSetup &view, const CViewSetup &hudViewSetup, int nClearFlags, int whatToDraw, VREye eye )
{
	m_UnderWaterOverlayMaterial.Shutdown();					// underwater view will set

	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int slot = GET_ACTIVE_SPLITSCREEN_SLOT();

	m_CurrentView = view;

	C_BaseAnimating::AutoAllowBoneAccess boneaccess( true, true );
	VPROF( "CVRViewRender::RenderViewEyeBase" );

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

		if ( cl_drawmonitors.GetBool() && ( ( whatToDraw & RENDERVIEW_SUPPRESSMONITORRENDERING ) == 0 ) )
		{
			DrawMonitors( view );	
		}

		// g_bRenderingView = true;

		RenderPreScene( view );

		// Must be first 
		render->SceneBegin();

		if ( !InVRRender() )
		{
			g_pColorCorrectionMgr->UpdateColorCorrection();

			// Send the current tonemap scalar to the material system
			// UpdateMaterialSystemTonemapScalar();
		}

		// clear happens here probably
		SetupMain3DView( slot, view, hudViewSetup, nClearFlags, saveRenderTarget );

		g_pClientShadowMgr->UpdateSplitscreenLocalPlayerShadowSkip();

		bool bDrew3dSkybox = false;
		SkyboxVisibility_t nSkyboxVisible = SKYBOX_NOT_VISIBLE;

		// Don't bother with the skybox if we're drawing an ND buffer for the SFM
		if ( !view.m_bDrawWorldNormal )
		{
			// if the 3d skybox world is drawn, then don't draw the normal skybox
			CSkyboxView *pSkyView = new CSkyboxView( this );
			if ( ( bDrew3dSkybox = pSkyView->Setup( view, &nClearFlags, &nSkyboxVisible ) ) != false )
			{
				AddViewToScene( pSkyView );
			}
			SafeRelease( pSkyView );
		}

		// Force it to clear the framebuffer if they're in solid space.
		if ( ( nClearFlags & VIEW_CLEAR_COLOR ) == 0 )
		{
			MDLCACHE_CRITICAL_SECTION();
			if ( enginetrace->GetPointContents( g_VRRenderer.m_viewOrigin ) == CONTENTS_SOLID )
			{
				nClearFlags |= VIEW_CLEAR_COLOR;
			}
		}

		PreViewDrawScene( view );

		// Render world and all entities, particles, etc.
		if( !g_pIntroData )
		{
			ViewDrawSceneEye( bDrew3dSkybox, nSkyboxVisible, view, nClearFlags, VIEW_MAIN );
		}
		else
		{
			// VR TODO: make a vr version of this if needed
			ViewDrawScene_Intro( view, nClearFlags, *g_pIntroData );
		}

		// We can still use the 'current view' stuff set up in ViewDrawScene
		AllowCurrentViewAccess( true );

		PostViewDrawScene( view );

		engine->DrawPortals();

		DisableFog();

		// Finish scene
		render->SceneEnd();

		// Draw lightsources if enabled
		render->DrawLights();

		RenderPlayerSprites();

#if 0
		if ( IsDepthOfFieldEnabled() )
		{
			pRenderContext.GetFrom( materials );
			{
				PIXEVENT( pRenderContext, "DoDepthOfField()" );
				DoDepthOfField( view );
			}
			pRenderContext.SafeRelease();
		}

		if ( ( view.m_nMotionBlurMode != MOTION_BLUR_DISABLE ) && ( mat_motion_blur_enabled.GetInt() ) && !InVRRender() )
		{
			pRenderContext.GetFrom( materials );
			{
				PIXEVENT( pRenderContext, "DoImageSpaceMotionBlur()" );
				DoImageSpaceMotionBlur( view );
			}
			pRenderContext.SafeRelease();
		}
#endif

		// Now actually draw the viewmodel
		// DrawViewModels( view, whatToDraw & RENDERVIEW_DRAWVIEWMODEL );

		DrawUnderwaterOverlay();

		PixelVisibility_EndScene();

		// Draw fade over entire screen if needed
		byte color[4];
		bool blend;
		GetViewEffects()->GetFadeParams( &color[0], &color[1], &color[2], &color[3], &blend );

		// Store off color fade params to be applied in fullscreen postprocess pass
		SetViewFadeParams( color[0], color[1], color[2], color[3], blend );

		// Draw an overlay to make it even harder to see inside smoke particle systems.
		DrawSmokeFogOverlay();

		// Overlay screen fade on entire screen
		PerformScreenOverlay( view.x, view.y, view.width, view.height );

		// Prevent sound stutter if going slow
		engine->Sound_ExtraUpdate();	

		if ( g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE )
		{
			pRenderContext.GetFrom( materials );
			pRenderContext->SetToneMappingScaleLinear(Vector(1,1,1));
			pRenderContext.SafeRelease();
		}

		if ( view.m_bDoBloomAndToneMapping && !InVRRender() )
		{
			pRenderContext.GetFrom( materials );
			{
				static bool bAlreadyShowedLoadTime = false;
				
				if ( ! bAlreadyShowedLoadTime )
				{
					bAlreadyShowedLoadTime = true;
					if ( CommandLine()->CheckParm( "-timeload" ) )
					{
						Warning( "time to initial render = %f\n", Plat_FloatTime() );
					}
				}

				PIXEVENT( pRenderContext, "DoEnginePostProcessing()" );

				bool bFlashlightIsOn = false;
				C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
				if ( pLocal )
				{
					bFlashlightIsOn = pLocal->IsEffectActive( EF_DIMLIGHT );
				}

				DoEnginePostProcessing( view.x, view.y, view.width, view.height, bFlashlightIsOn );
			}
			pRenderContext.SafeRelease();
		}

		// And here are the screen-space effects

		// Grab the pre-color corrected frame for editing purposes
		engine->GrabPreColorCorrectedFrame( view.x, view.y, view.width, view.height );
		
		if ( !InVRRender() )
		{
			PerformScreenSpaceEffects( view.x, view.y, view.width, view.height );
		}

		GetClientMode()->DoPostScreenSpaceEffects( &view );

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
		if ( m_bDrawOverlay )
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
		}
	}

	// VR TODO: maybe set this up later?
#if 0
	// Draw the 2D graphics
	m_CurrentView = hudViewSetup;
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

		g_bRenderingView = false;

		// We can no longer use the 'current view' stuff set up in ViewDrawScene
		AllowCurrentViewAccess( false );

		CDebugViewRender::GenerateOverdrawForTesting();

		render->PopView( GetFrustum() );
	}
#endif

	pRenderContext.SafeRelease();

	g_WorldListCache.Flush();

	m_CurrentView = view;
}


void CVRViewRender::ViewDrawSceneEye( bool bDrew3dSkybox, SkyboxVisibility_t nSkyboxVisible, const CViewSetup &view, 
								int nClearFlags, view_id_t viewID, int baseDrawFlags, ViewCustomVisibility_t *pCustomVisibility )
{
	VPROF( "CViewRender::ViewDrawScene" );

	// this allows the refract texture to be updated once per *scene* on 360
	// (e.g. once for a monitor scene and once for the main scene)
	g_viewscene_refractUpdateFrame = gpGlobals->framecount - 1;

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
		SetClearColorToFogColor( );
	}

	bool drawSkybox = r_skybox.GetBool();
	if ( bDrew3dSkybox || ( nSkyboxVisible == SKYBOX_NOT_VISIBLE ) )
	{
		drawSkybox = false;
	}

	ParticleMgr()->IncrementFrameCode();

	// hmm
	DrawWorldAndEntities( drawSkybox, view, nClearFlags, pCustomVisibility );

	// Disable fog for the rest of the stuff
	DisableFog();

	// UNDONE: Don't do this with masked brush models, they should probably be in a separate list
	// render->DrawMaskEntities()

	// Here are the overlays...

	if ( !view.m_bDrawWorldNormal )
	{
		CGlowOverlay::DrawOverlays( view.m_bCacheFullSceneState );
	}

	// issue the pixel visibility tests
	if ( IsMainView( CurrentViewID() ) && !view.m_bDrawWorldNormal )
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
	CDebugViewRender::Draw3DDebuggingInfo( view );

	// Draw client side effects
	// NOTE: These are not sorted against the rest of the frame
	if ( !InVRRender() )
	{
		// clienteffects->DrawEffects( gpGlobals->frametime );	
	}

	// Mark the frame as locked down for client fx additions
	SetFXCreationAllowed( false );

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
