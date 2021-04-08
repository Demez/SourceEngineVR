#pragma once

#ifdef CLIENT_DLL
#include "c_baseentity.h"
#include "c_baseanimating.h"
#define CBaseEntity C_BaseEntity
// #define CVRTracker C_VRTracker
// #define CVRController C_VRController
#else
#include "baseentity.h"
#include "baseanimating.h"
#endif

#include "vr_tracker.h"
#include "vphysics_interface.h"
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

	virtual void                Spawn();

	virtual void                InitTracker( CmdVRTracker& cmdTracker, CVRBasePlayerShared* pPlayer );
	virtual void                UpdateTracker( CmdVRTracker& cmdTracker );

	//---------------------------------------------------------------
	// Using and Grabbing entities
	//---------------------------------------------------------------
	virtual bool                CanGrabEntities();

	virtual CBaseEntity*        GetLastUseEntity() { return m_pLastUseEntity; }
	virtual CBaseEntity*        FindUseEntity();
	virtual CBaseEntity*        FindGrabEntity();
	virtual CBaseEntity*        FindEntityBase( bool mustBeInPalm = false );

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

	Vector                      m_entLocalPos;
	QAngle                      m_entLocalAng;

	bool                        m_pointerEnabled;
	Vector                      m_prevPointDir;
};



// TODO: setup later in another file
class CVRWeaponController: public CVRController
{
};


