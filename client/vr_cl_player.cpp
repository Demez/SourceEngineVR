#include "cbase.h"
#include "vr_cl_player.h"
#include "vr_input.h"

#include "playerandobjectenumerator.h"
#include "engine/ivdebugoverlay.h"
#include "c_ai_basenpc.h"
#include "in_buttons.h"
#include "collisionutils.h"
#include "igamemovement.h"
#include "vphysics/constraints.h"
#include "bone_setup.h"
#include "vr_ik.h"
#include "solidsetdefaults.h"
#include "tier1/fmtstr.h"
#include "con_nprint.h"
#include "vr_ik.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// How fast to avoid collisions with center of other object, in units per second
#define AVOID_SPEED 2000.0f
extern ConVar cl_forwardspeed;
extern ConVar cl_backspeed;
extern ConVar cl_sidespeed;
extern ConVar vr_scale;

ConVar vr_fov_override( "vr_fov_override", "0", FCVAR_CLIENTDLL );
ConVar vr_disable_eye_ang( "vr_disable_eye_ang", "0", FCVAR_CLIENTDLL );
ConVar vr_cl_interp( "vr_interp_player", "0.1", FCVAR_CLIENTDLL );
ConVar vr_cl_hidehead( "vr_cl_hidehead", "30", FCVAR_CLIENTDLL, "Distance to hide the head by, 0 or keeps the head always visible" );
ConVar vr_cl_headdist( "vr_cl_headdist", "0", FCVAR_CLIENTDLL, "Distance to offset the head back by" );
ConVar vr_cl_headheight( "vr_cl_headheight", "80", FCVAR_CLIENTDLL, "height of the head from the origin" );


void CC_SetVRIKEnabled( IConVar *var, const char *pOldValue, float flOldValue );

ConVar vr_ik( "vr_ik", "0", FCVAR_ARCHIVE, "", CC_SetVRIKEnabled);

void CC_SetVRIKEnabled( IConVar *var, const char *pOldValue, float flOldValue )
{
	if ( vr_ik.GetBool() == g_VRIK.m_enabled )
	{
		return;
	}

	if ( vr_ik.GetBool() )
	{
		g_VRIK.Init();
	}
	else
	{
		g_VRIK.DeInit();
	}
}


#if defined( CVRBasePlayer )
	#undef CVRBasePlayer	
#endif

#if ENGINE_OLD
typedef matrix3x4_t matrix3x4a_t;
#endif


BEGIN_PREDICTION_DATA( C_VRBasePlayer )
END_PREDICTION_DATA()

IMPLEMENT_CLIENTCLASS( C_VRBasePlayer, DT_VRBasePlayer, CVRBasePlayer )

BEGIN_RECV_TABLE(C_VRBasePlayer, DT_VRBasePlayer)
END_RECV_TABLE()


extern CMoveData* g_pMoveData;

// iojsadiu9fj
ConVar cl_meathook_neck_pivot_ingame_up( "cl_meathook_neck_pivot_ingame_up", "7.0" );
ConVar cl_meathook_neck_pivot_ingame_fwd( "cl_meathook_neck_pivot_ingame_fwd", "3.0" );


C_VRBasePlayer* GetLocalVRPlayer()
{
	return (C_VRBasePlayer*)C_BasePlayer::GetLocalPlayer();
}


void RotateAroundAxis( QAngle& angles, const Vector axis, float degrees )
{
	/*Vector vectorOfRotation(axis);
	Vector axisOfRotation = Vector(
	1 - vec.x,
	1 - vec.y,
	1 - vec.z
	).Normalized(); // flip axes and normalize
	QAngle rotation(axisOfRotation * 90);

	angle = rotation;*/

	VMatrix mat, rotateMat, outMat;
	MatrixFromAngles( angles, mat );
	MatrixBuildRotationAboutAxis( rotateMat, axis, degrees );
	MatrixMultiply( mat, rotateMat, outMat );
	MatrixToAngles( outMat, angles );

	// Vector outMatPos;
	// MatrixGetColumn( outMat.As3x4(), 3, outMatPos );
	// NDebugOverlay::Axis( outMatPos, angles, 5.0f, true, 0.0f );


	/*VMatrix mat;
	AngleMatrix( angles, mat.As3x4() );
	MatrixBuildRotationAboutAxis( mat, axis, degrees );

	// MatrixAngles( mat.As3x4(), angle );
	MatrixToAngles( mat, angles );*/

	// Vector rotate;
	// VectorYawRotate( direction, degrees, rotate );

	// VectorAngles( rotate, angle );
}


void RotateAroundAxisForward( QAngle& angle, float degrees )
{
	Vector forward;
	AngleVectors( angle, &forward );

	RotateAroundAxis( angle, forward, degrees );
	// RotateAroundAxis( angle, Vector( 1, 0, 0 ), degrees );
}


void RotateAroundAxisUp( QAngle& angle, float degrees )
{
	Vector direction;
	AngleVectors( angle, NULL, NULL, &direction );

	RotateAroundAxis( angle, direction, degrees );
	// RotateAroundAxis( angle, Vector( 0, 0, 1 ), degrees );
}



CVRBoneInfo::CVRBoneInfo( C_VRBasePlayer* pPlayer, CStudioHdr* hdr, const char* boneName )
{
	name = boneName;
	index = pPlayer->LookupBone( name );
	studioBone = hdr->pBone( index );

	InitShared( pPlayer, hdr );
}


CVRBoneInfo::CVRBoneInfo( C_VRBasePlayer* pPlayer, CStudioHdr* hdr, int boneIndex )
{
	studioBone = hdr->pBone( boneIndex );
	name = studioBone->pszName();
	index = boneIndex;

	InitShared( pPlayer, hdr );
}


void CVRBoneInfo::InitShared( C_VRBasePlayer* pPlayer, CStudioHdr* hdr )
{
	ply = pPlayer;
	parentIndex = hdr->boneParent( index );

	if ( parentIndex != -1 )
	{
		parentStudioBone = hdr->pBone( parentIndex );
		parentName = parentStudioBone->pszName();

		CalcRelativeCoord();
	}
	else
	{
		parentName = "";
		dist = 0.0f;
		m_relPos.Init();
		m_relAng.Init();
	}

	hasNewAngles = false;
	hasNewPos = false;
	hasNewCoord = false;
	setAsTargetAngle = false;
	drawChildAxis = false;
	newAngles.Init();
}


CVRBoneInfo* CVRBoneInfo::GetParent()
{
	if ( parentIndex == -1 )
		return nullptr;

	return ply->GetBoneInfo( parentIndex );
}


CVRBoneInfo* CVRBoneInfo::GetChild( EVRBone bone )
{
	return GetChild( ply->GetBoneName( bone ) );
}


CVRBoneInfo* CVRBoneInfo::GetChild( const char* bone )
{
	if ( childBones.Count() == 0 || V_strcmp(bone, "") == 0 )
		return nullptr;

	for (int i = 0; i < childBones.Count(); i++)
	{
		if ( V_strcmp(childBones[i]->name, bone) == 0 )
		{
			return childBones[i];
		}
	}

	return nullptr;
}


void CVRBoneInfo::CalcRelativeCoord()
{
	if ( parentIndex != -1 )
	{
		dist = parentStudioBone->pos.DistTo( studioBone->pos );
		WorldToLocal( GetParentBoneForWrite(), GetBoneForWrite(), m_relPos, m_relAng );
	}

	ResetCustomStuff();
}


// blech
void CVRBoneInfo::ResetCustomStuff()
{
	hasNewAngles = false;
	hasNewPos = false;
	hasNewCoord = false;
	hasNewAbsPos = false;
	hasNewAbsAngles = false;

	newAngles.Init();
	newPos.Init();
	newCoord.Invalidate();
	newAbsPos.Init();
	newAbsPos.Init();
}


bool CVRBoneInfo::HasCustomAngles()
{
	return hasNewAngles;
}


// DEMEZ TODO: remove this
void CVRBoneInfo::SetCustomAngles( QAngle angles )
{
	hasNewAngles = true;
	newAngles = angles;
}


void CVRBoneInfo::SetTargetAng( QAngle ang )
{
	hasNewAngles = true;
	newAngles = ang;
}


void CVRBoneInfo::SetTargetPos( Vector pos )
{
	hasNewPos = true;
	newPos = pos;
}


void CVRBoneInfo::SetAbsAng( QAngle ang )
{
	hasNewAbsAngles = true;
	newAbsAngles = ang;
}


void CVRBoneInfo::SetAbsPos( Vector pos )
{
	hasNewAbsPos = true;
	newAbsPos = pos;
}


void CVRBoneInfo::SetTargetCoord( matrix3x4_t coord )
{
	hasNewCoord = true;
	newCoord = coord;
}


QAngle CVRBoneInfo::GetAngles()
{
	if ( hasNewAngles )
		return newAngles;

	QAngle angles;
	MatrixAngles( GetBoneForWrite(), angles );
	return angles;
}


matrix3x4_t CVRBoneInfo::GetCoord()
{
	if ( hasNewCoord )
		return newCoord;

	return GetBoneForWrite();
}


matrix3x4a_t& CVRBoneInfo::GetBoneForWrite()
{
	return ply->GetBoneForWrite( index );
}

matrix3x4a_t& CVRBoneInfo::GetParentBoneForWrite()
{
	return ply->GetBoneForWrite( parentIndex );
}

CVRBoneInfo* CVRBoneInfo::GetRootBoneInfo()
{
	// DEMEZ TODO: for now, just return the hip, easy way out
	return ply->GetBoneInfo( EVRBone::Pelvis );
}

const mstudiobone_t* CVRBoneInfo::GetRootBone()
{
	return GetRootBoneInfo()->studioBone;
}



//-----------------------------------------------------------------------------
// GENERAL NOTES AND THOUGHTS:
//-----------------------------------------------------------------------------
// thinking of being able to put your weapons on holsters on your body
// or having a simple inventory
// except either would probably be changed with anything using this base class


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
C_VRBasePlayer::C_VRBasePlayer():
	m_originHistory("C_VRBasePlayer::m_originHistory"),
	m_anglesHistory("C_VRBasePlayer::m_anglesHistory")
{
	m_predOrigin.Init();
	m_predAngles.Init();
	m_originHistory.Setup( &m_predOrigin, INTERPOLATE_LINEAR_ONLY );
	m_anglesHistory.Setup( &m_predAngles, INTERPOLATE_LINEAR_ONLY );
}


void C_VRBasePlayer::Spawn()
{
	BaseClass::Spawn();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : updateType - 
//-----------------------------------------------------------------------------
void C_VRBasePlayer::OnDataChanged( DataUpdateType_t updateType )
{
	// Make sure we're thinking
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}

	BaseClass::OnDataChanged( updateType );
}


// ------------------------------------------------------------------------------------------------
// Modified Functions if in VR
// TODO: most of these functions use g_VR, and we can't use that for multiplayer
// ------------------------------------------------------------------------------------------------
float C_VRBasePlayer::GetFOV()
{
	if ( g_VR.active )
	{
		if ( vr_fov_override.GetInt() <= 0 )
		{
			VRViewParams viewParams = g_VR.GetViewParams();
			return viewParams.fov;
		}
		else
		{
			return vr_fov_override.GetInt();
		}
	}
	else
	{
		return BaseClass::GetFOV();
	}
}


const QAngle &C_VRBasePlayer::LocalEyeAngles()
{
	// this is never called wtf
	if ( m_bInVR )
	{
		return m_vrViewAngles;
	}
	else
	{
		return BaseClass::LocalEyeAngles();
	}
}


bool C_VRBasePlayer::CreateMove( float flInputSampleTime, CUserCmd *pCmd )
{
	bool ret = BaseClass::CreateMove( flInputSampleTime, pCmd );

	if ( !IsInAVehicle() )
	{
		// PerformClientSideObstacleAvoidance( TICK_INTERVAL, pCmd );
		// PerformClientSideNPCSpeedModifiers( TICK_INTERVAL, pCmd );
	}

	GetVRInput()->VRMove( flInputSampleTime, pCmd );

	return ret;
}


void C_VRBasePlayer::ClientThink()
{
	BaseClass::ClientThink();

	m_prevTrackerCount = m_trackerCount;
	m_trackerCount = 0;

	PredictCoordinates();

	UpdateTrackers();
}


void C_VRBasePlayer::PredictCoordinates()
{
#if ENGINE_NEW
	float interp = vr_cl_interp.GetFloat();

	m_predOrigin = GetAbsOrigin();
	m_predAngles = GetAbsAngles();

	m_originHistory.NoteChanged( gpGlobals->curtime, gpGlobals->curtime, interp, false );
	m_originHistory.Interpolate( gpGlobals->curtime, interp );

	m_anglesHistory.NoteChanged( gpGlobals->curtime, gpGlobals->curtime, interp, false );
	m_anglesHistory.Interpolate( gpGlobals->curtime, interp );
#endif
}


void C_VRBasePlayer::AddLocalViewRotation( float offset )
{
	m_vrViewRotationLocal += offset;
	CorrectViewRotateOffset();
}


void C_VRBasePlayer::PlayerUse()
{
	if ( !m_bInVR )
	{
		BaseClass::PlayerUse();
		return;
	}

	/*if ( GetLeftHand() )
	{
		GetLeftHand()->m_drawGrabDebug = ( g_pMoveData->m_nButtons & IN_PICKUP_LEFT );
	}

	if ( GetRightHand() )
	{
		GetRightHand()->m_drawGrabDebug = ( g_pMoveData->m_nButtons & IN_PICKUP_RIGHT );
	}*/
}


bool C_VRBasePlayer::ShouldDraw()
{
	if ( g_VR.active && vr_ik.GetBool() )
		return true;

	return BaseClass::ShouldDraw();
}


// ------------------------------------------------------------------------------------------------
// New VR Functions
// ------------------------------------------------------------------------------------------------
void C_VRBasePlayer::CorrectViewRotateOffset()
{
	BaseClass::CorrectViewRotateOffset();

	if ( m_vrViewRotationLocal > 360.0 )
		m_vrViewRotationLocal -= 360.0;
	else if ( m_vrViewRotationLocal < -360.0 )
		m_vrViewRotationLocal += 360.0;

	if ( g_VR.active )
	{
		VRHostTracker* headset = g_VR.GetHeadset();
		if ( headset == NULL )
			return;

		m_vrViewAngles = headset->ang + GetViewRotationAng();
	}
	else
	{
		m_vrViewAngles = BaseClass::EyeAngles() + GetViewRotationAng();
	}
}


QAngle C_VRBasePlayer::GetViewRotationAng()
{
	// i don't like having 2 separate view rotation floats, but i need one on the server for portals
	// unless i can figure out a better way
	// return QAngle(0, m_vrViewRotationLocal + m_vrViewRotation, 0);
	return QAngle(0, m_vrViewRotation, 0);
}


void C_VRBasePlayer::OnVREnabled()
{
	BaseClass::OnVREnabled();
	OnNewModel();
}


void C_VRBasePlayer::OnVRDisabled()
{
	BaseClass::OnVRDisabled();
}


// ===================================================================
// Playermodel controlling
// ===================================================================
CStudioHdr* C_VRBasePlayer::OnNewModel()
{
	CStudioHdr* hdr = BaseClass::OnNewModel();

	if ( m_bInVR )
	{
		SetupPlayerScale();
		LoadModelBones( GetModelPtr() );
	}

	return hdr;
}


CON_COMMAND( vr_test_constraints, "" )
{
	GetLocalVRPlayer()->SetupConstraints();
}

CON_COMMAND( vr_test_scale, "" )
{
	GetLocalVRPlayer()->SetupPlayerScale();
}


void C_VRBasePlayer::SetupPlayerScale()
{
	// UNFINISHED: this ideally would scale the player based on the playermodel height, like in vrchat or neos vr
	if ( g_VR.active && IsLocalPlayer() )
	{
		int headBone = LookupBone( GetBoneName( EVRBone::Head ) );
		if ( headBone != -1 )
		{
			matrix3x4a_t headMat = GetBone( headBone );

			Vector headPos;
			MatrixGetColumn( headMat, 3, headPos );

			g_VR.SetScale( headPos.z - GetAbsOrigin().z );
		}
	}
}


void C_VRBasePlayer::SetupConstraints()
{
	CStudioHdr* hdr = GetModelPtr();

	/*ragdollparams_t params;
	params.modelIndex = ent->GetModelIndex();
	params.pCollide = modelinfo->GetVCollide( params.modelIndex );

	if ( !params.pCollide )
	{
		return;
	}

	params.pStudioHdr = pstudiohdr;
	params.forceVector = forceVector;
	params.forceBoneIndex = forceBone;
	params.forcePosition.Init();
	params.pCurrentBones = pCurrentBonePosition;
	params.jointFrictionScale = 1.0;
	params.allowStretch = false;
	params.fixedConstraints = bFixedConstraints;*/

	if ( physenv == NULL )
	{
		return;
	}

#if 0
	int modelIndex = GetModelIndex();
	cache_ragdoll_t2* cache = ParseRagdollIntoCache( this, hdr, modelinfo->GetVCollide( modelIndex ), modelIndex );

	if ( cache == NULL )
	{
		return;
	}

	float jointFrictionScale = 1.0;
	ragdoll_t ragdoll;

	constraint_groupparams_t group;
	group.Defaults();
	ragdoll.pGroup = physenv->CreateConstraintGroup( group );

	// where you left off: use params here idfk
	// RagdollAddSolids( physenv, ragdoll, params, const_cast<cache_ragdollsolid_t2 *>(cache->GetSolids()), cache->solidCount, cache->GetConstraints(), cache->constraintCount );
	RagdollAddConstraints( physenv, ragdoll, jointFrictionScale, cache->GetConstraints(), cache->constraintCount );
#endif
}


void C_VRBasePlayer::LoadModelBones( CStudioHdr* hdr )
{
	if ( hdr == NULL )
		return;

	// DEMEZ TODO: either have this load and read a keyvalues file
	// or look through all the bones manually and try to figure out what bone is what

	// try ValveBiped first
	if ( LookupBone( "ValveBiped.Bip01_Pelvis" ) != -1 )
	{
		m_boneSystem = EVRBoneSystem::ValveBiped;
	}
	else
	{
		m_boneSystem = EVRBoneSystem::Unity;
		SetupUnityBones();
	}
}


void C_VRBasePlayer::SetupUnityBones()
{
}


void C_VRBasePlayer::SetBone( EVRBone bone, const char* name )
{
	m_bones.Insert( bone, name );
}


const char* C_VRBasePlayer::GetBoneName( EVRBone bone )
{
	if ( m_boneSystem == EVRBoneSystem::ValveBiped )
	{
		return GetValveBipedBoneName( bone );
	}
	else
	{
		// DEMEZ TODO: look through the registered bones for it
		// for now, just assume ValveBiped still
		return GetValveBipedBoneName( bone );
	}
}


const char* C_VRBasePlayer::GetValveBipedBoneName( EVRBone bone )
{
	// TODO: setup finger bones, and uh, probably use a switch instead lmao

	if ( bone == EVRBone::Head )
		return "ValveBiped.Bip01_Head1";

	else if ( bone == EVRBone::Neck )
		return "ValveBiped.Bip01_Neck1";

	else if ( bone == EVRBone::Spine )
		return "ValveBiped.Bip01_Spine";

	else if ( bone == EVRBone::Pelvis )
		return "ValveBiped.Bip01_Pelvis";


	else if ( bone == EVRBone::LThigh )
		return "ValveBiped.Bip01_L_Thigh";

	else if ( bone == EVRBone::LKnee )
		return "ValveBiped.Bip01_L_Calf";

	else if ( bone == EVRBone::LFoot )
		return "ValveBiped.Bip01_L_Foot";


	else if ( bone == EVRBone::RThigh )
		return "ValveBiped.Bip01_R_Thigh";

	else if ( bone == EVRBone::RKnee )
		return "ValveBiped.Bip01_R_Calf";

	else if ( bone == EVRBone::RFoot )
		return "ValveBiped.Bip01_R_Foot";


	else if ( bone == EVRBone::RShoulder )
		return "ValveBiped.Bip01_R_Clavicle";

	else if ( bone == EVRBone::RUpperArm )
		return "ValveBiped.Bip01_R_UpperArm";

	else if ( bone == EVRBone::RForeArm )
		return "ValveBiped.Bip01_R_Forearm";

	else if ( bone == EVRBone::RHand )
		return "ValveBiped.Bip01_R_Hand";


	else if ( bone == EVRBone::LShoulder )
		return "ValveBiped.Bip01_L_Clavicle";

	else if ( bone == EVRBone::LUpperArm )
		return "ValveBiped.Bip01_L_UpperArm";

	else if ( bone == EVRBone::LForeArm )
		return "ValveBiped.Bip01_L_Forearm";

	else if ( bone == EVRBone::LHand )
		return "ValveBiped.Bip01_L_Hand";


	else
		return "";
}


const char* C_VRBasePlayer::GetUnityBoneName( EVRBone bone )
{
	return "";
}


// goes through each child of the bone and apply's transformations to that
// TODO: clean this shit up once it works fine, ugh
void C_VRBasePlayer::RecurseApplyBoneTransforms( CVRBoneInfo* boneInfo )
{
	if ( boneInfo == NULL )
	{
		Warning("[VR] boneInfo is NULL???\n");
		return;
	}

	matrix3x4_t &boneWrite = boneInfo->GetBoneForWrite();
	matrix3x4_t &parentBoneWrite = boneInfo->GetParentBoneForWrite();

	if ( boneInfo->hasNewAbsPos || boneInfo->hasNewAbsAngles )
	{
		AngleMatrix( boneInfo->newAbsAngles, boneWrite );
		MatrixSetColumn( boneInfo->newAbsPos, 3, boneWrite );
	}
	else if ( boneInfo->hasNewCoord )
	{
		matrix3x4a_t worldToBone, local;
		MatrixInvert( boneInfo->GetParent()->GetCoord(), worldToBone );

#if ENGINE_NEW
		ConcatTransforms_Aligned( worldToBone, boneInfo->newCoord, local );
#else
		ConcatTransforms( worldToBone, boneInfo->newCoord, local );
#endif

		Vector bonePos;
		QAngle localAngles;
		MatrixAngles( local, localAngles );
		MatrixGetColumn( local, 3, bonePos );

		if ( boneInfo->hasNewAngles )
		{
			// not correct yet, and the tracker hand angles are rotating with the view offset?
			// not good for this
			// AngleMatrix( boneInfo->m_relAng + boneInfo->newAngles, bonePos, local );
		}
		else
		{
			AngleMatrix( localAngles + boneInfo->newAngles, bonePos, local );
		}

		ConcatTransforms( parentBoneWrite, local, boneWrite );

		if ( boneInfo->hasNewAngles )
		{
			MatrixGetColumn( boneWrite, 3, bonePos );
			AngleMatrix( boneInfo->newAngles, bonePos, boneWrite );
		}

		if ( boneInfo->drawChildAxis )
		{
			Vector origin;
			QAngle angles;
			MatrixAngles( boneWrite, angles );
			MatrixGetColumn( boneWrite, 3, origin );

			// NDebugOverlay::Axis( origin, boneInfo->newAngles, 2.5f, true, 0.0f );
			// NDebugOverlay::Text( origin, boneInfo->name, true, 0.0f );
		}
	}
	else
	{
		matrix3x4_t matBoneToParent, matNewBoneCoord;

		QAngle newAngles( boneInfo->m_relAng + boneInfo->newAngles );

		// hmm
		// if ( boneInfo->setAsTargetAngle )
		if ( boneInfo->hasNewAngles )
		 	newAngles = QAngle( boneInfo->newAngles );

		NormalizeAngles( newAngles );

		AngleMatrix( newAngles, matBoneToParent );
		MatrixSetColumn( boneInfo->m_relPos, 3, matBoneToParent );

		ConcatTransforms( parentBoneWrite, matBoneToParent, boneWrite );

		if ( boneInfo->hasNewAngles )
		{
			Vector pos;
			MatrixGetColumn( boneWrite, 3, pos );
			AngleMatrix( boneInfo->newAngles, pos, boneWrite );
		}

		if ( boneInfo->drawChildAxis )
		{
			matrix3x4_t boneToWorld;
			ConcatTransforms( m_cameraTransform, boneWrite, boneToWorld );

			Vector absPos;
			QAngle absAng;
			MatrixAngles( boneToWorld, absAng, absPos );

			// NDebugOverlay::Axis( absPos, absAng, 3, true, 0.0f );
			// NDebugOverlay::BoxAngles( absPos, Vector(-1,-1,-1), Vector(1,1,1), absAng, 100, 100, 255, 2, 0.0 );
		}

		if ( boneInfo->drawChildAxis )
		{
			Vector origin;
			QAngle angles;
			MatrixAngles( boneWrite, angles );
			MatrixGetColumn( boneWrite, 3, origin );

			//NDebugOverlay::Axis( origin, angles, 2.5f, true, 0.0f );
			//NDebugOverlay::Text( origin, boneInfo->name, true, 0.0f );
		}
	}

	/*if ( IsLocalPlayer() && GetHeadset() && V_strcmp(boneInfo->name, GetHeadset()->GetBoneName()) == 0 && vr_cl_hidehead.GetFloat() > 0.0f )
	{
		Vector headPos;
		MatrixGetColumn( boneWrite, 3, headPos );

		// have the head be visible if far away enough
		// though this might vary per model, idk
		float dist = GetHeadset()->GetAbsOrigin().DistTo( headPos );
		if ( dist < vr_cl_hidehead.GetFloat() )
		{
			MatrixScaleByZero( boneWrite );
		}
	}*/

	for ( int i = 0; i < boneInfo->childBones.Count(); i++ )
	{
		// if ( boneInfo->drawChildAxis )
		// 	boneInfo->childBones[i]->drawChildAxis = false;

		RecurseApplyBoneTransforms( boneInfo->childBones[i] );
	}
}


// ------------------------------------------------------------------------------------------------
// Playermodel controlling
// ------------------------------------------------------------------------------------------------
void C_VRBasePlayer::BuildTransformations( CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed )
{
	matrix3x4_t newTransform = cameraTransform;

	Vector cameraOrigin;
	QAngle cameraAngles;
	MatrixAngles( newTransform, cameraAngles );
	MatrixGetColumn( newTransform, 3, cameraOrigin );

	cameraAngles.x = 0.0;
	// cameraAngles.y = 0.0;
	cameraAngles.z = 0.0;

	// cameraOrigin.z -= 40;
	cameraOrigin.z += VRHeightOffset();
	
	AngleMatrix( cameraAngles, newTransform );

	if ( !m_bInVR || (m_bInVR && !vr_ik.GetBool()) )
	{
		MatrixSetColumn( cameraOrigin, 3, newTransform );
		BaseClass::BuildTransformations( hdr, pos, q, newTransform, boneMask, boneComputed );
		return;
	}

	UpdateBoneInformation( hdr );

	m_BoneAccessor.SetWritableBones( BONE_USED_BY_ANYTHING );

	CVRTracker* hmd = GetHeadset();

	if ( hmd )
	{
		// TODO: calculate distance from head bone (or forward bone?) to the player origin
		// just using a funny magic number for now
		// cameraOrigin.z += -vr_cl_headheight.GetFloat() + hmd->m_pos.z;

		Vector forward;
		AngleVectors( hmd->m_ang, &forward );

		// cameraOrigin -= forward * vr_cl_headdist.GetFloat();
	}

	MatrixSetColumn( cameraOrigin, 3, newTransform );
	BaseClass::BuildTransformations( hdr, pos, q, newTransform, boneMask, boneComputed );

	m_cameraTransform = newTransform;

	engine->Con_NPrintf( 40, "m_cameraTransform origin %s", VecToString( cameraOrigin ) );
	engine->Con_NPrintf( 41, "m_cameraTransform ang %s", VecToString( cameraAngles ) );

	for (int i = 0; i < m_boneInfoList.Count(); i++)
	{
		CVRBoneInfo* boneInfo = m_boneInfoList[i];
		boneInfo->CalcRelativeCoord();
	}

	// do the hip first if available?
	if ( GetTracker( EVRTracker::HIP ) )
	{
		// CVRTracker* pTracker = GetTracker( EVRTracker::HIP );

		// do ik stuff here with g_VRIK

		// RecurseApplyBoneTransforms( GetRootBoneInfo( pTracker ) );
	}

	// Do the hip and legs here next, but check if we have any trackers for them just in case

	// finally, do the rest of the trackers
	for (int i = 0; i < m_VRTrackers.Count(); i++)
	{
		CVRTracker* pTracker = m_VRTrackers[i];

		if ( !pTracker->m_bInit )
			continue;

		if ( pTracker->m_type == EVRTracker::HIP )
			continue;

		if ( pTracker->IsHeadset() )
		{
			CVRBoneInfo* mainBoneInfo = GetBoneInfo( LookupBone( pTracker->GetBoneName() ) );
			if ( mainBoneInfo )
			{
				QAngle adjustedAngles = pTracker->GetAbsAngles();
				RotateAroundAxis( adjustedAngles, Vector(1, 0, 0), 90 );
				RotateAroundAxis( adjustedAngles, Vector(0, 0, 1), 75 );

				mainBoneInfo->SetCustomAngles( adjustedAngles );
				mainBoneInfo->setAsTargetAngle = true;
				mainBoneInfo->drawChildAxis = true;

				// DEMEZ TODO: do some ik here

				RecurseApplyBoneTransforms( GetRootBoneInfo( pTracker ) );
			}
		}
		else
		{
			BuildTrackerTransformations( hdr, pos, q, m_cameraTransform, boneMask, boneComputed, pTracker );

			if ( pTracker->IsHand() )
			{
				// BuildFingerTransformations( hdr, pos, q, cameraTransform, boneMask, boneComputed, (CVRController*)pTracker );
			}
		}
	}

	for (int i = 0; i < m_VRTrackers.Count(); i++)
	{
		CVRTracker* pTracker = m_VRTrackers[i];

		if ( !pTracker->m_bInit )
			continue;

		RecurseApplyBoneTransforms( GetRootBoneInfo( pTracker ) );
	}

	if ( !hmd )
		return;

	// offset the entire skeleton by vr_cl_headdist
	float offsetDist = vr_cl_headdist.GetFloat();
	CVRBoneInfo* mainBoneInfo = GetBoneInfo( LookupBone( hmd->GetBoneName() ) );

	matrix3x4_t& headBone = mainBoneInfo->GetBoneForWrite();
	Vector headPos;
	MatrixGetColumn( headBone, 3, headPos );

	QAngle headAng;
	MatrixAngles( headBone, headAng );

	// NDebugOverlay::Axis( headPos, headAng, 5.0f, true, 0.0f );

	// AngleMatrix( hmd->GetAbsAngles(), headPos, headBone );

	// QAngle headAng;
	// MatrixAngles( headBone, headAng );

	Vector forward;
	AngleVectors( hmd->GetAbsAngles(), &forward );

	for (int i = 0; i < hdr->numbones(); i++)
	{
		// Only update bones reference by the bone mask.
		/*if ( !( hdr->boneFlags( i ) & boneMask ) )
		{
			continue;
		}*/

		matrix3x4_t& bone = GetBoneForWrite( i );
		Vector vBonePos;
		MatrixGetColumn( bone, 3, vBonePos );

		// vBonePos += forward * offsetDist;
		vBonePos.z += -vr_cl_headheight.GetFloat() + hmd->m_pos.z;
		// MatrixSetColumn( vBonePos, 3, bone );
	}

	if ( IsLocalPlayer() && GetHeadset() && vr_cl_hidehead.GetFloat() > 0.0f )
	{
		matrix3x4_t &boneWrite = mainBoneInfo->GetBoneForWrite();

		Vector headPos;
		MatrixGetColumn( boneWrite, 3, headPos );

		// have the head be visible if far away enough
		// though this might vary per model, idk
		float dist = GetHeadset()->GetAbsOrigin().DistTo( headPos );
		if ( dist < vr_cl_hidehead.GetFloat() )
		{
			MatrixScaleByZero( boneWrite );
		}
	}

	for (int i = 0; i < m_boneInfoList.Count(); i++)
	{
		m_boneInfoList[i]->ResetCustomStuff();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Update the calculated bone positions and saved bones
//          if the model is changed
//-----------------------------------------------------------------------------
void C_VRBasePlayer::UpdateBoneInformation( CStudioHdr *hdr )
{
	if ( hdr == m_prevModel || m_prevTrackerCount == m_VRTrackers.Count() )
		return;

	m_boneMainInfoList.Purge();
	m_boneInfoList.PurgeAndDeleteElements();

	m_prevModel = hdr;

	for (int i = 0; i < hdr->numbones(); i++)
	{
		CVRBoneInfo* boneInfo = new CVRBoneInfo( this, hdr, i );
		m_boneInfoList.AddToTail( boneInfo );

		int iBoneParent = hdr->boneParent(i);
		if ( GetBoneInfo(iBoneParent) )
		{
			GetBoneInfo(iBoneParent)->childBones.AddToTail( boneInfo );
		}
	}
}


CVRBoneInfo* C_VRBasePlayer::GetRootBoneInfo( CVRTracker* pTracker )
{
	return GetBoneInfo( LookupBone( pTracker->GetRootBoneName() ) );
}

CVRBoneInfo* C_VRBasePlayer::GetBoneInfo( CVRTracker* pTracker )
{
	return GetBoneInfo( LookupBone( pTracker->GetBoneName() ) );
}

CVRBoneInfo* C_VRBasePlayer::GetBoneInfo( EVRBone bone )
{
	return GetBoneInfo( LookupBone( GetBoneName( bone ) ) );
}


CVRBoneInfo* C_VRBasePlayer::GetBoneInfo( int index )
{
	for (int i = 0; i < m_boneInfoList.Count(); i++)
	{
		CVRBoneInfo* boneInfo = m_boneInfoList[i];
		if ( boneInfo->index == index )
			return boneInfo;
	}

	return NULL;
}


CVRBoneInfo* C_VRBasePlayer::GetRootBoneInfo( int index )
{
	for (int i = 0; i < m_boneMainInfoList.Count(); i++)
	{
		CVRBoneInfo* boneInfo = m_boneMainInfoList[i];
		if ( boneInfo->index == index )
			return boneInfo;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Have the position of the bone the tracker wants to be moved to where the tracker is
//-----------------------------------------------------------------------------
void C_VRBasePlayer::BuildTrackerTransformations( CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed, CVRTracker* pTracker )
{
	int iMainBone = LookupBone( pTracker->GetBoneName() );
	if ( iMainBone == -1 )
	{
		return;
	}

	int iRootBone = LookupBone( pTracker->GetRootBoneName() );
	if ( iRootBone == -1 )
	{
		return;
	}

	matrix3x4_t &mainBone = GetBoneForWrite( iMainBone );
	matrix3x4_t &parentEndBone = GetBoneForWrite( iRootBone );

	// AngleMatrix( pTracker->GetAbsAngles(), pTracker->GetAbsOrigin(), mainBone );
	
	// for now, just have the bone be where the tracker position is
	/*Vector vBonePos;
	MatrixGetTranslation( mainBone, vBonePos );
	vBonePos = pTracker->GetAbsOrigin();
	MatrixSetTranslation( vBonePos, mainBone );*/

	CVRBoneInfo* mainBoneInfo = GetBoneInfo( iMainBone );
	CVRBoneInfo* rootBoneInfo = GetBoneInfo( iRootBone );

	// mainBoneInfo->SetCustomAngles( pTracker->GetAbsAngles() );

	if ( !rootBoneInfo )
	{
		return;
	}

	if ( pTracker->IsHand() )
	{
		BuildArmTransform( hdr, pTracker, mainBoneInfo, rootBoneInfo );
		return;

		CVRBoneInfo* upperArmInfo;
		CVRBoneInfo* foreArmInfo;

		if ( pTracker->IsLeftHand() )
		{
			upperArmInfo = GetBoneInfo( LookupBone( "ValveBiped.Bip01_L_UpperArm" ) );
			foreArmInfo = GetBoneInfo( LookupBone( "ValveBiped.Bip01_L_Forearm" ) );
		}
		else
		{
			upperArmInfo = GetBoneInfo( LookupBone( "ValveBiped.Bip01_R_UpperArm" ) );
			foreArmInfo = GetBoneInfo( LookupBone( "ValveBiped.Bip01_R_Forearm" ) );
		}

		matrix3x4a_t pBoneToWorld[3] = {
			upperArmInfo->GetBoneForWrite(),
			foreArmInfo->GetBoneForWrite(),
			mainBoneInfo->GetBoneForWrite()
		};

		Vector handBonePos;
		MatrixGetColumn( pBoneToWorld[2], 3, handBonePos );
		// AngleMatrix( pTracker->GetAbsAngles(), handBonePos, pBoneToWorld[2] ); 

		Vector origin = pTracker->GetAbsOrigin();
		bool hmm = Studio_SolveIK( 0, 1, 2, origin, pBoneToWorld );
		// bool hmm = VR_SolveIK( 0, 1, 2, pTracker->GetAbsOrigin(), pBoneToWorld );
		// bool hmm = VR_SolveIK( 0, 1, 2, pTracker->GetAbsOrigin(), ((CVRController*)pTracker)->GetPointDir(), pBoneToWorld );

		Vector plyRight;
		AngleVectors( GetAbsAngles(), NULL, &plyRight, NULL );

		VMatrix mat0(pBoneToWorld[0]);
		VMatrix mat1(pBoneToWorld[1]);
		VMatrix mat2(pBoneToWorld[2]);

		/*MatrixBuildRotationAboutAxis( mat0, plyRight, 90 );
		MatrixBuildRotationAboutAxis( mat1, plyRight, 90 );
		MatrixBuildRotationAboutAxis( mat2, plyRight, 90 );

		Vector tmp;
		MatrixGetColumn( pBoneToWorld[0], 3, tmp );
		MatrixSetColumn( tmp, 3, mat0.As3x4() );
		MatrixGetColumn( pBoneToWorld[1], 3, tmp );
		MatrixSetColumn( tmp, 3, mat1.As3x4() );
		MatrixGetColumn( pBoneToWorld[2], 3, tmp );
		MatrixSetColumn( tmp, 3, mat2.As3x4() );*/

		upperArmInfo->SetTargetCoord( mat0.As3x4() );
		foreArmInfo->SetTargetCoord( mat1.As3x4() );
		mainBoneInfo->SetTargetCoord( mat2.As3x4() );


		// MatrixCopy( pBoneToWorld[0], upperArmInfo->GetBoneForWrite() );
		// MatrixCopy( pBoneToWorld[1], foreArmInfo->GetBoneForWrite() );
		// MatrixCopy( pBoneToWorld[2], mainBoneInfo->GetBoneForWrite() );

		// what now???
	}
}


void C_VRBasePlayer::BuildArmTransform( CStudioHdr *hdr, CVRTracker* pTracker, CVRBoneInfo* handBoneInfo, CVRBoneInfo* clavicleBoneInfo  )
{
	// lazy, should do this on new model and if ik is enabled
	if ( !g_VRIK.UpdateArm( this, pTracker, handBoneInfo, clavicleBoneInfo ) )
	{
		g_VRIK.CreateArmNodes( this, pTracker, handBoneInfo, clavicleBoneInfo );
		g_VRIK.UpdateArm( this, pTracker, handBoneInfo, clavicleBoneInfo );
		// RecurseApplyBoneTransforms( clavicleBoneInfo );
	}
}


// copied from gmod vr lol
void C_VRBasePlayer::BuildArmTransformOld( CStudioHdr *hdr, CVRTracker* pTracker, CVRBoneInfo* clavicleBoneInfo )
{
	QAngle plyAng(0, GetAbsAngles().y, 0);

	Vector plyRight;
	AngleVectors( plyAng, NULL, &plyRight, NULL );
	// plyRight.Init(0, 1, 0);

	engine->Con_NPrintf( 10, "Player Angles: %s\n", VecToString(plyAng) );
	engine->Con_NPrintf( 11, "Player Right: %s\n", VecToString(plyRight) );

	// NDebugOverlay::Line( GetAbsOrigin() + Vector(0, 0, 48), GetAbsOrigin() + Vector(0, 0, 48) + (plyRight * 8), 5, 205, 250, true, 0.0f );
	// NDebugOverlay::Axis( GetAbsOrigin() + plyRight + Vector(0, 0, 48), plyAng, 4.0f, true, 0.0f );

	// if ( pTracker->IsLeftHand() )
	//  	plyRight -= plyRight;

	matrix3x4_t &clavicleBone = GetBoneForWrite( clavicleBoneInfo->index );

	Vector vClaviclePos;
	MatrixGetColumn( clavicleBone, 3, vClaviclePos );

	// -------------------------------------
	// calc target angle for clavicle

	Vector shoulderPos;  // desired shoulder position

	if ( pTracker->IsLeftHand() )
	{
		Vector shoulderPosNeutral = vClaviclePos + plyRight *- clavicleBoneInfo->dist;
		shoulderPos = shoulderPosNeutral + (pTracker->GetAbsOrigin() - shoulderPosNeutral) * 0.15;
	}
	else
	{
		Vector shoulderPosNeutral = vClaviclePos + plyRight * clavicleBoneInfo->dist;
		shoulderPos = shoulderPosNeutral + (pTracker->GetAbsOrigin() - shoulderPosNeutral) * 0.15;
	}

	QAngle clavicleTargetAng;

	if ( !IsInAVehicle() )
	{
		VectorAngles( (shoulderPos - vClaviclePos), clavicleTargetAng );
	}
	else
	{
	}

	// engine->Con_NPrintf( 12, "clavicleTargetAng pre rotate: %s\n", VecToString(clavicleTargetAng) );

	Vector clavicleTargetAngForward;
	AngleVectors( clavicleTargetAng, &clavicleTargetAngForward );
//	NDebugOverlay::Line( shoulderPos, shoulderPos + (Vector(1, 0, 0) * 8), 5, 205, 250, true, 0.0f );

//	NDebugOverlay::Axis( shoulderPos, clavicleTargetAng, 5.0f, true, 0.0f );

	RotateAroundAxis( clavicleTargetAng, Vector(1, 0, 0), 90 );
	// RotateAroundAxisForward( clavicleTargetAng, 90 );
	// RotateAroundAxisUp( clavicleTargetAng, 90 );
	// AngleMatrix( clavicleTargetAng, vClaviclePos, clavicleBone );

	clavicleBoneInfo->SetCustomAngles( clavicleTargetAng );
	// VectorAngles( plyRight, clavicleTargetAng );
	// clavicleBoneInfo->SetCustomAngles( clavicleTargetAng );
	engine->Con_NPrintf( 12, "clavicleTargetAng : %s\n", VecToString(clavicleTargetAng) );

	// -------------------------------------

	Vector forward;
	AngleVectors( clavicleTargetAng, &forward );
	Vector vUpperarmPos = vClaviclePos + forward * clavicleBoneInfo->dist;

	Vector vTargetVec = pTracker->GetAbsOrigin() - vUpperarmPos;

	QAngle targetVecAng, targetVecAngLocal;
	if ( !IsInAVehicle() )
	{
		VectorAngles( vTargetVec, targetVecAng );
	}
	else
	{
	}

	QAngle upperarmTargetAng(targetVecAng);

	// if ( pTracker->IsRightHand() )
	// 	RotateAroundAxis( upperarmTargetAng, vTargetVec, 180 );

	QAngle tmp;
	if ( !IsInAVehicle() )
	{
		// 2nd argument is something calculated value in gmod vr
		// tmp.Init( targetVecAng.x, pTracker->GetAbsAngles().y, pTracker->IsLeftHand() ? -90 : 90 );
		tmp.Init( targetVecAng.x, plyAng.y, pTracker->IsLeftHand() ? -90 : 90 );
	}
	else
	{
	}

	// --------------------------

	Vector testPos;
	QAngle testAngle;
	WorldToLocal( vec3_origin, tmp, vec3_origin, targetVecAng, testPos, testAngle );

	float degrees = testAngle.z;
	// RotateAroundAxisForward( upperarmTargetAng, degrees );
	RotateAroundAxis( upperarmTargetAng, Vector(1, 0, 0), degrees );

	// uh
	CVRBoneInfo* infoUpperArm = NULL;
	CVRBoneInfo* infoLowerArm = NULL;

	if ( pTracker->IsLeftHand() )
	{
		infoUpperArm = GetBoneInfo( LookupBone("ValveBiped.Bip01_L_UpperArm") );
		infoLowerArm = GetBoneInfo( LookupBone("ValveBiped.Bip01_L_Forearm") );
	}
	else
	{
		infoUpperArm = GetBoneInfo( LookupBone("ValveBiped.Bip01_R_UpperArm") );
		infoLowerArm = GetBoneInfo( LookupBone("ValveBiped.Bip01_R_Forearm") );
	}

	// this calculation is wrong, should not be below -1 or above 1
	float a1 = (Sqr(infoUpperArm->dist) + Sqr(vTargetVec.Length()) - Sqr(infoLowerArm->dist));
	float a2 = (2 * infoUpperArm->dist * vTargetVec.Length());

	// also broken
	// float tmpCosValue = (Sqr(infoUpperArm->dist) + Sqr(vTargetVec.Length()) - Sqr(infoLowerArm->dist)) / (2 * infoUpperArm->dist * vTargetVec.Length());
	float tmpCosValue = a2 / a1;
	tmpCosValue = clamp(tmpCosValue, -1.0, 1.0);

	/*
	* typedef struct con_nprint_s
{
	int		index;			// Row #
	float	time_to_live;	// # of seconds before it disappears. -1 means to display for 1 frame then go away.
	float	color[ 3 ];		// RGB colors ( 0.0 -> 1.0 scale )
	bool	fixed_width_font;
} con_nprint_t;
	*/

	// engine->Con_NPrintf( 13, "upperarm tmpCosValue: %f\n", tmpCosValue );
	
	con_nprint_s printInfo;
	printInfo.time_to_live = -1;
	printInfo.fixed_width_font = true;
	printInfo.color[0] = printInfo.color[1] = printInfo.color[2] = 1;

	printInfo.index = pTracker->IsLeftHand() ? 13 : 14;
	if ( (tmpCosValue > 1.0 || tmpCosValue < -1.0) )
	{
		printInfo.color[0] = 220;
		printInfo.color[1] = printInfo.color[2] = 35;
	}
	else
	{
		printInfo.color[0] = 255;
		printInfo.color[1] = 255;
		printInfo.color[2] = 255;
	}

	engine->Con_NXPrintf( &printInfo, "upperarm tmpCosValue: %f\n", tmpCosValue );
	printInfo.index++;

	float finalRotationValue = RAD2DEG( acos(tmpCosValue) );
	// RotateAroundAxisUp( upperarmTargetAng, tmpCosValue );
	// RotateAroundAxis( upperarmTargetAng, Vector(0, 0, 1), finalRotationValue );

	// vehicle check
	float zTest = ((pTracker->GetAbsOrigin().z - (vUpperarmPos.z)) + 20) *1.5;

	if ( zTest < 0 )
		zTest = 0;

	if ( pTracker->IsLeftHand() )
		zTest = (30 + zTest);
	else
		zTest = -(30 + zTest);

	// RotateAroundAxis( upperarmTargetAng, pTracker->GetAbsOrigin().Normalized(), zTest );

	infoUpperArm->SetCustomAngles( upperarmTargetAng );

	// NDebugOverlay::Axis( shoulderPos, upperarmTargetAng, 5.0f, true, 0.0f );

	QAngle forearmTargetAng(upperarmTargetAng);

	// broken
	float tmpCosValueLower = (Sqr(infoLowerArm->dist) + vTargetVec.LengthSqr() - Sqr(infoUpperArm->dist)) / (2 * infoLowerArm->dist * vTargetVec.Length());
	tmpCosValueLower = clamp(tmpCosValueLower, -1.0, 1.0);
	float finalLowerRotationValue = 180 - finalRotationValue - RAD2DEG( acos(tmpCosValueLower) );

	// RotateAroundAxisUp( forearmTargetAng, 180 + finalLowerRotationValue );

	// infoLowerArm->SetCustomAngles( upperarmTargetAng );
	infoLowerArm->SetCustomAngles( forearmTargetAng );

	// NDebugOverlay::Axis( shoulderPos, upperarmTargetAng, 5.0f, true, 0.0f );
	Vector dbgFwd;
	Vector dbgUpperArmPos;
	Vector dbgForeArmPos;

	AngleVectors( clavicleTargetAng, &dbgFwd );
	// NDebugOverlay::Line( shoulderPos, shoulderPos + dbgFwd * infoUpperArm->dist, 255, 0, 255, true, 0.0f );
	

	dbgUpperArmPos = shoulderPos + dbgFwd * infoUpperArm->dist * 2;
	AngleVectors( upperarmTargetAng, &dbgFwd );
	// NDebugOverlay::Line( dbgUpperArmPos, dbgUpperArmPos + dbgFwd * infoUpperArm->dist, 255, 255, 0, true, 0.0f );


	dbgForeArmPos = dbgUpperArmPos + dbgFwd * infoLowerArm->dist * 2;
	AngleVectors( forearmTargetAng, &dbgFwd );
	// NDebugOverlay::Line( dbgForeArmPos, dbgForeArmPos + dbgFwd * infoLowerArm->dist, 255, 0, 255, true, 0.0f );
	// NDebugOverlay::Axis( dbgForeArmPos, forearmTargetAng, 5.0f, true, 0.0f );


}


//-----------------------------------------------------------------------------
// Purpose: Control the finger positions of the hand
//-----------------------------------------------------------------------------
void C_VRBasePlayer::BuildFingerTransformations( CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed, CVRController* pTracker )
{
#if 1
	/*int iHand = LookupBone( pTracker->GetBoneName() );
	if ( iHand == -1 )
	{
		return;
	}

	matrix3x4_t &mHandTransform = GetBoneForWrite( iHand );

	const char* fingerBoneNames[FINGER_BONE_COUNT];  // is this right?
	pTracker->GetFingerBoneNames( fingerBoneNames );

	int iFingerBones[FINGER_BONE_COUNT];
	for (int i = 0; i < FINGER_BONE_COUNT; i++)
	{
		if ( fingerBoneNames[i] == NULL )
		{
			iFingerBones[i] = -1;
			continue;
		}

		// models could have less or even no finger bones
		iFingerBones[i] = LookupBone( fingerBoneNames[i] );
	}*/

#endif
}


// ------------------------------------------------------------------------------------------------
// Untouched Functions
// ------------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Hack to zero out player's pitch, use value from poseparameter instead
// Input  : flags - 
// Output : int
//-----------------------------------------------------------------------------
/*int C_VRBasePlayer::DrawModel( int flags )
{
	// Not pitch for player
	QAngle saveAngles = GetLocalAngles();

	QAngle useAngles = saveAngles;
	useAngles[ PITCH ] = 0.0f;

	SetLocalAngles( useAngles );

	int iret = BaseClass::DrawModel( flags );

	SetLocalAngles( saveAngles );

	return iret;
}*/

//-----------------------------------------------------------------------------
// Purpose: Helper to remove from ladder
//-----------------------------------------------------------------------------
void C_VRBasePlayer::ExitLadder()
{
	if ( MOVETYPE_LADDER != GetMoveType() )
		return;
	
	SetMoveType( MOVETYPE_WALK );
	SetMoveCollide( MOVECOLLIDE_DEFAULT );

	// Remove from ladder
	// m_HL2Local.m_hLadder = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Determines if a player can be safely moved towards a point
// Input:   pos - position to test move to, fVertDist - how far to trace downwards to see if the player would fall,
//			radius - how close the player can be to the object, objPos - position of the object to avoid,
//			objDir - direction the object is travelling
//-----------------------------------------------------------------------------
bool C_VRBasePlayer::TestMove( const Vector &pos, float fVertDist, float radius, const Vector &objPos, const Vector &objDir )
{
	trace_t trUp;
	trace_t trOver;
	trace_t trDown;
	float flHit1, flHit2;
	
	UTIL_TraceHull( GetAbsOrigin(), pos, GetPlayerMins(), GetPlayerMaxs(), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &trOver );
	if ( trOver.fraction < 1.0f )
	{
		// check if the endpos intersects with the direction the object is travelling.  if it doesn't, this is a good direction to move.
		if ( objDir.IsZero() ||
			( IntersectInfiniteRayWithSphere( objPos, objDir, trOver.endpos, radius, &flHit1, &flHit2 ) && 
			( ( flHit1 >= 0.0f ) || ( flHit2 >= 0.0f ) ) )
			)
		{
			// our first trace failed, so see if we can go farther if we step up.

			// trace up to see if we have enough room.
			UTIL_TraceHull( GetAbsOrigin(), GetAbsOrigin() + Vector( 0, 0, m_Local.m_flStepSize ), 
				GetPlayerMins(), GetPlayerMaxs(), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &trUp );

			// do a trace from the stepped up height
			UTIL_TraceHull( trUp.endpos, pos + Vector( 0, 0, trUp.endpos.z - trUp.startpos.z ), 
				GetPlayerMins(), GetPlayerMaxs(), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &trOver );

			if ( trOver.fraction < 1.0f )
			{
				// check if the endpos intersects with the direction the object is travelling.  if it doesn't, this is a good direction to move.
				if ( objDir.IsZero() ||
					IntersectInfiniteRayWithSphere( objPos, objDir, trOver.endpos, radius, &flHit1, &flHit2 ) && ( ( flHit1 >= 0.0f ) || ( flHit2 >= 0.0f ) ) )
				{
					return false;
				}
			}
		}
	}

	// trace down to see if this position is on the ground
	UTIL_TraceLine( trOver.endpos, trOver.endpos - Vector( 0, 0, fVertDist ), 
		MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &trDown );

	if ( trDown.fraction == 1.0f ) 
		return false;

	return true;
}







