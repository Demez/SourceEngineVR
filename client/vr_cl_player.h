#ifndef VR_CL_PLAYER_H
#define VR_CL_PLAYER_H
#ifdef _WIN32
#pragma once
#endif

#include "c_baseplayer.h"
// #include "c_hl2_playerlocaldata.h"
// #include "vr_game_movement.h"
#include "vr_player_shared.h"
#include "vr_tracker.h"
#include "vr_controller.h"
#include "vr.h"


class C_VRBasePlayer;


struct CVRBoneInfo
{
	CVRBoneInfo( C_VRBasePlayer* pPlayer, CStudioHdr* hdr, const char* boneName );
	CVRBoneInfo( C_VRBasePlayer* pPlayer, CStudioHdr* hdr, int boneIndex );

	void InitShared( C_VRBasePlayer* pPlayer, CStudioHdr* hdr );

	bool HasCustomAngles();
	void SetCustomAngles( QAngle angles );

	matrix3x4_t& GetBoneForWrite();
	matrix3x4_t& GetParentBoneForWrite();

	const char* name;
	int index;

	const char* parentName;
	int parentIndex;

	C_VRBasePlayer* ply;
	float dist;  // distance to parent, dot product
	const mstudiobone_t* studioBone;

	CUtlVector< CVRBoneInfo* > childBones;

	bool hasNewAngles;
	QAngle newAngles;

	// coordinates relative to the parent bone
	Vector m_relPos;
	QAngle m_relAng;
};


// TODO: make this a template class
class C_VRBasePlayer : public CVRBasePlayerShared
{
public:
	DECLARE_CLASS( C_VRBasePlayer, CVRBasePlayerShared );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_VRBasePlayer();
	C_VRBasePlayer( const C_VRBasePlayer & );

	// ------------------------------------------------------------------------------------------------
	// Modified Functions if in VR
	// ------------------------------------------------------------------------------------------------
	virtual float                   GetFOV();
	virtual Vector                  EyePosition();
	virtual const QAngle&           EyeAngles();
	virtual const QAngle&           LocalEyeAngles();
	virtual bool					CreateMove( float flInputSampleTime, CUserCmd *pCmd );
	virtual void                    ClientThink();
	virtual void                    PredictCoordinates();

	// ------------------------------------------------------------------------------------------------
	// Playermodel controlling
	// ------------------------------------------------------------------------------------------------
	virtual void                    BuildTransformations( CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed );
	virtual void                    UpdateBoneInformation( CStudioHdr *hdr );
	virtual void                    RecurseFindBones( CStudioHdr *hdr, CVRBoneInfo* boneInfo );

	virtual void                    BuildFirstPersonMeathookTransformations( CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed, const char *pchHeadBoneName );
	virtual void                    BuildTrackerTransformations( CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed, CVRTracker* pTracker );

	virtual void                    BuildArmTransform( CStudioHdr *hdr, CVRTracker* pTracker, CVRBoneInfo* clavicleBoneInfo );
	virtual void                    BuildFingerTransformations( CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed, CVRController* pTracker );

	//virtual void                    HandleRootBoneTransformations( CVRTracker* pTracker, CVRBoneInfo* rootBoneInfo );

	// ------------------------------------------------------------------------------------------------
	// New VR Only Functions
	// ------------------------------------------------------------------------------------------------
	virtual void                    SetViewRotateOffset( float offset );
	virtual void                    AddViewRotateOffset( float offset );
	virtual void                    CorrectViewRotateOffset();
	virtual const QAngle&           EyeAnglesNoOffset();

	virtual CVRBoneInfo*            GetBoneInfo( int index );
	virtual CVRBoneInfo*            GetMainBoneInfo( int index );  // less stuff to iterate through

	// ------------------------------------------------------------------------------------------------
	// Other
	// ------------------------------------------------------------------------------------------------
	virtual void                    OnDataChanged( DataUpdateType_t updateType );
	virtual void                    ExitLadder();

	// Input handling
	// virtual void                    PerformClientSideObstacleAvoidance( float flFrameTime, CUserCmd *pCmd );

	virtual bool                    TestMove( const Vector &pos, float fVertDist, float radius, const Vector &objPos, const Vector &objDir );

	// NOTE: you might need this in Alien Swarm or newer, otherwise the player is stuck at the origin point
	virtual bool                    ShouldRegenerateOriginFromCellBits() const { return true; }


	float                           lastViewHeight;
	float                           viewOffset;
	int                             m_trackerCount;
	int                             m_prevTrackerCount;

	CStudioHdr*                     m_prevModel;
	CUtlVector< CVRBoneInfo* >      m_boneInfoList;
	CUtlVector< CVRBoneInfo* >      m_boneMainInfoList;
	CUtlVector< int >               m_boneOrder;

	Vector                          m_predOrigin;
	QAngle                          m_predAngles;
	CInterpolatedVar<Vector>        m_originHistory;
	CInterpolatedVar<QAngle>        m_anglesHistory;

friend class CVRGameMovement;
};


#define CVRBasePlayer C_VRBasePlayer


#endif
