#include "cbase.h"
#include "vr_ik.h"
#include "vr_cl_player.h"
#include "vr_util.h"
#include "tier1/fmtstr.h"


VRIKSystem g_VRIK;
// ik_interface_t ik;


extern ConVar vr_cl_headheight;


VRIKSystem::VRIKSystem()
{
	m_enabled = false;
}


VRIKSystem::~VRIKSystem()
{
}


void VRIKSystem::Init()
{
	if (ik.init() != IK_OK)
		Error("[VRIK] Failed to initialize IK");

	if (ik.log.init() != IK_OK)
		Warning("[VRIK] Failed to initialize IK logging");

	ik.log.set_severity(IK_WARNING);

	m_enabled = true;
}


void VRIKSystem::DeInit()
{
	ik.deinit();
	ik.log.deinit();

	m_enabled = false;
}


void VRIKSystem::PrintInfo()
{
	// const char* v  = ik.info.version();        /* Returns the version string of the library */
	// int bn         = ik.info.build_number();   /* Returns how many times the library was compiled */
	// const char* h  = ik.info.host();           /* Returns the name of the host computer that built the library */
	// const char* d  = ik.info.date();           /* Returns the time the library was built */
	// const char* ch = ik.info.commit();         /* Returns the commit hash */
	// const char* cp = ik.info.compiler();       /* Returns the compiler used to build the library */
	// const char* cm = ik.info.cmake();          /* Returns the exact cmake options used */
	// const char* a  = ik.info.all();            /* Returns everything above as a single string */

	Msg( "[VRIK] Info:\n %s", ik.info.all() );

	// Msg( "[VRIK] Info:\n" );
	// Msg( " - Version:\t\t %s\n", ik.info.version() );
	// Msg( " - Build Number:\t\t %d\n", ik.info.build_number() );     /* Returns how many times the library was compiled */
	// Msg( " - Host:\t\t %s\n", ik.info.build_number() );             /* Returns the name of the host computer that built the library */
	// Msg( " - Host:\t\t %s\n", ik.info.commit() );                   /* Returns the commit hash */
	// Msg( " - Host:\t\t %s\n", ik.info.compiler() );                 /* Returns the compiler used to build the library */
}


// TODO: convert coordinate system if needed?
ik_vec3_t VR_VecToIK( const Vector& src )
{
	ik_vec3_t dest;
	dest.x = src.x;
	dest.y = src.y;
	dest.z = src.z;
	return dest;
}


ik_vec3_t VR_AngToIK( const QAngle& src )
{
	ik_vec3_t dest;
	dest.x = src.x;
	dest.y = src.y;
	dest.z = src.z;
	return dest;
}


ik_quat_t VR_QuatToIK( const Quaternion& src )
{
	ik_quat_t dest;
	dest.w = src.w;
	dest.x = src.x;
	dest.y = src.y;
	dest.z = src.z;
	return dest;
}


ik_quat_t VR_AngToQuatIK( const QAngle& ang )
{
	Quaternion src;
	AngleQuaternion( ang, src );
	return VR_QuatToIK( src );
}


QAngle VR_AngToSrc( const ik_vec3_t& src )
{
	QAngle dest;
	dest.x = src.x;
	dest.y = src.y;
	dest.z = src.z;
	return dest;
}


Vector VR_VecToSrc( const ik_vec3_t& src )
{
	Vector dest;
	dest.x = src.x;
	dest.y = src.y;
	dest.z = src.z;
	return dest;
}


Quaternion VR_QuatToSrc( const ik_quat_t& src )
{
	Quaternion dest;
	dest.w = src.w;
	dest.x = src.x;
	dest.y = src.y;
	dest.z = src.z;
	return dest;
}


QAngle VR_QuatToAngSrc( const ik_quat_t& quat )
{
	QAngle src;
	QuaternionAngles( VR_QuatToSrc( quat ), src );
	return src;
}


struct VRNodeUserData
{
	VRNodeUserData( C_VRBasePlayer* pPlayer, CVRTracker* pTracker, CVRBoneInfo* boneInfo )
	{
		this->pPlayer = pPlayer;
		this->pTracker = pTracker;
		this->boneInfo = boneInfo;
	}

	CVRBoneInfo* boneInfo;
	CVRTracker* pTracker;
	C_VRBasePlayer* pPlayer;
};


static void ApplyNodesToBones(struct ik_node_t* ikNode)
{
	VRNodeUserData* node = (VRNodeUserData*)ikNode->user_data;
	CVRBoneInfo* boneInfo = node->boneInfo;
	C_VRBasePlayer* pPlayer = node->pPlayer;

	Vector absPos;
	QAngle absAng;

	Vector bonePos = VR_VecToSrc(ikNode->position);
	QAngle boneAng = VR_QuatToAngSrc(ikNode->rotation);

	QAngle plyAng(0, pPlayer->GetAbsAngles().y, 0);

	// LocalToWorld( pPlayer->m_cameraTransform, plyAng, VR_VecToSrc(ikNode->position), VR_QuatToAngSrc(ikNode->rotation), absPos, absAng );

	/*matrix3x4_t bonematrix;
	matrix3x4_t boneToWorld;
	AngleMatrix( boneAng, bonePos, bonematrix );

	ConcatTransforms( node->pPlayer->m_cameraTransform, bonematrix, boneToWorld );

	MatrixAngles( boneToWorld, absAng, absPos );


	matrix3x4_t matBone, matBoneToWorld;

	AngleMatrix( boneAng, matBone );
	MatrixSetColumn( bonePos, 3, matBone );

	ConcatTransforms( node->pPlayer->m_cameraTransform, matBone, matBoneToWorld );

	MatrixAngles( matBoneToWorld, absAng, absPos );*/



	matrix3x4a_t matBone;

	AngleMatrix( boneAng, matBone );
	MatrixSetColumn( bonePos, 3, matBone );


	matrix3x4a_t matBoneToWorld2;

	if ( boneInfo->parentIndex == -1 ) 
	{
		ConcatTransforms( pPlayer->m_cameraTransform, matBone, matBoneToWorld2 );
	} 
	else
	{
		ConcatTransforms_Aligned( boneInfo->GetParentBoneForWrite(), matBone, matBoneToWorld2 );

		//CVRBoneInfo* parentInfo = boneInfo->GetParent();

		/*while (parentInfo->GetParent() != nullptr)
		{
			ConcatTransforms_Aligned( parentInfo->GetParentBoneForWrite(), matBoneToWorld2, matBoneToWorld2 );

			ConcatTransforms_Aligned( parentInfo->GetParentBoneForWrite(), matBone, matBoneToWorld2 );
			parentInfo = parentInfo->GetParent();
		}*/
	}


	MatrixAngles( matBoneToWorld2, absAng, absPos );



	// LocalToWorld( node->pPlayer->m_cameraTransform, bonePos, boneAng, absPos, absAng );

	// make it relative to the parent bone world pos first?
	// LocalToWorld( node->boneInfo->parentStudioBone->poseToBone, bonePos, boneAng, absPos, absAng );
	//LocalToWorld( node->boneInfo->studioBone->poseToBone, bonePos, boneAng, absPos, absAng );
	
	// now make that relative to the camera transform?
	// LocalToWorld( node->pPlayer->m_cameraTransform, absPos, absAng, absPos, absAng );
	//LocalToWorld( node->pPlayer->GetOriginViewOffset(), node->pPlayer->GetAbsAngles(), absPos, absAng, absPos, absAng );
	// LocalToWorld( node->pPlayer->GetOriginViewOffset(), node->pPlayer->GetAbsAngles(), bonePos, boneAng, absPos, absAng );


	// NDebugOverlay::Axis( absPos, absAng, 3, true, 0.0f );
	NDebugOverlay::BoxAngles( absPos, Vector(-1,-1,-1), Vector(1,1,1), absAng, 255, 100, 100, 5, 0.0 );


	int pos = node->pTracker->IsLeftHand() ? 15 : 24;
	// engine->Con_NPrintf( pos + ikNode->guid, "%s Target Node pos: %s", node->pTracker->IsLeftHand() ? "L" : "R", VecToString(absPos) );
	engine->Con_NPrintf( pos + ikNode->guid + 3, "%s Target Node ang: %s", node->pTracker->IsLeftHand() ? "L" : "R", VecToString(absAng) );

	node->boneInfo->SetAbsPos( absPos );
	node->boneInfo->SetAbsAng( absAng );

	// node->boneInfo->SetTargetPos( VR_VecToSrc(ikNode->position) );
	// node->boneInfo->SetTargetAng( VR_QuatToAngSrc(ikNode->rotation) );
}


void VRIKSystem::CreateArmNodes( C_VRBasePlayer* pPlayer, CVRTracker* pTracker, CVRBoneInfo* handInfo, CVRBoneInfo* clavicleBoneInfo )
{
	if ( pTracker == NULL || handInfo == NULL || clavicleBoneInfo == NULL )
	{
		Warning("[VRIK] Didn't create arm nodes due to either the tracker, hand or shoulder boneInfo being NULL\n");
		return;
	}

	ik_solver_t* solver = ik.solver.create(IK_FABRIK);

	// not needed for IK_TWO_BONE
	// solver->max_iterations = 20;
	// solver->tolerance = 1e-3;

	// solver->flags |= IK_ENABLE_CONSTRAINTS;
	solver->flags |= IK_ENABLE_JOINT_ROTATIONS;

	ik_node_t* upperArmNode = solver->node->create(0);
	ik_node_t* foreArmNode = solver->node->create_child(upperArmNode, 1);
	ik_node_t* handNode = solver->node->create_child(foreArmNode, 2);

	CVRBoneInfo* upperArmInfo;
	CVRBoneInfo* foreArmInfo;

	if ( pTracker->IsLeftHand() )
	{
		upperArmInfo = clavicleBoneInfo->GetChild( EVRBone::LUpperArm );
		foreArmInfo = upperArmInfo->GetChild( EVRBone::LForeArm );
	}
	else
	{
		upperArmInfo = clavicleBoneInfo->GetChild( EVRBone::RUpperArm );
		foreArmInfo = upperArmInfo->GetChild( EVRBone::RForeArm );
	}

	// hmmm
	upperArmNode->user_data = (void*) new VRNodeUserData( pPlayer, pTracker, upperArmInfo );
	foreArmNode->user_data = (void*) new VRNodeUserData( pPlayer, pTracker, foreArmInfo );
	handNode->user_data = (void*) new VRNodeUserData( pPlayer, pTracker, handInfo );

	SetNodeCoords( pPlayer, pTracker, upperArmNode, upperArmInfo );
	SetNodeCoords( pPlayer, pTracker, foreArmNode, foreArmInfo );
	SetNodeCoords( pPlayer, pTracker, handNode, handInfo );

	ik_effector_t* effector = solver->effector->create();
	// effector->chain_length = 3; /* how many parent segments should be affected */
	solver->effector->attach(effector, handNode);

	VRIKInfo* ikInfo = new VRIKInfo;
	ikInfo->m_effector = effector;
	ikInfo->m_solver = solver;
	ikInfo->m_nodes.AddToTail( upperArmNode );
	ikInfo->m_nodes.AddToTail( foreArmNode );
	ikInfo->m_nodes.AddToTail( handNode );

	if ( pTracker->IsLeftHand() )
	{
		pPlayer->m_ikLArm = ikInfo;
	}
	else
	{
		pPlayer->m_ikRArm = ikInfo;
	}

	ik.solver.set_tree( ikInfo->m_solver, upperArmNode );
	ik.solver.rebuild( solver );
	ik.solver.update_distances( solver );
}


// Get real world bone pos and make it local to headset instead
void VRIKSystem::SetNodeCoords( C_VRBasePlayer* pPlayer, CVRTracker* pTracker, ik_node_t* node, CVRBoneInfo* boneInfo )
{
	// if this ever happens i will be surprised
	if ( !pPlayer->GetHeadset() )
		return;

	VRNodeUserData* vrNode = (VRNodeUserData*)node->user_data;

	CVRTracker* hmd = pPlayer->GetHeadset();

	Vector finalPos;
	QAngle finalAng;

	Vector bonePos = boneInfo->studioBone->pos;

	QAngle boneAng;
	QuaternionAngles( boneInfo->studioBone->quat, boneAng );

	Vector worldBonePos, localBonePos;
	QAngle worldBoneAng, localBoneAng;

	// uhhhh
	// worldBonePos = boneInfo->studioBone->pos;
	// worldBoneAng = boneInfo->studioBone->rot.ToQAngle();

	matrix3x4a_t& worldBone = boneInfo->GetBoneForWrite();
	VMatrix worldBoneMat(worldBone);

	MatrixGetColumn( worldBone, 3, worldBonePos ); 
	MatrixAngles( worldBone, worldBoneAng );

	Vector playerPos = pPlayer->GetOriginViewOffset();

	matrix3x4a_t matBone, matBoneToWorld;

	AngleMatrix( boneAng, matBone );
	MatrixSetColumn( bonePos, 3, matBone );

	ConcatTransforms( pPlayer->m_cameraTransform, matBone, matBoneToWorld );

	//MatrixAngles( matBoneToWorld, finalAng, finalPos );




	matrix3x4a_t matBoneLocal;

	if ( boneInfo->parentIndex == -1 ) 
	{
		ConcatTransforms( pPlayer->m_cameraTransform, matBone, matBoneLocal );
		MatrixAngles( matBoneLocal, finalAng, finalPos );
	} 
	else
	{
		//WorldToLocal( worldBone, boneInfo->GetParentBoneForWrite(), finalPos, finalAng );
		WorldToLocal( boneInfo->GetParentBoneForWrite(), worldBone, finalPos, finalAng );

		//ConcatTransforms_Aligned( boneInfo->GetParentBoneForWrite(), matBone, matBoneLocal );
	}






	// WorldToLocal( pPlayer->m_cameraTransform, worldBone, finalPos, finalAng );



	// QAngle hmdAng(0, hmd->GetAbsAngles().y, 0);
	// QAngle hmdAng(0, 0, 0);
	// WorldToLocal( playerPos, pPlayer->GetAbsAngles(), worldBonePos, worldBoneAng, localBonePos, localBoneAng );
	// QAngle plyAng(0, pPlayer->GetAbsAngles().y, 0);
	// WorldToLocal( pPlayer->GetAbsOrigin(), hmdAng, worldBonePos, hmdAng, localBonePos, localBoneAng );

	// const mstudiobone_t* rootBone = boneInfo->GetRootBone();
	// WorldToLocal( rootBone->pos, hmdAng, boneInfo->studioBone->pos, hmdAng, localBonePos, localBoneAng );

	// idk if i need anything else?
	// finalPos = localBonePos;
	// finalAng = localBoneAng;

	//MatrixGetColumn( boneInfo->studioBone->poseToBone, 3, finalPos );
	//MatrixAngles( boneInfo->studioBone->poseToBone, finalAng );

	node->position = VR_VecToIK( finalPos );    // ik.vec3.vec3(0, 1, 0);
	node->rotation = VR_AngToQuatIK( finalAng );  // ik.quat.quat(1, 0, 0, 0);

	// !!!! WORKS FOR POSITION !!!!
	// ah, i know why this works, it's because each studioBone pos is relative to the parent bone

	//finalPos = boneInfo->studioBone->pos;
	// finalPos.z += -vr_cl_headheight.GetFloat(); // + hmd->m_pos.z;
	//node->position = VR_VecToIK( finalPos );
	//node->rotation = VR_QuatToIK( boneInfo->studioBone->quat );
}


void GetTargetCoords( C_VRBasePlayer* pPlayer, CVRTracker* pTracker, Vector& outPos, QAngle& outAng )
{
	QAngle plyAng(0, pPlayer->GetAbsAngles().y, 0);
	WorldToLocal( pTracker->GetAbsOrigin(), pTracker->GetAbsAngles(), pPlayer->GetAbsOrigin(), plyAng, outPos, outAng );
}


// DEMEZ TODO: this does update and does somewhat put the hand in the right spot,
// but the rotation is all off and if i stand up or move from where im sitting at all,
// the hand position becomes offset
bool VRIKSystem::UpdateArm( C_VRBasePlayer* pPlayer, CVRTracker* pTracker, CVRBoneInfo* handInfo, CVRBoneInfo* clavicleBoneInfo )
{
	VRIKInfo* ikInfo = pTracker->IsLeftHand() ? pPlayer->m_ikLArm : pPlayer->m_ikRArm;

	if ( ikInfo == nullptr )
	{
		Warning( "[VRIK] No IK setup for %s Arm!\n", pTracker->IsLeftHand() ? "Left" : "Right" );
		return false;
	}

	CVRBoneInfo* upperArmInfo;
	CVRBoneInfo* foreArmInfo;

	if ( pTracker->IsLeftHand() )
	{
		upperArmInfo = clavicleBoneInfo->GetChild( EVRBone::LUpperArm );
		foreArmInfo = upperArmInfo->GetChild( EVRBone::LForeArm );
	}
	else
	{
		upperArmInfo = clavicleBoneInfo->GetChild( EVRBone::RUpperArm );
		foreArmInfo = upperArmInfo->GetChild( EVRBone::RForeArm );
	}

	SetNodeCoords( pPlayer, pTracker, ikInfo->m_nodes[0], upperArmInfo );
	SetNodeCoords( pPlayer, pTracker, ikInfo->m_nodes[1], foreArmInfo );
	SetNodeCoords( pPlayer, pTracker, ikInfo->m_nodes[2], handInfo );

	// this is necessary since the positions and rotations seem to be garbage data for some reason now, which causes the last bone to just spin violently
	// ikInfo->m_nodes[0]->position = VR_VecToIK( upperArmInfo->studioBone->pos );    // ik.vec3.vec3(0, 1, 0);
	// ikInfo->m_nodes[0]->rotation = VR_QuatToIK( upperArmInfo->studioBone->quat );  // ik.quat.quat(1, 0, 0, 0);
	// ikInfo->m_nodes[0]->rotation = VR_AngToQuatIK( upperArmInfo->studioBone->rot.ToQAngle() );  // ik.quat.quat(1, 0, 0, 0);
	// 
	// ikInfo->m_nodes[1]->position = VR_VecToIK( foreArmInfo->studioBone->pos );    // ik.vec3.vec3(0, 1, 0);
	// ikInfo->m_nodes[1]->rotation = VR_QuatToIK( foreArmInfo->studioBone->quat );  // ik.quat.quat(1, 0, 0, 0);
	// ikInfo->m_nodes[1]->rotation = VR_AngToQuatIK( foreArmInfo->studioBone->rot.ToQAngle() );  // ik.quat.quat(1, 0, 0, 0);
	// 
	// ikInfo->m_nodes[2]->position = VR_VecToIK( handInfo->studioBone->pos );    // ik.vec3.vec3(0, 1, 0);
	// ikInfo->m_nodes[1]->rotation = VR_QuatToIK( handInfo->studioBone->quat );  // ik.quat.quat(1, 0, 0, 0);
	// ikInfo->m_nodes[2]->rotation = VR_AngToQuatIK( handInfo->studioBone->rot.ToQAngle() );  // ik.quat.quat(1, 0, 0, 0);

	ikInfo->m_solver->tolerance = 1e-3;
	// ikInfo->m_solver->tolerance = 0.9;

	ik.solver.rebuild( ikInfo->m_solver );
	ik.solver.update_distances( ikInfo->m_solver );


	Vector finalPos = pTracker->m_posOffset;
	QAngle finalAng = pTracker->m_ang;

	// Vector finalPos = pTracker->GetAbsOrigin();
	// QAngle finalAng = pTracker->GetAbsAngles();

	Vector worldBonePos, localBonePos;
	QAngle worldBoneAng, localBoneAng;

	matrix3x4a_t& worldBone = handInfo->GetBoneForWrite();

	MatrixGetColumn( worldBone, 3, worldBonePos ); 
	MatrixAngles( worldBone, worldBoneAng );

	CVRTracker* hmd = pPlayer->GetHeadset();

	
	CVRBoneInfo* rootInfo = handInfo->GetRootBoneInfo();
	Vector distFromRoot;


	// QAngle plyAng(0, hmd->GetAbsAngles().y, 0);
	QAngle plyAng(0, 0, 0);
	// WorldToLocal( hmd->GetAbsOrigin(), plyAng, pTracker->GetAbsOrigin(), pTracker->GetAbsAngles(), finalPos, finalAng );

	finalAng = pTracker->m_absAng;


	ikInfo->m_effector->target_position = VR_VecToIK( finalPos );
	// ikInfo->m_effector->target_position = ik_vec3_t();
	ikInfo->m_effector->target_rotation = VR_AngToQuatIK( finalAng );
	// ikInfo->m_effector->target_rotation = ik_quat_t();
	// ikInfo->m_effector->chain_length = 3;
	ikInfo->m_effector->weight = 1.0;

	ik.solver.solve( ikInfo->m_solver );
	ik.solver.iterate_affected_nodes( ikInfo->m_solver, ApplyNodesToBones );

	engine->Con_NPrintf( pTracker->IsLeftHand() ? 33 : 34, "%s hand node pos %s", pTracker->IsLeftHand() ? "L" : "R", VecToString( ikInfo->m_nodes[2]->position ) );
	engine->Con_NPrintf( pTracker->IsLeftHand() ? 36 : 37, "%s hand tracker pos %s", pTracker->IsLeftHand() ? "L" : "R", VecToString( pTracker->m_posOffset ) );

	for (int i = 0; i < 2; i++)
	{
		Vector pos = VR_VecToSrc(ikInfo->m_nodes[i]->position);
		QAngle ang = VR_QuatToAngSrc(ikInfo->m_nodes[i]->rotation);

		// is this right?
		// NDebugOverlay::Axis( pPlayer->GetOriginViewOffset() + pos, ang, 3, true, 0.0f );
		NDebugOverlay::BoxAngles( pPlayer->GetOriginViewOffset() + pos, Vector(-1,-1,-1), Vector(1,1,1), ang, 100, 100, 255, 2, 0.0 );
		// NDebugOverlay::Axis( hmd->GetAbsOrigin() + pos, ang, 3, true, 0.0f );
	}

	NDebugOverlay::BoxAngles( pPlayer->GetOriginViewOffset() + finalPos, Vector(-1,-1,-1), Vector(1,1,1), finalAng, 100, 255, 100, 2, 0.0 );

	return true;
}


