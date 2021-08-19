#pragma once
#include "cbase.h"
#include "vr.h"
#include "view_shared.h"

class CVRController;

class CVRRenderThread: public CThread
{
public:
	virtual int Run();

	bool shouldStop;
};


// wow yet another tracker class, smh
class CVRTrackerRender
{
public:
	CVRTrackerRender( VRHostTracker* tracker );
	~CVRTrackerRender();

	void Draw();
	void LoadModel();

	Vector GetPointPos();

	model_t* m_model;
	VRHostTracker* m_tracker;
	Vector m_prevPointDir;
};


// ====================================================
// VR World Renderer for when not in-game
// ====================================================
class CVRRenderer
{
public:
	CVRRenderer();
	~CVRRenderer();

	void Init();
	void InitMaterials();
	void InitEyeRenderTargets();
	void UpdateEyeRenderTargets();
	void UnloadMaterials();
	
	void Render();
	void RenderEye( VREye eye );
	void RenderBase();

	// shared across this and CVRViewRender
	void RenderShared();
	void PreRender();
	void PostRender();
	void PostEyeRender( VREye eye );

	void PrepareEyeViewSetup( CViewSetup &eyeView, VREye eye, const Vector& origin, const QAngle& angles );

	void DrawControllerPointer( CVRController* controller );
	void DrawScreen();
	void DrawCroppedView();
	void DrawDebugEyeRenderTarget( IMaterial* eyeMat, int pos );

	Vector TrackerOrigin( VRHostTracker* tracker );
	// Vector TrackerAngles();

	Vector ViewOrigin();
	QAngle ViewAngles();

	bool ShouldDrawTracker( EVRTracker tracker );
	CVRTrackerRender* GetTrackerRender( VRHostTracker* tracker );

	CachedRenderInfo_t* CreateRenderInfo();

	ITexture *leftEye;
	ITexture *rightEye;

	IMaterial *leftEyeMat;
	IMaterial *rightEyeMat;

	IMaterial *screenMat;

	int lastWidth;
	int lastHeight;

	bool m_bInEyeRender;
	bool m_bInTestRender;
	bool m_bInMap;
	bool m_bUpdateRT;

	int m_totalRenderViews;  // all render view calls we will draw
	int m_renderViewCount;   // how many have been called

	Vector m_viewOrigin;

	CUtlVector< CachedRenderInfo_t* > m_renderInfos;

	CUtlVector< CVRTrackerRender* > m_trackers;
};


extern CVRRenderThread* g_VRRenderThread;
extern CVRRenderer g_VRRenderer;
extern CVRRenderer* VRRenderer();

extern bool DrawingVREyes();
extern bool FirstEyeRender();

#if ENGINE_CSGO
#define PUSH_VIEW( pRenderContext, eyeView, nFlags, eyeTex, frustum ) render->Push3DView( pRenderContext, eyeView, nFlags, eyeTex, frustum )
#define POP_VIEW( pRenderContext, frustum ) render->PopView( pRenderContext, frustum )
#else
#define PUSH_VIEW( pRenderContext, eyeView, nFlags, eyeTex, frustum ) render->Push3DView( eyeView, nFlags, eyeTex, frustum )
#define POP_VIEW( pRenderContext, frustum ) render->PopView( frustum )
#endif


