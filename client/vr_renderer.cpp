#include "cbase.h"
#include "vr_renderer.h"
#include "vr.h"
#include "vr_tracker.h"
#include "vr_controller.h"
#include "vr_player_shared.h"
#include "vr_cl_player.h"

#if DXVK_VR
#include "vr_dxvk.h"
extern IDXVK_VRSystem* g_pDXVK;
#endif

#include "debugoverlay_shared.h"
#include "shaderapi/ishaderapi.h"
#include "beamdraw.h"
#include "tier1/fmtstr.h"

// why is this not in the 2007 leak when it's in the orangebox sdk?
#if !ENGINE_QUIVER
#include "tier3/mdlutils.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


CVRRenderer g_VRRenderer;
CVRRenderThread* g_VRRenderThread;

extern ConVar vr_desktop_eye;
extern ConVar vr_fov_override;
extern ConVar vr_dbg_rt_test;
extern ConVar vr_dbg_rt;
extern ConVar vr_one_rt_test;
extern ConVar vr_no_renderable_caching;

ConVar vr_dbg_rt_scale("vr_dbg_rt_scale", "0.5", FCVAR_CLIENTDLL);

extern IShaderAPI* g_pShaderAPI;

ConVar vr_nearz("vr_nearz", "3", FCVAR_CLIENTDLL);
ConVar vr_always_draw_trackers("vr_always_draw_trackers", "0", FCVAR_CLIENTDLL);
ConVar vr_draw_trackers("vr_draw_trackers", "1", FCVAR_CLIENTDLL);
ConVar vr_dbg_point("vr_dbg_point", "1", FCVAR_CHEAT);
ConVar vr_pointer_width("vr_pointer_width", "1.0", FCVAR_ARCHIVE);

extern ConVar vr_pointer_lerp;

CON_COMMAND_F(vr_update_rt, "Force recreate render targets for a different resolution", FCVAR_CLIENTDLL)
{
	// g_VRRenderer.InitEyeRenderTargets();
	g_VRRenderer.m_bUpdateRT = true;
}


int CVRRenderThread::Run()
{
	for (;;)
	{
		if ( !g_VR.active || shouldStop )
			return 0;

		// g_VR.Update( 0 );  // should probably sleep automatically? idk
		// g_VRRenderer.Render();
	}

	return 0;
}


CVRTrackerRender::CVRTrackerRender( VRHostTracker* tracker ) :
	m_tracker(tracker)
{
}

CVRTrackerRender::~CVRTrackerRender()
{
}


static void SetBodygroup( studiohdr_t *pstudiohdr, int &body, int iGroup, int iValue )
{
	if ( !pstudiohdr )
	{
		return;
	}

	if ( iGroup >= pstudiohdr->numbodyparts )
	{
		return;
	}

	mstudiobodyparts_t *pbodypart = pstudiohdr->pBodypart( iGroup );

	if ( iValue >= pbodypart->nummodels )
	{
		return;
	}

	int iCurrent = ( body / pbodypart->base ) % pbodypart->nummodels;

	body = ( body - ( iCurrent * pbodypart->base ) + ( iValue * pbodypart->base ) );
}


ConVar vr_hand_x("vr_hand_x", "0.0", 0);
ConVar vr_hand_y("vr_hand_y", "0.0", 0);
ConVar vr_hand_z("vr_hand_z", "0.0", 0);


void CVRTrackerRender::Draw()
{
#if ENGINE_NEW  // well this sucks

	// maybe use modelrender instead?

	if ( g_pOVR->IsSteamVRDrawingControllers() )
		return;

	if ( !m_model )
		return;

	MDLHandle_t hStudioHdr = modelinfo->GetCacheHandle( m_model );
	studiohdr_t *pStudioHdr = mdlcache->GetStudioHdr( hStudioHdr );

	CMDL	MDL;
	MDL.SetMDL( hStudioHdr );

	SetBodygroup( pStudioHdr, MDL.m_nBody, 0, 0 );
	MDL.m_Color = Color( 255, 255, 255, 255 );

	VMatrix matrix;
	Vector pos; // = g_VRRenderer.TrackerOrigin( m_tracker );
	QAngle ang; // = m_tracker->ang;

	if ( g_VR.m_inMap )
	{
		CVRBasePlayerShared* pPlayer = (CVRBasePlayerShared*)C_BasePlayer::GetLocalPlayer();
		CVRTracker* gameVRTracker = pPlayer->GetTracker( m_tracker->type );

		if ( !gameVRTracker )
			return;

		pos = gameVRTracker->GetAbsOrigin();
		ang = gameVRTracker->GetAbsAngles();
	}
	else
	{
		pos = m_tracker->pos;
		ang = m_tracker->ang;
	}

	pos.x += vr_hand_x.GetFloat();
	pos.y += vr_hand_y.GetFloat();
	pos.z += vr_hand_z.GetFloat();

	matrix.SetupMatrixOrgAngles( pos, ang );

	/*if ( false )
	{
		// maybe i can do this?
		char materialPath[512];
		V_snprintf( materialPath, 512, "%s/%s", pStudioHdr->pCdtexture(0), pStudioHdr->pTexture(0)->pszName());

		IMaterial* modelMaterial = materials->FindMaterial( materialPath, TEXTURE_GROUP_OTHER );
		if ( modelMaterial )
		{
			modelMaterial->SetShader( "UnlitGeneric" );
		}
	}*/

	if ( pStudioHdr )
	{
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		CMatRenderData< matrix3x4_t > rdBoneToWorld( pRenderContext, pStudioHdr->numbones );
		MDL.SetUpBones( matrix.As3x4(), pStudioHdr->numbones, rdBoneToWorld.Base() );
		MDL.Draw( matrix.As3x4(), rdBoneToWorld.Base(), STUDIORENDER_DRAW_NO_SHADOWS );
		// STUDIORENDER_DRAW_NO_SHADOWS
		// STUDIORENDER_DRAW_WIREFRAME
	}
	else
	{
		MDL.Draw( matrix.As3x4() );
	}

	// TODO: this doesn't work on the menu screen, smh
#if 0
	if ( m_tracker->type != EVRTracker::HMD )
	{
		DebugAxis( pos, ang, 5, true, 0.0f );

		if ( m_tracker->type == EVRTracker::LHAND || m_tracker->type == EVRTracker::RHAND )
		 	VR_DrawPointer( pos, ang, m_lerpedPointDir );
	}
#endif
#endif
}


void CVRTrackerRender::LoadModel()
{
	// EVRTracker type = m_tracker->type;

	// const char* modelName = g_VR.GetTrackerModelName( m_tracker->type );
	// ew
	// const char* modelName = m_tracker->device ? m_tracker->device->GetTrackerModelName(m_tracker->type) : g_VR.GetTrackerModelName(m_tracker->type);
	const char* modelName = m_tracker->GetModelName();

	if ( V_strcmp(modelName, "") != 0 )
	{
		m_model = (model_t *)engine->LoadModel( modelName );
	}
	else
	{
		m_model = NULL;
	}

	// m_model = (model_t *)engine->LoadModel( "models/vr/oculus_cv1/controller_left.mdl" );
	// m_model = (model_t *)engine->LoadModel( "models/controller_test.mdl" );

	// test model
	// m_model = (model_t *)engine->LoadModel( "models/props_junk/PopCan01a.mdl" );
}


Vector CVRTrackerRender::GetPointPos()
{
	return m_tracker->pos;
}


// ====================================================
// VR World Renderer for when not in-game
// ====================================================



CVRRenderer* VRRenderer()
{
	return &g_VRRenderer;
}

bool InVRRender()
{
	return g_VRRenderer.m_bInEyeRender;
}

bool InTestRender()
{
	return g_VRRenderer.m_bInTestRender;
}


CVRRenderer::CVRRenderer()
{
	leftEye = NULL;
	rightEye = NULL;

	leftEyeMat = NULL;
	rightEyeMat = NULL;

	m_bInEyeRender = false;
	m_bUpdateRT = false;
}


CVRRenderer::~CVRRenderer()
{
	if ( leftEye != NULL )
		leftEye->Release();

	if ( rightEye != NULL )
		rightEye->Release();

	leftEye = NULL;
	rightEye = NULL;
}


void CVRRenderer::Init()
{
	InitMaterials();
}


void CVRRenderer::Render()
{
	// TODO: do something different for the loading screen, ugh

	UpdateEyeRenderTargets();

	if ( m_bUpdateRT )
		return;

	PreRender();

	RenderEye( VREye::Left );
	RenderEye( VREye::Right );

	PostRender();
}


void CVRRenderer::RenderEye( VREye eye )
{
	m_bInEyeRender = true;
	ITexture* eyeTex = (eye == VREye::Left) ? leftEye : rightEye;

	// ISSUE: no view turning here
	VRHostTracker* hmd = g_VR.GetHeadset();
	
	// no point in even rendering if there is no headset
	if ( hmd == NULL )
	{
		m_bInEyeRender = false;
		return;
	}

	CViewSetup eyeView;
	PrepareEyeViewSetup( eyeView, eye, hmd->pos, hmd->ang );

	// call might not be needed
	// Frustum_t frustum;
	// GeneratePerspectiveFrustum( eyeView.origin, eyeView.angles, eyeView.zNear, eyeView.zFar, eyeView.fov, eyeView.m_flAspectRatio, frustum );

	CMatRenderContextPtr pRenderContext( materials );

	Frustum frustum;
	PUSH_VIEW( pRenderContext, eyeView, VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH, eyeTex, frustum );

	RenderBase();
	RenderShared();

	// if ( !vr_dbg_rt_test.GetBool() )
	//	g_VR.Submit( eyeTex, eye );

	POP_VIEW( pRenderContext, frustum );

	m_bInEyeRender = false;
}


void CVRRenderer::RenderBase()
{
	// DrawScreen();
}


Vector CVRRenderer::TrackerOrigin( VRHostTracker* tracker )
{
	if ( tracker == NULL )
	{
		return vec3_origin;
	}

	if ( g_VR.m_inMap )
	{
		CVRBasePlayerShared* pPlayer = (CVRBasePlayerShared*)C_BasePlayer::GetLocalPlayer();
		CVRTracker* gameTracker = pPlayer->GetTracker( tracker->type );

		if ( gameTracker )
			return gameTracker->GetAbsOrigin();
	}

	return tracker->pos;
}

Vector CVRRenderer::ViewOrigin()
{
	if ( g_VR.m_inMap )
	{
		return TrackerOrigin( g_VR.GetHeadset() );
	}

	return g_VR.GetHeadset() ? g_VR.GetHeadset()->pos : vec3_origin;
}

QAngle CVRRenderer::ViewAngles()
{
	return g_VR.GetHeadset() ? g_VR.GetHeadset()->ang : vec3_angle;
}



/*
P0 = start
P1 = control
P2 = end
P(t) = (1-t)^2 * P0 + 2t(1-t)*P1 + t^2 * P2
*/
void DrawPointerQuadratic( const Vector &start, const Vector &control, const Vector &end, const Vector &color )
{
	int subdivisions = 16;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	Vector prevPos;
	prevPos.Init();

	float t = 0;
	float dt = 1.0 / (float)subdivisions;
	for( int i = 0; i <= subdivisions; i++, t += dt )
	{
		float omt = (1-t);
		float p0 = omt*omt;
		float p1 = 2*t*omt;
		float p2 = t*t;

		Vector curPos = p0 * start + p1 * control + p2 * end;

		if ( !prevPos.IsZero() )
		{
			debugoverlay->AddLineOverlay( prevPos, curPos, color.x, color.y, color.z, true, 0.0f );
		}

		prevPos = curPos;
	}
}

/*
P0 = start
P1 = control
P2 = end
P(t) = (1-t)^2 * P0 + 2t(1-t)*P1 + t^2 * P2
*/
static void DrawBeamQuadraticNew( IMaterial* material, const Vector &start, const Vector &control, const Vector &end, float width, const Vector &color, float scrollOffset )
{
	int subdivisions = 16;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	CBeamSegDraw beamDraw;
	beamDraw.Start( pRenderContext, subdivisions+1, material );

	BeamSeg_t seg;
	seg.m_flWidth = width;

	float t = 0;
	float u = fmod( scrollOffset, 1 );
	float dt = 1.0 / (float)subdivisions;
	for( int i = 0; i <= subdivisions; i++, t += dt )
	{
		float omt = (1-t);
		float p0 = omt*omt;
		float p1 = 2*t*omt;
		float p2 = t*t;

		seg.m_vPos = p0 * start + p1 * control + p2 * end;
		seg.m_flTexCoord = u - t;
		if ( i == 0 || i == subdivisions )
		{
			// HACK: fade out the ends a bit
#if ENGINE_NEW
			seg.m_color.r = seg.m_color.g = seg.m_color.b = 0;
			seg.m_color.a = 255;
#else
			seg.m_vColor.Init();
			seg.m_flAlpha = 255;
#endif
		}
		else
		{
#if ENGINE_NEW
			seg.SetColor( color, 1.0f );
#else
			seg.m_vColor = color;
#endif
		}
		beamDraw.NextSeg( &seg );
	}

	beamDraw.End();
}

// rip this shit from foundryhelpers_client.cpp
// extern void AddCoolLine( const Vector &v1, const Vector &v2, unsigned long iExtraFadeOffset, bool bNegateMovementDir );


// very broken right now still with the beam
void CVRRenderer::DrawControllerPointer( CVRController* controller )
{
	// if ( vr_dbg_point.GetBool() )
	if ( controller->m_pointerEnabled )
	{
		Vector pointDir = controller->GetPointDir();
		Vector lerpedPointDir = controller->GetLerpedPointDir();

		Vector pointPos = controller->GetPointPos();
		Vector color(203, 66, 245);

		if ( vr_dbg_point.GetBool() )
		{
			DrawPointerQuadratic( pointPos, pointPos + (pointDir * 4), pointPos + lerpedPointDir, color );
		}
		// holy shit this is broken
		else
		{
			DrawBeamQuadraticNew( controller->m_beamMaterial, pointPos, pointPos + (pointDir * 4), pointPos + lerpedPointDir, vr_pointer_width.GetFloat(), color, 0.0f );
		}
	}
}


void CVRRenderer::DrawScreen()
{
	unsigned char pColor[4] = { 255, 255, 255, 255 };

	float flWidth = 20.0;
	float flHeight = 20.0;

	int scrWidth, scrHeight;
	materials->GetBackBufferDimensions( scrWidth, scrHeight );
	float aspect = (float)scrWidth / (float)scrHeight;

	float widthRatio = (float)scrWidth / (float)flWidth;
	float heightRatio = (float)scrHeight / (float)flHeight;
	float bestRatio = MIN(widthRatio, heightRatio);

	// flHeight *= bestRatio;
	// flWidth *= bestRatio;

	flHeight = heightRatio / 4;
	flWidth = widthRatio / 4;

	// Generate half-widths
	// float flWidth = 20 / aspect;
	// float flHeight = 20 / aspect;

	Vector vecOrigin;
	if ( !g_VR.m_inMap )
	{
		vecOrigin.Init(64, 0, 16);
	}
	else
	{
		vecOrigin.Init(64, 0, 16);
		vecOrigin += ViewOrigin();
	}

	// Compute direction vectors for the sprite
	Vector fwd, right( 1, 0, 0 ), up( 0, 1, 0 );

	/*VectorSubtract( ViewOrigin(), vecOrigin, fwd );
	float flDist = VectorNormalize( fwd );
	if (flDist >= 1e-3)
	{
		Vector viewRight, viewUp;
		AngleVectors( ViewAngles(), NULL, &viewRight, &viewUp );

		CrossProduct( viewUp, fwd, right );
		flDist = VectorNormalize( right );
		if (flDist >= 1e-3)
		{
			CrossProduct( fwd, right, up );
		}
		else
		{
			// In this case, fwd == g_vecVUp, it's right above or 
			// below us in screen space
			CrossProduct( fwd, viewRight, up );
			VectorNormalize( up );
			CrossProduct( up, fwd, right );
		}
	}*/

	QAngle angles(0, 0, 0);

	// DEMEZ TODO: maybe rotate to point towards head height?
	// or just have a slider option to rotate it maybe, idk
	VMatrix mat, rotateMat, outMat;
	MatrixFromAngles( angles, mat );
	MatrixBuildRotationAboutAxis( rotateMat, Vector( 0, 1, 0 ), 35 );
	MatrixMultiply( mat, rotateMat, outMat );
	MatrixToAngles( outMat, angles );

	AngleVectors( angles, &fwd, &right, &up );

	// up.Init(0, 1.0, 0);
	// right.Init(1, 0, 0);

	CMeshBuilder meshBuilder;
	Vector point;
	CMatRenderContextPtr pRenderContext( materials );

	if ( g_VR.m_inMap )
	{
		pRenderContext->Bind( leftEyeMat );
	}
	else
	{
		pRenderContext->Bind( screenMat );
	}

	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	meshBuilder.Color4ubv (pColor);
	meshBuilder.TexCoord2f (0, 0, 1);
	VectorMA (vecOrigin, -flHeight, up, point);
	VectorMA (point, -flWidth, right, point);
	meshBuilder.Position3fv (point.Base());
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ubv (pColor);
	meshBuilder.TexCoord2f (0, 0, 0);
	VectorMA (vecOrigin, flHeight, up, point);
	VectorMA (point, -flWidth, right, point);
	meshBuilder.Position3fv (point.Base());
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ubv (pColor);
	meshBuilder.TexCoord2f (0, 1, 0);
	VectorMA (vecOrigin, flHeight, up, point);
	VectorMA (point, flWidth, right, point);
	meshBuilder.Position3fv (point.Base());
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ubv (pColor);
	meshBuilder.TexCoord2f (0, 1, 1);
	VectorMA (vecOrigin, -flHeight, up, point);
	VectorMA (point, flWidth, right, point);
	meshBuilder.Position3fv (point.Base());
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
}


void CVRRenderer::RenderShared()
{
	// DrawScreen();

	// draw trackers
	/*for ( int i = 0; i < m_trackers.Count(); i++ )
	{
		if ( ShouldDrawTracker( m_trackers[i]->m_tracker->type ) )
			m_trackers[i]->Draw();
	}

	// DEMEZ TODO: do this for all players somehow?
	C_VRBasePlayer* pPlayer = GetLocalVRPlayer();
	if ( pPlayer )
	{
		for ( int i = 0; i < pPlayer->m_VRTrackers.Count(); i++ )
		{
			if ( pPlayer->m_VRTrackers[i]->IsHand() )
			{
				DrawControllerPointer( (CVRController*)pPlayer->m_VRTrackers[i] );
			}
		}
	}*/
}


void CVRRenderer::PostEyeRender( VREye eye )
{
	if ( !vr_one_rt_test.GetBool() )
		return;

	if ( !vr_dbg_rt_test.GetBool() && !m_bUpdateRT && !g_VR.NeedD3DInit() )
	{
		g_VR.Submit( leftEye, eye );
	}

	if ( vr_dbg_rt.GetBool() )
	{
		DrawDebugEyeRenderTarget( leftEyeMat, eye == VREye::Left ? 0 : 1 );
		// DrawDebugEyeRenderTarget( rightEyeMat, 1 );
		// DrawDebugEyeRenderTarget( leftEyeDepthMat, 2 );
		// DrawDebugEyeRenderTarget( rightEyeDepthMat, 3 );
	}
}


CVRTrackerRender* CVRRenderer::GetTrackerRender( VRHostTracker* tracker )
{
	for ( int i = 0; i < m_trackers.Count(); i++ )
	{
		if ( m_trackers[i]->m_tracker == tracker )
			return m_trackers[i];
	}

	return nullptr;
}


void CVRRenderer::PreRender()
{
	// check tracker count
	for ( int i = 0; i < g_VR.m_currentTrackers.Count(); i++ )
	{
		VRHostTracker* tracker = g_VR.m_currentTrackers[i];

		if ( tracker->type == EVRTracker::HMD )
			continue;

		if ( !GetTrackerRender( tracker ) )
		{
			CVRTrackerRender* trackerRender = new CVRTrackerRender( tracker );
			trackerRender->LoadModel();

			m_trackers.AddToTail( trackerRender );
		}
	}

	m_renderViewCount = 0;
}


void CVRRenderer::PostRender()
{
	DrawCroppedView();

	if ( !vr_one_rt_test.GetBool() )
	{
		if ( !vr_dbg_rt_test.GetBool() && !m_bUpdateRT && !g_VR.NeedD3DInit() )
		{
			g_VR.Submit( leftEye, rightEye );
		}

		if ( vr_dbg_rt.GetBool() )
		{
			DrawDebugEyeRenderTarget( leftEyeMat, 0 );
			DrawDebugEyeRenderTarget( rightEyeMat, 1 );
		}
	}
}


bool CVRRenderer::ShouldDrawTracker( EVRTracker tracker )
{
	if ( tracker == EVRTracker::HMD )
		return false;

	if ( vr_always_draw_trackers.GetBool() )
		return true;

	// if we disable vr in the middle of updating a tracker, OVR could be NULL
	// and then a crash would occur here, not sure if this can happen anywhere else yet
	return vr_draw_trackers.GetBool() && (g_pOVR && !g_pOVR->IsSteamVRDrawingControllers());
}


ConVar vr_ipd_test("vr_ipd_test", "1.0");

#define VR_SCALE 39.37012415030996

void CVRRenderer::PrepareEyeViewSetup( CViewSetup &eyeView, VREye eye, const Vector& origin, const QAngle& angles )
{
	VRViewParams viewParams = g_VR.GetViewParams();

	if ( g_VR.active )
	{
		VMatrix matOffset = g_VR.GetMidEyeFromEye( eye );

		// Move eyes to IPD positions.
		VMatrix worldMatrix;
		worldMatrix.SetupMatrixOrgAngles( origin, angles );

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
		eyeView.width = viewParams.width;
		eyeView.height = viewParams.height;
		eyeView.m_flAspectRatio = (float)eyeView.width / (float)eyeView.height;
	}

	eyeView.zNear = vr_nearz.GetFloat();

	// eyeView.m_nUnscaledWidth = eyeView.width;
	// eyeView.m_nUnscaledHeight = eyeView.height;
}


// not sure if RT_SIZE_LITERAL is even needed tbh
#if !ENGINE_CSGO && !ENGINE_2013
#define RT_SIZE_LITERAL RT_SIZE_NO_CHANGE
#endif


void CVRRenderer::InitEyeRenderTargets()
{
	if ( !g_VR.active && !vr_dbg_rt_test.GetBool() )
	{
		m_bUpdateRT = false;
		return;
	}

	m_bUpdateRT = true;

	static ConVarRef mat_queue_mode("mat_queue_mode");
	static int ogMode = -1;
	int tmpMode = mat_queue_mode.GetInt();

	// wait until next frame so it has time to change probably?
	if ( tmpMode != 0 )
	{
		ogMode = tmpMode;
		mat_queue_mode.SetValue( 0 );
		return;
	}

	Msg("[VR] Creating Eye Render Targets\n");

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

#if DXVK_VR
	if ( g_pDXVK ) g_pDXVK->SetRenderTargetSize( lastWidth, lastHeight );
#endif

	// needed if alien swarm or csgo
#if ENGINE_NEW
	// i don't care
	materials->ReEnableRenderTargetAllocation_IRealizeIfICallThisAllTexturesWillBeUnloadedAndLoadTimeWillSufferHorribly();
#endif

	materials->BeginRenderTargetAllocation();

	ImageFormat format = materials->GetBackBufferFormat();

#if DXVK_VR
	if ( g_pDXVK ) g_pDXVK->NextCreateTextureIsEye( vr::Eye_Left );
#endif

	leftEye = materials->CreateNamedRenderTargetTextureEx2(
	    "_rt_vr_left",
		lastWidth, lastHeight,
		RT_SIZE_NO_CHANGE,
		format,// IMAGE_FORMAT_RGBA8888,
		MATERIAL_RT_DEPTH_SHARED, // MATERIAL_RT_DEPTH_SEPARATE,
	    TEXTUREFLAGS_RENDERTARGET /*| TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_NOMIP*/,
	    0 );

#if DXVK_VR
	if ( g_pDXVK ) g_pDXVK->NextCreateTextureIsEye( vr::Eye_Right );
#endif

	rightEye = materials->CreateNamedRenderTargetTextureEx2(
	    "_rt_vr_right",
		lastWidth, lastHeight,
		RT_SIZE_NO_CHANGE,
		// IMAGE_FORMAT_RGBA8888, // materials->GetBackBufferFormat(),
		format,
		MATERIAL_RT_DEPTH_SHARED, // MATERIAL_RT_DEPTH_NONE,
	    TEXTUREFLAGS_RENDERTARGET /*| TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_NOMIP*/,
	    0 );

	materials->EndRenderTargetAllocation();

#if ENGINE_NEW
	// VR TODO: why the fuck does this crash on dxvk for when changing resolution
	materials->FinishRenderTargetAllocation();
#endif

	m_bUpdateRT = false;

	mat_queue_mode.SetValue( ogMode );

#if !DXVK_VR
#if ENGINE_QUIVER || ENGINE_CSGO 
	// uint dxlevel, recommended_dxlevel;
	// materials->GetDXLevelDefaults( dxlevel, recommended_dxlevel );

	if ( !g_VR.IsDX11() && !vr_dbg_rt_test.GetBool() )
	{
		if ( g_VR.NeedD3DInit() ) 
		{
			g_VR.InitDX9Device( materials->VR_GetDevice() );
		}

		g_VR.DX9EXToDX11( materials->VR_GetSubmitInfo( leftEye ), materials->VR_GetSubmitInfo( rightEye ) );
	}

#elif ENGINE_ASW

	if ( g_VR.active && g_pShaderAPI )
	{
		if ( g_VR.NeedD3DInit() )
		{
			g_VR.InitDX9Device( g_pShaderAPI->VR_GetDevice() );
		}

		g_VR.DX9EXToDX11( g_pShaderAPI->VR_GetSubmitInfo( leftEye ), g_pShaderAPI->VR_GetSubmitInfo( rightEye ) );
	}
	
#endif
#endif

	InitMaterials();
}


void CVRRenderer::UpdateEyeRenderTargets()
{
	int width, height;
	
	VRViewParams viewParams = g_VR.GetViewParams();
	width = viewParams.width;
	height = viewParams.height;

	if ( m_bUpdateRT || leftEye == NULL || rightEye == NULL|| lastWidth != width || lastHeight != height || g_VR.NeedD3DInit() )
	{
		InitEyeRenderTargets();
	}
}


void CVRRenderer::InitMaterials()
{
	UnloadMaterials();

	// for displaying on the screen
	KeyValues *_rt_left_kv = new KeyValues( "UnlitGeneric" );
	_rt_left_kv->SetString( "$basetexture", "_rt_vr_left" );

	KeyValues *_rt_right_kv = new KeyValues( "UnlitGeneric" );
	_rt_right_kv->SetString( "$basetexture", "_rt_vr_right" );

	// TODO: this is not right, there has to be a way to copy the backbuffer
	//KeyValues *screenMatKV = new KeyValues( "UnlitGeneric" );
	//screenMatKV->SetString( "$basetexture", "_rt_FullFrameFB" );
	// screenMatKV->SetString( "$basetexture", "_rt_Fullscreen" );

	leftEyeMat  = materials->CreateMaterial( "vr_left_rt", _rt_left_kv );
	rightEyeMat = materials->CreateMaterial( "vr_right_rt", _rt_right_kv );
	// screenMat = materials->CreateMaterial( "vr_screen_rt", screenMatKV );

	// leftEyeMat = (IMaterial*)engine->LoadModel( "vr/_rt_vr_left.vmt" ); // materials->FindMaterial( "vr/_rt_vr_left", TEXTURE_GROUP_OTHER );
	// rightEyeMat = materials->FindMaterial( "vr/_rt_vr_right", TEXTURE_GROUP_OTHER );
}


void CVRRenderer::UnloadMaterials()
{
	if ( leftEyeMat )
		leftEyeMat->DeleteIfUnreferenced();

	if ( rightEyeMat )
		rightEyeMat->DeleteIfUnreferenced();

	if ( screenMat )
		screenMat->DeleteIfUnreferenced();
}


void CVRRenderer::DrawCroppedView()
{
	if ( rightEye == NULL || leftEye == NULL )
		return;

	if ( vr_desktop_eye.GetInt() == 1 || vr_desktop_eye.GetInt() == 2 )
	{
		CMatRenderContextPtr pRenderContext( materials );

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

		// hack, this cropping code probably isn't perfect if i need this
		if ( vr_dbg_rt_test.GetBool() )
			heightOffset /= (scrAspect/1.5);

		// now crop this to the entire screen
		pRenderContext->DrawScreenSpaceRectangle(
			eyeMat,
			0, 0,
			scrWidth, scrHeight,
			0, heightOffset,
			eyeWidth-1, eyeHeight-1 - heightOffset,
			eyeWidth, eyeHeight
		);

		pRenderContext.SafeRelease();
	}
}


void CVRRenderer::DrawDebugEyeRenderTarget( IMaterial* eyeMat, int pos )
{
	if ( eyeMat == NULL )
		return;

	VRViewParams viewParams = g_VR.GetViewParams();

	// draw right eye
	int width = viewParams.width * vr_dbg_rt_scale.GetFloat();
	int height = viewParams.height * vr_dbg_rt_scale.GetFloat();

	int posX = width * pos;
	int posY = 0;

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->DrawScreenSpaceRectangle(
		eyeMat,
		posX, posY,
		width, height,
		0, 0,
		width-1, height-1,
		width, height
	);
}


CachedRenderInfo_t* CVRRenderer::CreateRenderInfo()
{
	static int i = -1;

	if ( FirstEyeRender() || vr_no_renderable_caching.GetBool() )
	{
		i = -1;

		// doesn't exist, create it
		CachedRenderInfo_t* renderInfo = new CachedRenderInfo_t;
		renderInfo->setupInfo = new SetupRenderInfo_t;

		m_renderInfos.AddToTail( renderInfo );
		return renderInfo;
	}

	i++;
	return m_renderInfos[i];
}


bool DrawingVREyes()
{
	return vr_dbg_rt_test.GetBool() || g_VR.active;
}

// only issue is that the desktop view could be drawing extra stuff
// but is it faster to use the same render list for the eyes on the desktop and draw the extra stuff
// or is it faster to recalculate the render list for the desktop and draw less stuff?
bool FirstEyeRender()
{
	if ( !DrawingVREyes() || vr_no_renderable_caching.GetBool() )
		return true;

	return g_VRRenderer.m_renderViewCount == 0;
}


