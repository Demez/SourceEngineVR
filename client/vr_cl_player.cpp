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
#include "solidsetdefaults.h"

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

#if defined( CVRBasePlayer )
	#undef CVRBasePlayer	
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
	newAngles.Init();
}


CVRBoneInfo* CVRBoneInfo::GetParent()
{
	if ( parentIndex == -1 )
		return NULL;

	return ply->GetBoneInfo( parentIndex );
}


void CVRBoneInfo::CalcRelativeCoord()
{
	if ( parentIndex != -1 )
	{
		dist = parentStudioBone->pos.DistTo( studioBone->pos );
		WorldToLocal( GetParentBoneForWrite(), GetBoneForWrite(), m_relPos, m_relAng );
	}
}


bool CVRBoneInfo::HasCustomAngles()
{
	return hasNewAngles;
}


void CVRBoneInfo::SetCustomAngles( QAngle angles )
{
	hasNewAngles = true;
	newAngles = angles;
}


void CVRBoneInfo::SetTargetPos( Vector pos )
{
	hasNewPos = true;
	newPos = pos;
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


matrix3x4_t& CVRBoneInfo::GetBoneForWrite()
{
	return ply->GetBoneForWrite( index );
}

matrix3x4_t& CVRBoneInfo::GetParentBoneForWrite()
{
	return ply->GetBoneForWrite( parentIndex );
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


// this is stupid
Vector C_VRBasePlayer::EyePosition()
{
	if ( m_bInVR )
	{
		CVRTracker* hmd = GetHeadset();
		if ( hmd == NULL )
			return BaseClass::EyePosition();

		Vector basePos = GetAbsOrigin();
		basePos += m_viewOriginOffset;
		// basePos.z += hmd->m_pos.z;

		return basePos;
	}
	else
	{
		return BaseClass::EyePosition();
	}
}


const QAngle &C_VRBasePlayer::EyeAnglesNoOffset()
{
	if ( m_bInVR )
	{
		CVRTracker* headset = GetHeadset();
		if ( headset == NULL )
			return BaseClass::EyeAngles();

		return headset->m_ang;
	}
	else
	{
		return BaseClass::EyeAngles();
	}
}


const QAngle &C_VRBasePlayer::EyeAngles()
{
	if ( m_bInVR )
	{
		return m_vrViewAngles;
	}
	else
	{
		return BaseClass::EyeAngles();
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

	CVRInput* vr_input = (CVRInput*)input;
	vr_input->VRMove( flInputSampleTime, pCmd );

	return ret;
}


void C_VRBasePlayer::ClientThink()
{
	m_prevTrackerCount = m_trackerCount;
	m_trackerCount = 0;

	PredictCoordinates();
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


// ------------------------------------------------------------------------------------------------
// New VR Functions
// ------------------------------------------------------------------------------------------------
void C_VRBasePlayer::SetViewRotateOffset( float offset )
{
	viewOffset = offset;
	CorrectViewRotateOffset();
}


void C_VRBasePlayer::AddViewRotateOffset( float offset )
{
	viewOffset += offset;
	CorrectViewRotateOffset();
}


void C_VRBasePlayer::CorrectViewRotateOffset()
{
	if ( viewOffset > 360.0 )
		viewOffset -= 360.0;
	else if ( viewOffset < -360.0 )
		viewOffset += 360.0;

	if ( g_VR.active )
	{
		VRHostTracker* headset = g_VR.GetHeadset();
		if ( headset == NULL )
			return;

		m_vrViewAngles = headset->ang;
		m_vrViewAngles.y += viewOffset;
	}
	else
	{
		m_vrViewAngles = BaseClass::EyeAngles();
	}
}


// ===================================================================
// Playermodel controlling
// ===================================================================
CStudioHdr* C_VRBasePlayer::OnNewModel()
{
	CStudioHdr* hdr = BaseClass::OnNewModel();

	SetupConstraints();
	SetupPlayerScale();

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
	if ( /*g_VR.active &&*/ IsLocalPlayer() )
	{
		int headBone = LookupBone( "ValveBiped.Bip01_Head1" );
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


// goes through each child of the bone and apply's transformations to that
void C_VRBasePlayer::RecurseApplyBoneTransforms( CVRBoneInfo* boneInfo )
{
	// matrix3x4_t &bone = boneInfo->GetBoneForWrite();
	// matrix3x4_t &parentBone = boneInfo->GetParentBoneForWrite();

	matrix3x4_t &boneWrite = boneInfo->GetBoneForWrite();
	matrix3x4_t &parentBoneWrite = boneInfo->GetParentBoneForWrite();

	if ( boneInfo->hasNewCoord )
	{
		matrix3x4_t newBone = boneInfo->newCoord;
		matrix3x4_t newParentBone = boneInfo->GetParent()->GetCoord();



		matrix3x4a_t worldToBone;
		MatrixInvert( newParentBone, worldToBone );

		matrix3x4a_t local;
		ConcatTransforms_Aligned( worldToBone, newBone, local );

		// uh
		Vector bonePos;
		QAngle localAngles;
		MatrixAngles( local, localAngles );
		// MatrixGetColumn( local, 3, bonePos );
		MatrixGetColumn( local, 3, bonePos );

		if ( boneInfo->hasNewAngles )
		{
			// not correct yet, and the tracker hand angles are rotating with the view offset?
			// not good for this
			AngleMatrix( boneInfo->m_relAng /*+ boneInfo->newAngles*/, bonePos, local );
		}
		else
		{
			AngleMatrix( localAngles + boneInfo->newAngles, bonePos, local );
		}



		/*matrix3x4_t matBoneToParent, matNewBoneCoord;
		QAngle newAngles( boneInfo->m_relAng + localAngles + boneInfo->newAngles );
		NormalizeAngles( newAngles );

		AngleMatrix( newAngles, matBoneToParent );
		MatrixSetColumn( boneInfo->m_relPos, 3, matBoneToParent );*/

		ConcatTransforms( parentBoneWrite, local, boneWrite );



		// matrix3x4_t tmpBoneCoord(boneWrite);
		// LocalToWorld( tmpBoneCoord, bonePos, newAngle, boneWrite );

		// AngleMatrix( newAngle, bonePos, boneWrite );
		// MatrixCopy( local, boneWrite );
	}
	else
	{
		matrix3x4_t matBoneToParent, matNewBoneCoord;
		QAngle newAngles(boneInfo->m_relAng + boneInfo->newAngles);
		NormalizeAngles( newAngles );

		AngleMatrix( newAngles, matBoneToParent );
		MatrixSetColumn( boneInfo->m_relPos, 3, matBoneToParent );

		ConcatTransforms( parentBoneWrite, matBoneToParent, boneWrite );
	}

	// MatrixAngles( local,)
	// MatrixAngles( local, q[iBone], pos[iBone] );



	/*matrix3x4_t matBoneToParent, matNewBoneCoord;
	QAngle newAngles(boneInfo->m_relAng + boneInfo->newAngles);
	NormalizeAngles( newAngles );

	AngleMatrix( newAngles, matBoneToParent );
	MatrixSetColumn( boneInfo->m_relPos, 3, matBoneToParent );

	ConcatTransforms( parentBone, matBoneToParent, bone );*/

	/*{
		Vector bonePos;
		QAngle boneAng;
		MatrixGetColumn( bone, 3, bonePos );
		MatrixAngles( bone, boneAng );

		// NDebugOverlay::Axis( bonePos, boneAng, 5, false, 0.0f );
		// NDebugOverlay::Text( bonePos, boneInfo->name, false, 0.0f );
	}*/

	for (int i = 0; i < boneInfo->childBones.Count(); i++)
	{
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

	cameraAngles.z = 0.0;
	
	AngleMatrix( cameraAngles, newTransform );
	MatrixSetColumn( cameraOrigin, 3, newTransform );

	BaseClass::BuildTransformations( hdr, pos, q, newTransform, boneMask, boneComputed );

	if( !m_bInVR )
		return;

	UpdateBoneInformation( hdr );

	for (int i = 0; i < m_boneInfoList.Count(); i++)
	{
		CVRBoneInfo* boneInfo = m_boneInfoList[i];
		boneInfo->CalcRelativeCoord();
	}

	m_BoneAccessor.SetWritableBones( BONE_USED_BY_ANYTHING );

	for (int i = 0; i < m_VRTrackers.Count(); i++)
	{
		CVRTracker* pTracker = m_VRTrackers[i];

		if ( !pTracker->m_bInit )
			continue;

		if (pTracker->IsHeadset())
		{
			CVRBoneInfo* mainBoneInfo = GetBoneInfo( LookupBone( pTracker->GetBoneName() ) );
			if ( mainBoneInfo )
				mainBoneInfo->SetCustomAngles( pTracker->GetAbsAngles() );

			// BuildFirstPersonMeathookTransformations( hdr, pos, q, cameraTransform, boneMask, boneComputed, "ValveBiped.Bip01_Head1" );
		}
		else
		{
			BuildTrackerTransformations( hdr, pos, q, cameraTransform, boneMask, boneComputed, pTracker );

			if (pTracker->IsHand())
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

	for (int i = 0; i < m_VRTrackers.Count(); i++)
	{
		CVRTracker* pTracker = m_VRTrackers[i];

		if ( pTracker == NULL || !pTracker->m_bInit )
			continue;

		CVRBoneInfo* boneInfo = new CVRBoneInfo( this, hdr, pTracker->GetRootBoneName() );
		RecurseFindBones( hdr, boneInfo );
		m_boneMainInfoList.AddToTail( boneInfo );
		m_boneOrder.AddToTail( boneInfo->index );  // uhh
	}
}


// probably so god damn inefficient
void C_VRBasePlayer::RecurseFindBones( CStudioHdr *hdr, CVRBoneInfo* boneInfo )
{
	m_boneInfoList.AddToTail( boneInfo );

	matrix3x4_t bone = GetBoneForWrite( boneInfo->index );

	// actually be stupid and go through all the bones to see if one has a parent set to this bone

	for (int i = 0; i < hdr->numbones(); i++)
	{
		int iBoneParent = hdr->boneParent(i);

		if ( iBoneParent != boneInfo->index )
			continue;

		CVRBoneInfo* childBoneInfo = new CVRBoneInfo( this, hdr, i );
		RecurseFindBones( hdr, childBoneInfo );
		boneInfo->childBones.AddToTail( childBoneInfo );
		m_boneOrder.AddToTail( i );  // uhh
	}
}


//-----------------------------------------------------------------------------
// Purpose: In meathook mode, fix the bone transforms to hang the user's own
//			avatar under the camera.
//-----------------------------------------------------------------------------
void C_VRBasePlayer::BuildFirstPersonMeathookTransformations( CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed, const char *pchHeadBoneName )
{
#if ENGINE_CSGO || ENGINE_2013 || ENGINE_TF2
	int iHead = LookupBone( pchHeadBoneName );
	if ( iHead == -1 )
	{
		return;
	}

	return; 

	matrix3x4_t &mHeadTransform = GetBoneForWrite( iHead );

	// "up" on the head bone is along the negative Y axis - not sure why.
	//Vector vHeadTransformUp ( -mHeadTransform[0][1], -mHeadTransform[1][1], -mHeadTransform[2][1] );
	//Vector vHeadTransformFwd ( mHeadTransform[0][1], mHeadTransform[1][1], mHeadTransform[2][1] );
	Vector vHeadTransformTranslation ( mHeadTransform[0][3], mHeadTransform[1][3], mHeadTransform[2][3] );


	// Find out where the player's head (driven by the HMD) is in the world.
	// We can't move this with animations or effects without causing nausea, so we need to move
	// the whole body so that the animated head is in the right place to match the player-controlled head.
	Vector vHeadUp;
	Vector vRealPivotPoint;

	VMatrix mWorldFromMideye;
	mWorldFromMideye.SetupMatrixOrgAngles( EyePosition(), EyeAngles() );

	// What we do here is:
	// * Take the required eye pos+orn - the actual pose the player is controlling with the HMD.
	// * Go downwards in that space by cl_meathook_neck_pivot_ingame_* - this is now the neck-pivot in the game world of where the player is actually looking.
	// * Now place the body of the animated character so that the head bone is at that position.
	// The head bone is the neck pivot point of the in-game character.

	Vector vRealMidEyePos = mWorldFromMideye.GetTranslation();
	vRealPivotPoint = vRealMidEyePos - ( mWorldFromMideye.GetUp() * cl_meathook_neck_pivot_ingame_up.GetFloat() ) - ( mWorldFromMideye.GetForward() * cl_meathook_neck_pivot_ingame_fwd.GetFloat() );

	Vector vDeltaToAdd = vRealPivotPoint - vHeadTransformTranslation;

	// Now add this offset to the entire skeleton.
	/*for (int i = 0; i < hdr->numbones(); i++)
	{
		// Only update bones reference by the bone mask.
		if ( !( hdr->boneFlags( i ) & boneMask ) )
		{
			continue;
		}
		matrix3x4_t& bone = GetBoneForWrite( i );
		Vector vBonePos;
		MatrixGetTranslation ( bone, vBonePos );
		vBonePos += vDeltaToAdd;
		MatrixSetTranslation ( vBonePos, bone );
	}*/

	// Then scale the head to zero, but leave its position - forms a "neck stub".
	// This prevents us rendering junk all over the screen, e.g. inside of mouth, etc.
	if ( IsLocalPlayer() )
	{
		MatrixScaleByZero( mHeadTransform );
	}

#endif
}


CVRBoneInfo* C_VRBasePlayer::GetRootBoneInfo( CVRTracker* pTracker )
{
	return GetBoneInfo( LookupBone( pTracker->GetRootBoneName() ) );
}

CVRBoneInfo* C_VRBasePlayer::GetBoneInfo( CVRTracker* pTracker )
{
	return GetBoneInfo( LookupBone( pTracker->GetBoneName() ) );
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
#if ENGINE_QUIVER || ENGINE_CSGO || ENGINE_2013 || ENGINE_TF2
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

	mainBoneInfo->SetCustomAngles( pTracker->GetAbsAngles() );

	if ( !rootBoneInfo )
	{
		return;
	}

	if ( pTracker->IsHand() )
	{
		// BuildArmTransform( hdr, pTracker, rootBoneInfo );

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

		Vector targetHand;
		bool hmm = Studio_SolveIK( 0, 1, 2, pTracker->GetAbsOrigin(), pBoneToWorld );

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

	/*for (int i = 0; i < hdr->numbones(); i++)
	{
		rootBoneInfo->childBones;
	}*/

	// adjust other bones
	/*for (int i = 0; i < hdr->numbones(); i++)
	{
		// Only update bones reference by the bone mask.
		if ( !( hdr->boneFlags( i ) & boneMask ) )
		{
			continue;
		}

		matrix3x4a_t mainBone = GetBone( i );
		

		// somehow rotate the matrix to point toward the ?
	}*/

#endif
}


void RotateAroundAxis( QAngle& angle, Vector axis, float degrees )
{
	/*Vector vectorOfRotation(axis);
	Vector axisOfRotation = Vector(
		1 - vec.x,
		1 - vec.y,
		1 - vec.z
	).Normalized(); // flip axes and normalize
	QAngle rotation(axisOfRotation * 90);

	angle = rotation;*/

	VMatrix mat;
	AngleMatrix( angle, mat.As3x4() );
	MatrixBuildRotationAboutAxis( mat, axis, degrees );

	// MatrixAngles( mat.As3x4(), angle );
	MatrixToAngles( mat, angle );

	// Vector rotate;
	// VectorYawRotate( direction, degrees, rotate );

	// VectorAngles( rotate, angle );
}


void RotateAroundAxisForward( QAngle& angle, float degrees )
{
	Vector forward;
	AngleVectors( angle, &forward );

	RotateAroundAxis( angle, forward, degrees );
}


void RotateAroundAxisUp( QAngle& angle, float degrees )
{
	Vector direction;
	AngleVectors( angle, NULL, NULL, &direction );

	RotateAroundAxis( angle, direction, degrees );
}


// copied from gmod vr lol
void C_VRBasePlayer::BuildArmTransform( CStudioHdr *hdr, CVRTracker* pTracker, CVRBoneInfo* clavicleBoneInfo )
{
	QAngle plyAng(0, GetAbsAngles().y, 0);

	Vector plyRight;
	AngleVectors( plyAng, NULL, &plyRight, NULL );

	

	// if ( pTracker->IsLeftHand() )
	//  	plyRight -= plyRight;

	matrix3x4_t &clavicleBone = GetBoneForWrite( clavicleBoneInfo->index );

	Vector vClaviclePos;
	MatrixGetColumn( clavicleBone, 3, vClaviclePos );

	// -------------------------------------
	// calc target angle for clavicle

	Vector shoulderPosNeutral = vClaviclePos + plyRight *- clavicleBoneInfo->dist;
	Vector shoulderPos = shoulderPosNeutral + (pTracker->GetAbsOrigin() - shoulderPosNeutral) * 0.15;  // desired shoulder position

	QAngle clavicleTargetAng;

	if ( !IsInAVehicle() )
	{
		VectorAngles( (shoulderPos - vClaviclePos), clavicleTargetAng );
	}
	else
	{
	}

	RotateAroundAxisForward( clavicleTargetAng, 90 );
	// RotateAroundAxisUp( clavicleTargetAng, 90 );
	// AngleMatrix( clavicleTargetAng, vClaviclePos, clavicleBone );
	clavicleBoneInfo->SetCustomAngles( clavicleTargetAng );

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

	QAngle tmp;
	if ( !IsInAVehicle() )
	{
		// 2nd argument is something calculated value in gmod vr
		tmp.Init(targetVecAng.x, pTracker->GetAbsAngles().y, -90);
	}
	else
	{
	}

	// --------------------------

	Vector testPos;
	QAngle testAngle;
	WorldToLocal( vec3_origin, tmp, vec3_origin, targetVecAng, testPos, testAngle );

	float degrees = testAngle.z;
	RotateAroundAxisForward( upperarmTargetAng, degrees );

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

	float tmpCosValue = (Sqr(infoUpperArm->dist) + Sqr(vTargetVec.Length()) - Sqr(infoLowerArm->dist)) / (2 * infoUpperArm->dist * vTargetVec.Length());
	float finalRotationValue = RAD2DEG( acos(tmpCosValue) );
	RotateAroundAxisUp( upperarmTargetAng, tmpCosValue );

	// vehicle check
	float zTest = ((pTracker->GetAbsOrigin().z - (vUpperarmPos.z)) + 20) *1.5;

	if ( zTest < 0 )
		zTest = 0;

	if ( pTracker->IsLeftHand() )
		zTest = (30 + zTest);
	else
		zTest = -(30 + zTest);

	RotateAroundAxis( upperarmTargetAng, pTracker->GetAbsOrigin().Normalized(), zTest );

	// infoUpperArm->SetCustomAngles( upperarmTargetAng );

	QAngle forearmTargetAng(upperarmTargetAng);

	float tmpCosValueLower = (Sqr(infoLowerArm->dist) + vTargetVec.LengthSqr() - Sqr(infoUpperArm->dist)) / (2 * infoLowerArm->dist * vTargetVec.Length());
	float finalLowerRotationValue = 180 - finalRotationValue - RAD2DEG( acos(tmpCosValueLower) );

	// infoLowerArm->SetCustomAngles( upperarmTargetAng );


}


//-----------------------------------------------------------------------------
// Purpose: Control the finger positions of the hand
//-----------------------------------------------------------------------------
void C_VRBasePlayer::BuildFingerTransformations( CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed, CVRController* pTracker )
{
#if ENGINE_CSGO || ENGINE_2013 || ENGINE_TF2
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







