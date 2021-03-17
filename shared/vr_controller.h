#pragma once

#ifdef CLIENT_DLL
#include "c_baseentity.h"
#include "c_baseanimating.h"
#define CBaseEntity C_BaseEntity
#define CVRTracker C_VRTracker
#define CVRController C_VRController
#else
#include "baseentity.h"
#include "baseanimating.h"
#endif

#include "vr_tracker.h"
#include "vphysics_interface.h"
class CVRBasePlayerShared;

#include <mathlib/vector.h>
#include <mathlib/vmatrix.h>


// derive from this so we can add save/load data to it
struct vr_shadowcontrol_params_t : public hlshadowcontrol_params_t
{
	DECLARE_SIMPLE_DATADESC();
};


// These trackers are used for the hands only, they allow you to pickup physics props (or more in the future)
class CVRController: public CVRTracker, public IMotionEvent
{
	DECLARE_CLASS( CVRController, CVRTracker )

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

	//---------------------------------------------------------------
	// IMotionEvent functions
	//---------------------------------------------------------------
	virtual void                AttachEntity( CBaseEntity *pEntity, IPhysicsObject *pPhys, bool bIsMegaPhysCannon, const Vector &vGrabPosition, bool bUseGrabPosition );
	virtual void                DetachEntity( bool bClearVelocity );
	virtual simresult_e	        Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular );
	virtual float               GetLoadWeight( void ) const { return m_flLoadWeight; }

	//---------------------------------------------------------------
	// vars
	//---------------------------------------------------------------
	CBaseEntity*                m_pLastUseEntity;
	CBaseEntity*                m_pGrabbedObject;
	bool                        m_bIsRightHand;

	IPhysicsMotionController*   m_pController;
	vr_shadowcontrol_params_t   m_shadow;
	float                       m_flLoadWeight;
	float                       m_contactAmount;
	float                       m_timeToArrive;
	float                       m_frameCount;
	Vector                      m_attachedPositionObjectSpace;

	Vector                      m_grabbedObjectPos;
	QAngle                      m_grabbedObjectAng;
	Vector                      m_grabbedObjectLocalPos;
	QAngle                      m_grabbedObjectLocalAng;
	Vector                      m_grabbedObjectPosDiff;
	QAngle                      m_grabbedObjectAngDiff;

	Vector                      m_handPos;
	QAngle                      m_handAng;
	Vector                      m_lastHandPos;
	QAngle                      m_lastHandAng;
};



// TODO: setup later in another file
class CVRWeaponController: public CVRController
{
};


