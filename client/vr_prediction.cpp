#include "cbase.h"
#include "vr_prediction.h"
#include "vr_gamemovement.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


extern CPrediction* prediction;

CVRPrediction* GetVRPrediction()
{
	return (CVRPrediction*)prediction;
}


void CVRPrediction::SetupMove( C_BasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *moveBase ) 
{
#if !defined( NO_ENTITY_PREDICTION )
	CVRMoveData* move = ToVRMoveData( moveBase );

	if ( ucmd->vr_active == 1 )
	{
		move->vr_active = true;
		move->vr_viewRotation = ucmd->vr_viewRotation;
		move->vr_hmdOrigin = ucmd->vr_hmdOrigin;
		move->vr_hmdOriginOffset = ucmd->vr_hmdOriginOffset;
		move->vr_trackers = ucmd->vr_trackers;
	}
	else
	{
		if ( !move->vr_active )
		{
			move->m_vecAbsViewAngles.Init();
			move->m_vecViewAngles.Init();
		}

		move->vr_active = false;
		move->vr_viewRotation = 0.0f;
		move->vr_hmdOrigin.Init();
		move->vr_hmdOriginOffset.Init();
	}

#endif

	BaseClass::SetupMove( player, ucmd, pHelper, moveBase );
}

