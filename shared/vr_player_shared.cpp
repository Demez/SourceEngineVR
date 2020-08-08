#include "cbase.h"# 

#ifdef CLIENT_DLL
#include "vr_cl_player.h"
#else
#include "vr_sv_player.h"
#endif


void CVRBasePlayer::RegisterUserMessages()
{
	/*usermessages->Register( "", -1 );

	SingleUserRecipientFilter user( this );
	user.MakeReliable();

	UserMessageBegin( user, "VRUtilEventPreRender" );
	WRITE_SHORT( (int)m_ArmorValue);
	MessageEnd();*/
}

/*
CSingleUserRecipientFilter user( this );
user.MakeReliable();

UserMessageBegin( user, "VRUtilEventPreRender" );
WRITE_SHORT( (int)m_ArmorValue);
MessageEnd();
*/

