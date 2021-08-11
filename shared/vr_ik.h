#pragma once

// #include <ik/ik.h>
extern "C"
{
	#include <ik/ik.h>
}


// #include "vr_cl_player.h"

class C_VRBasePlayer;
class CVRTracker;
class CVRBoneInfo;


struct VRIKInfo
{
	ik_solver_t* m_solver;
	ik_effector_t* m_effector;
	CUtlVector< ik_node_t* > m_nodes;
};


class VRIKSystem
{
public:
	VRIKSystem();
	~VRIKSystem();

	void Init();
	void DeInit();
	void PrintInfo();

	void CreateArmNodes( C_VRBasePlayer* pPlayer, CVRTracker* pTracker, CVRBoneInfo* handInfo, CVRBoneInfo* clavicleBoneInfo );

	bool UpdateArm( C_VRBasePlayer* pPlayer, CVRTracker* pTracker, CVRBoneInfo* handInfo, CVRBoneInfo* clavicleBoneInfo );

	void SetNodeCoords( C_VRBasePlayer* pPlayer, CVRTracker* pTracker, ik_node_t* node, CVRBoneInfo* boneInfo );

	bool m_enabled;
};


extern VRIKSystem g_VRIK;

