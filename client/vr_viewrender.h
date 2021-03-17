#ifndef VR_VIEWRENDER_H
#define VR_VIEWRENDER_H
#pragma once

#include "iviewrender.h"
#include "viewrender.h"

#include "vr.h"

// retarded
#ifndef VR_BASE_VIEW_RENDER
#define VR_BASE_VIEW_RENDER CViewRender
#endif

// use your own viewrender if you want to
// i hope this would be able to just inherit any custom view render and have it work in vr
// but that most likely won't be the case
class CVRViewRender : public VR_BASE_VIEW_RENDER
{
	typedef VR_BASE_VIEW_RENDER BaseViewRender;
public:
	CVRViewRender();

	virtual void LevelInit( void );
	virtual void LevelShutdown( void );

	virtual void Render( vrect_t *rect );

	virtual void RenderView( const CViewSetup &view, int nClearFlags, int whatToDraw );
	virtual void RenderView( const CViewSetup &view, const CViewSetup &hudView, int nClearFlags, int whatToDraw );

	// virtual void RenderViewEye( const CViewSetup &view, int nClearFlags, int whatToDraw, ITexture* rt, ITexture* depth );

	virtual void PrepareEyeViewSetup( CViewSetup &eyeView, const CViewSetup &screenView, VREye eye );

	virtual void UpdateEyeRenderTargets();
	virtual void InitEyeRenderTargets();
	virtual void InitEyeMats();

	ITexture *leftEye;
	ITexture *leftEyeDepth;

	ITexture *rightEye;
	ITexture *rightEyeDepth;

	// dumb
	CTextureReference leftEyeRef;
	CTextureReference leftEyeDepthRef;

	CTextureReference rightEyeRef;
	CTextureReference rightEyeDepthRef;

	IMaterial *leftEyeMat;
	IMaterial *rightEyeMat;

	int lastWidth;
	int lastHeight;

	bool m_bIsInEyeRender;
};




#endif