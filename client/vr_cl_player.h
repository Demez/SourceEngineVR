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


// Purpose: abstract bone names for different types of model armatures to be used (like unity's for vrchat avatars)
// not even really started yet lol, maybe this could be part of the player class?
enum class EVRBone
{
	Invalid = 0,
	Head,
	Neck,
	Chest,
	Spine,
	Pelvis,

	LThigh,
	LKnee,
	LFoot,

	RThigh,
	RKnee,
	RFoot,


	LShoulder,
	LUpperArm,
	LForeArm,
	LHand,

	LFingerPinky0,
	LFingerPinky1,
	LFingerPinky2,

	LFingerRing0,
	LFingerRing1,
	LFingerRing2,

	LFingerMiddle0,
	LFingerMiddle1,
	LFingerMiddle2,

	LFingerIndex0,
	LFingerIndex1,
	LFingerIndex2,

	LFingerThumb0,
	LFingerThumb1,
	LFingerThumb2,


	RShoulder,
	RUpperArm,
	RForeArm,
	RHand,

	RFingerPinky0,
	RFingerPinky1,
	RFingerPinky2,

	RFingerRing0,
	RFingerRing1,
	RFingerRing2,

	RFingerMiddle0,
	RFingerMiddle1,
	RFingerMiddle2,

	RFingerIndex0,
	RFingerIndex1,
	RFingerIndex2,

	RFingerThumb0,
	RFingerThumb1,
	RFingerThumb2,
};



enum EVRBoneSystem
{
	Invalid = 0,
	ValveBiped,
	Unity,
};


struct VRIKInfo;


#if ENGINE_QUIVER
#define matrix3x4a_t matrix3x4_t
#endif


struct CVRBoneInfo
{
	CVRBoneInfo( C_VRBasePlayer* pPlayer, CStudioHdr* hdr, const char* boneName );
	CVRBoneInfo( C_VRBasePlayer* pPlayer, CStudioHdr* hdr, int boneIndex );

	void InitShared( C_VRBasePlayer* pPlayer, CStudioHdr* hdr );

	CVRBoneInfo* GetParent();
	CVRBoneInfo* GetChild( EVRBone bone );
	CVRBoneInfo* GetChild( const char* bone );

	void CalcRelativeCoord();
	void ResetCustomStuff();
	bool HasCustomAngles();
	void SetCustomAngles( QAngle angles );
	void SetTargetAng( QAngle angles );
	void SetTargetPos( Vector pos );

	void SetAbsAng( QAngle angles );
	void SetAbsPos( Vector pos );

	void SetTargetCoord( matrix3x4_t coord );

	matrix3x4_t GetCoord();
	QAngle GetAngles();

	matrix3x4a_t& GetBoneForWrite();
	matrix3x4a_t& GetParentBoneForWrite();

	CVRBoneInfo* GetRootBoneInfo();
	const mstudiobone_t* GetRootBone();

	const char* name;
	int index;

	const char* parentName;
	int parentIndex;

	C_VRBasePlayer* ply;
	float dist;  // distance to parent, dot product

	const mstudiobone_t* studioBone;
	const mstudiobone_t* parentStudioBone;

	CUtlVector< CVRBoneInfo* > childBones;

	bool hasNewAngles;
	bool setAsTargetAngle;
	QAngle newAngles;

	bool hasNewPos;
	Vector newPos;

	bool hasNewCoord;

	bool drawChildAxis;

#if ENGINE_ASW
	matrix3x4a_t newCoord;
#else
	matrix3x4_t newCoord;
#endif

	bool hasNewAbsAngles;
	bool hasNewAbsPos;
	QAngle newAbsAngles;
	Vector newAbsPos;

	// coordinates relative to the parent bone

	Vector m_relPos;
	QAngle m_relAng;

	EVRBone bone;
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
	virtual const QAngle&           LocalEyeAngles();
	virtual bool					CreateMove( float flInputSampleTime, CUserCmd *pCmd );
	virtual void                    ClientThink();
	virtual void                    PredictCoordinates();

	// ------------------------------------------------------------------------------------------------
	// Playermodel controlling
	// ------------------------------------------------------------------------------------------------
	virtual void                    BuildTransformations( CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed );
	virtual void                    UpdateBoneInformation( CStudioHdr *hdr );

	virtual void                    BuildFirstPersonMeathookTransformations( CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed, const char *pchHeadBoneName );
	virtual void                    BuildTrackerTransformations( CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed, CVRTracker* pTracker );

	virtual void                    BuildArmTransform( CStudioHdr *hdr, CVRTracker* pTracker, CVRBoneInfo* handBoneInfo, CVRBoneInfo* clavicleBoneInfo );
	virtual void                    BuildArmTransformOld( CStudioHdr *hdr, CVRTracker* pTracker, CVRBoneInfo* clavicleBoneInfo );
	virtual void                    BuildFingerTransformations( CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed, CVRController* pTracker );

	//virtual void                    HandleRootBoneTransformations( CVRTracker* pTracker, CVRBoneInfo* rootBoneInfo );
	virtual void                    RecurseApplyBoneTransforms( CVRBoneInfo* boneInfo );

	virtual void                    SetupConstraints();
	virtual void                    SetupPlayerScale();

	// ------------------------------------------------------------------------------------------------
	// Bone System Functions
	// ------------------------------------------------------------------------------------------------
	virtual void                    LoadModelBones( CStudioHdr* hdr );
	virtual void                    SetupUnityBones();

	virtual void                    SetBone( EVRBone bone, const char* name );
	virtual const char*             GetBoneName( EVRBone bone );
	// virtual int                     GetBoneIndex( EVRBone bone );
	virtual CVRBoneInfo*            GetBoneInfo( EVRBone bone );

	virtual const char*             GetValveBipedBoneName( EVRBone bone );
	virtual const char*             GetUnityBoneName( EVRBone bone );

	virtual CVRBoneInfo*            GetBoneInfo( CVRTracker* pTracker );
	virtual CVRBoneInfo*            GetRootBoneInfo( CVRTracker* pTracker );

	virtual CVRBoneInfo*            GetBoneInfo( int index );
	virtual CVRBoneInfo*            GetRootBoneInfo( int index );  // less stuff to iterate through

	// ------------------------------------------------------------------------------------------------
	// New VR Only Functions
	// ------------------------------------------------------------------------------------------------
	virtual void                    OnVREnabled();
	virtual void                    OnVRDisabled();

	virtual void                    SetViewRotateOffset( float offset );
	virtual void                    AddViewRotateOffset( float offset );
	virtual void                    CorrectViewRotateOffset();
	virtual QAngle                  GetViewRotationAng();

	// ------------------------------------------------------------------------------------------------
	// Other
	// ------------------------------------------------------------------------------------------------
	virtual void                    Spawn();
	virtual CStudioHdr*             OnNewModel();

	virtual void                    OnDataChanged( DataUpdateType_t updateType );
	virtual void                    ExitLadder();

	// Input handling
	// virtual void                    PerformClientSideObstacleAvoidance( float flFrameTime, CUserCmd *pCmd );

	virtual bool                    TestMove( const Vector &pos, float fVertDist, float radius, const Vector &objPos, const Vector &objDir );

	// NOTE: you might need this in Alien Swarm or newer, otherwise the player is stuck at the origin point
	virtual bool                    ShouldRegenerateOriginFromCellBits() const { return true; }

	// ------------------------------------------------------------------------------------------------
	// Vars
	// ------------------------------------------------------------------------------------------------
	float                           lastViewHeight;
	float                           viewOffset;
	int                             m_trackerCount;
	int                             m_prevTrackerCount;

	CStudioHdr*                     m_prevModel;
	CUtlVector< CVRBoneInfo* >      m_boneInfoList;
	CUtlVector< CVRBoneInfo* >      m_boneMainInfoList;

	Vector                          m_predOrigin;
	QAngle                          m_predAngles;
	CInterpolatedVar<Vector>        m_originHistory;
	CInterpolatedVar<QAngle>        m_anglesHistory;

	EVRBoneSystem                   m_boneSystem;
	CUtlMap< EVRBone, const char* > m_bones;

	// should probably be abstracted, but that's more work
	VRIKInfo* m_ikLArm;
	VRIKInfo* m_ikRArm;

friend class CVRGameMovement;
};


C_VRBasePlayer* GetLocalVRPlayer();


#define CVRBasePlayer C_VRBasePlayer


#endif
