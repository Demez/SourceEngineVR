#ifndef VR_PLAYER_MOVE_H
#define VR_PLAYER_MOVE_H
#pragma once


#include "player_command.h"
#include "networkvar.h"


class IMoveHelper;
class CMoveData;
class CBasePlayer;

//-----------------------------------------------------------------------------
// Purpose: Server side player movement
//-----------------------------------------------------------------------------
class CVRPlayerMove: public CPlayerMove
{
public:
	DECLARE_CLASS( CVRPlayerMove, CPlayerMove );
	
	// Construction/destruction
					CVRPlayerMove( void );
	virtual			~CVRPlayerMove( void ) {}

	virtual void	SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move );
};


#endif // VR_PLAYER_MOVE_H
