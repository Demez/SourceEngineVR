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

	virtual void RenderViewTest( const CViewSetup &view, const CViewSetup &hudView, int nClearFlags, int whatToDraw );
	virtual void ViewDrawSceneTest( bool bDrew3dSkybox, SkyboxVisibility_t nSkyboxVisible, const CViewSetup &view, int nClearFlags, view_id_t viewID, bool bDrawViewModel = false, int baseDrawFlags = 0, ViewCustomVisibility_t *pCustomVisibility = NULL );

	virtual void RenderViewEye( CMatRenderContextPtr &pRenderContext, const CViewSetup &view, int nClearFlags, ITexture* eyeTex, VREye eye );
	virtual void RenderViewDesktop( const CViewSetup &view, const CViewSetup &hudView, int nClearFlags, int whatToDraw );

	// different per engine base
	virtual void RenderViewEyeBase( const CViewSetup &view, const CViewSetup &hudViewSetup, int nClearFlags, int whatToDraw, VREye eye );
	virtual void ViewDrawSceneEye( bool bDrew3dSkybox, SkyboxVisibility_t nSkyboxVisible, const CViewSetup &view, 
								  int nClearFlags, view_id_t viewID, int baseDrawFlags = 0, ViewCustomVisibility_t *pCustomVisibility = NULL );

	// will extend this into it's own class, so you can pickup and move the camera around
	// might need to have that be an entity to make it easy though
	// unless i create a custom vr item pickup class for local use only
	virtual void SetupCameraView( CViewSetup &camView );
};




#endif