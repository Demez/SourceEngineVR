#ifndef VR_PREDICTION_H
#define VR_PREDICTION_H

#include "prediction.h"

#if defined( CLIENT_DLL )
	class C_VRBasePlayer;
	#include "vr_cl_player.h"
	#define CVRBasePlayer C_VRBasePlayer
#else
	#include "vr_sv_player.h"
#endif


class CVRPrediction: public CPrediction
{
	DECLARE_CLASS( CVRPrediction, CPrediction )

public:
	void                        SetupMove( C_BasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *moveBase );

	bool						vr_active;
	float						vr_viewRotation;
	Vector						vr_hmdOrigin;
	Vector						vr_hmdOriginOffset;
	CUtlVector< CmdVRTracker >	vr_trackers;
};


CVRPrediction* GetVRPrediction();

inline CVRPrediction* ToVRPrediction( CPrediction* pred )
{
	return (CVRPrediction*)pred;
}



#endif