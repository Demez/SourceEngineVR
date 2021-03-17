#include "cbase.h"
#include "ivmodemanager.h"


class CVRModeManager : public IVModeManager
{
public:
				CVRModeManager( void );
	virtual		~CVRModeManager( void );

	virtual void	CreateMove( float flInputSampleTime, CUserCmd *cmd );

	// unused
	virtual void	Init( void ) {}
	virtual void	SwitchMode( bool commander, bool force ) {}
	virtual void	OverrideView( CViewSetup *pSetup ) {}
	virtual void	LevelInit( const char *newmap ) {}
	virtual void	LevelShutdown( void ) {}
};

