#pragma once

#ifdef CLIENT_DLL
#include "c_baseentity.h"
#include "c_baseanimating.h"
#include "beamdraw.h"
#define CBaseEntity C_BaseEntity
// #define CVRTracker C_VRTracker
// #define CVRController C_VRController
#else
#include "baseentity.h"
#include "baseanimating.h"
#endif

#include "vr_tracker.h"
#include "vphysics_interface.h"
#include "engine_defines.h"

class CVRBasePlayerShared;

#include <mathlib/vector.h>
#include <mathlib/vmatrix.h>


#define FINGER_BONE_COUNT 1  // figure this out later


// derive from this so we can add save/load data to it
struct vr_shadowcontrol_params_t : public hlshadowcontrol_params_t
{
	DECLARE_SIMPLE_DATADESC();
};


// These trackers are used for the hands only, they allow you to pickup physics props (or more in the future)
class CVRController: public CVRTracker, public IMotionEvent
{
	typedef CVRTracker BaseClass;

public:

	CVRController();
	virtual ~CVRController();

	virtual void                InitTracker( CmdVRTracker& cmdTracker, CVRBasePlayerShared* pPlayer );
	virtual void                UpdateTracker( CmdVRTracker& cmdTracker );

#ifdef CLIENT_DLL
	// override to draw the controller beam as well
	virtual int                 DrawModel( int flags RENDER_INSTANCE_INPUT );  // i hate this macro
#endif

	//---------------------------------------------------------------
	// Using and Grabbing entities
	//---------------------------------------------------------------
	virtual bool                CanGrabEntities();

	virtual CBaseEntity*        GetLastUseEntity() { return m_pLastUseEntity; }
	virtual CBaseEntity*        FindUseEntity();
	virtual CBaseEntity*        FindGrabEntity();
	virtual CBaseEntity*        FindEntityBase( bool mustBeInPalm = false );

#ifdef GAME_DLL
	virtual bool                Intersects( CBaseEntity *pOther );
#endif

	// should be in the GAME_DLL ifdef
	virtual void                GrabObject( CBaseEntity* pEntity );
	virtual void                DropObject();
	virtual void                DropObjectIfReleased();

	virtual bool                UpdateObject();
	virtual void                SetTargetPosition( const Vector &target, const QAngle &targetOrientation );

	virtual void                DetachEntity( bool bClearVelocity );
	virtual simresult_e	        Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular );

	virtual float               GetLoadWeight( void ) const { return m_flLoadWeight; }
	virtual CBaseEntity*        GetGrabbedObject( void ) { return m_pGrabbedObject; }
	// virtual Vector              GetEntPos( void ) { return m_entLocalPos; }
	// virtual QAngle              GetEntAng( void ) { return m_entLocalAng; }

	//---------------------------------------------------------------
	// other
	//---------------------------------------------------------------
	virtual Vector              GetPalmDir();
	virtual Vector              GetPointDir();
	inline virtual Vector       GetLerpedPointDir() { return m_lerpedPointDir; }
	virtual Vector              GetPointPos();
	virtual QAngle              GetPointAng();
	virtual void                GetFingerBoneNames( const char* fingerBoneNames[FINGER_BONE_COUNT] );
	virtual void                SetDrawPointBeam( bool draw );

	//---------------------------------------------------------------
	// vars
	//---------------------------------------------------------------
	CBaseEntity*                m_pLastUseEntity;
	CBaseEntity*                m_pGrabbedObject;

	Vector2D                    m_fingerCurls[5];

	IPhysicsMotionController*   m_pController;
	vr_shadowcontrol_params_t   m_shadow;
	float                       m_flLoadWeight;
	float                       m_contactAmount;
	float                       m_timeToArrive;
	float                       m_frameCount;
	float                       m_savedRotDamping;
	float                       m_savedMass;

	Vector                      m_entLocalPos;
	QAngle                      m_entLocalAng;

	bool                        m_pointerEnabled;
	Vector                      m_lerpedPointDir;

	int                         m_mdl_openxr_aim;

#ifdef CLIENT_DLL
	// bool                        m_drawGrabDebug;

	// test
	Beam_t*                     m_pBeam;
#endif
};



// TODO: setup later in another file
class CVRWeaponController: public CVRController
{
};


