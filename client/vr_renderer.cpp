#include "cbase.h"
#include "vr_renderer.h"
#include "vr.h"
#include "vr_tracker.h"

#if ENGINE_NEW  // well this sucks
#include "tier3/mdlutils.h"
#endif

#include "shaderapi/ishaderapi.h"
#include "vr_player_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


CVRRenderer g_VRRenderer;
CVRRenderThread* g_VRRenderThread;

extern ConVar vr_desktop_eye;
extern ConVar vr_fov_override;
extern ConVar vr_dbg_rt_test;
extern ConVar vr_dbg_rt_scale;
extern ConVar vr_dbg_rt;
extern ConVar vr_one_rt_test;

extern IShaderAPI* g_pShaderAPI;

ConVar vr_nearz("vr_nearz", "5", FCVAR_CLIENTDLL);
ConVar vr_always_draw_trackers("vr_always_draw_trackers", "1", FCVAR_CLIENTDLL);

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
		 	VR_DrawPointer( pos, ang, m_prevPointDir );
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
	if ( ShouldDrawTrackers() )
	{
		for ( int i = 0; i < m_trackers.Count(); i++ )
		{
			m_trackers[i]->Draw();
		}
	}
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
			DrawDebugEyeRenderTarget( screenMat, 0 );
			// DrawDebugEyeRenderTarget( leftEyeMat, 0 );
			// DrawDebugEyeRenderTarget( rightEyeMat, 1 );
		}
	}
}


bool CVRRenderer::ShouldDrawTrackers()
{
	return ( !g_VR.m_inMap || vr_always_draw_trackers.GetBool() );
}


ConVar vr_ipd_test("vr_ipd_test", "1.0");

#define VR_SCALE 39.37012415030996

void CVRRenderer::PrepareEyeViewSetup( CViewSetup &eyeView, VREye eye, const Vector& origin, const QAngle& angles )
{
	if ( g_VR.active )
	{
		VRViewParams viewParams = g_VR.GetViewParams();

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
		VRViewParams viewParams = g_VR.GetViewParams();
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
	if ( !g_VR.active || !vr_dbg_rt_test.GetBool() )
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

	// needed if alien swarm or csgo
#if ENGINE_NEW
	// i don't care
	materials->ReEnableRenderTargetAllocation_IRealizeIfICallThisAllTexturesWillBeUnloadedAndLoadTimeWillSufferHorribly();
#endif

	materials->BeginRenderTargetAllocation();

	ImageFormat format = materials->GetBackBufferFormat();

	leftEye = materials->CreateNamedRenderTargetTextureEx2(
	    "_rt_vr_left",
		lastWidth, lastHeight,
		RT_SIZE_LITERAL, // RT_SIZE_NO_CHANGE,
		format,// IMAGE_FORMAT_RGBA8888,
		MATERIAL_RT_DEPTH_SHARED, // MATERIAL_RT_DEPTH_SEPARATE,
	    TEXTUREFLAGS_RENDERTARGET /*| TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_NOMIP*/,
	    0 );

	rightEye = materials->CreateNamedRenderTargetTextureEx2(
	    "_rt_vr_right",
		lastWidth, lastHeight,
		RT_SIZE_LITERAL, // RT_SIZE_NO_CHANGE,
		// IMAGE_FORMAT_RGBA8888, // materials->GetBackBufferFormat(),
		format, // materials->GetBackBufferFormat(),
		MATERIAL_RT_DEPTH_SHARED, // MATERIAL_RT_DEPTH_NONE,
	    TEXTUREFLAGS_RENDERTARGET /*| TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_NOMIP*/,
	    0 );

	materials->EndRenderTargetAllocation();

#if ENGINE_NEW
	materials->FinishRenderTargetAllocation();
#endif

	m_bUpdateRT = false;

	mat_queue_mode.SetValue( ogMode );

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
	KeyValues *screenMatKV = new KeyValues( "UnlitGeneric" );
	screenMatKV->SetString( "$basetexture", "_rt_FullFrameFB" );
	// screenMatKV->SetString( "$basetexture", "_rt_Fullscreen" );

	leftEyeMat  = materials->CreateMaterial( "vr_left_rt", _rt_left_kv );
	rightEyeMat = materials->CreateMaterial( "vr_right_rt", _rt_right_kv );
	screenMat = materials->CreateMaterial( "vr_screen_rt", screenMatKV );
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
			heightOffset /= scrAspect;

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



