//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "vr_usercmd.h"
#include "bitbuf.h"
#include "checksum_md5.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// TF2 specific, need enough space for OBJ_LAST items from tf_shareddefs.h
#define WEAPON_SUBTYPE_BITS	6

#ifdef CLIENT_DLL
extern ConVar net_showusercmd;
#define LogUserCmd( msg, ... ) if ( net_showusercmd.GetInt() ) { ConDMsg( msg, __VA_ARGS__ ); }
#else
#define LogUserCmd( msg, ... ) NULL;
#endif

static bool WriteUserCmdDeltaShort( bf_write *buf, char *what, int from, int to )
{
	if ( from != to )
	{
		LogUserCmd( "\t%s %d -> %d\n", what, from, to );

		buf->WriteOneBit( 1 );
		buf->WriteShort( to );
		return true;
	}

	buf->WriteOneBit( 0 );
	return false;
}

static bool WriteUserCmdDeltaFloat( bf_write *buf, char *what, float from, float to )
{
	if ( from != to )
	{
		LogUserCmd( "\t%s %2.2f -> %2.2f\n", what, from, to );

		buf->WriteOneBit( 1 );
		buf->WriteFloat( to );
		return true;
	}

	buf->WriteOneBit( 0 );
	return false;
}


static bool ReadUserCmdDeltaFloat( bf_write *buf, char *what, float from, float to )
{
	if ( from != to )
	{
		LogUserCmd( "\t%s %2.2f -> %2.2f\n", what, from, to );

		buf->WriteOneBit( 1 );
		buf->WriteFloat( to );
		return true;
	}

	buf->WriteOneBit( 0 );
	return false;
}


#define FROM_TRACKER(source) hasPrevTracker ? source : 0.0


//-----------------------------------------------------------------------------
// Purpose: Write a delta compressed user command.
// Input  : *buf - 
//			*to - 
//			*from - 
// Output : static
//-----------------------------------------------------------------------------
#if ENGINE_NEW
void WriteUsercmdVR( bf_write *buf, const CUserCmd *to, const CUserCmd *from )
#else
void WriteUsercmdVR( bf_write *buf, CUserCmd *to, CUserCmd *from )
#endif
{
	// TODO: Can probably get away with fewer bits.
	WriteUserCmdDeltaShort( buf, "vr_active", from->vr_active, to->vr_active );

	if (!to->vr_active)
		return;

	WriteUserCmdDeltaFloat( buf, "vr_viewRotation", from->vr_viewRotation, to->vr_viewRotation );

	// could probably use buf->WriteBitVec3Coord( to );
	WriteUserCmdDeltaFloat( buf, "vr_originOffset[0]", from->vr_originOffset[0], to->vr_originOffset[0] );
	WriteUserCmdDeltaFloat( buf, "vr_originOffset[1]", from->vr_originOffset[1], to->vr_originOffset[1] );
	WriteUserCmdDeltaFloat( buf, "vr_originOffset[2]", from->vr_originOffset[2], to->vr_originOffset[2] );

	// handle trackers
	if ( to->vr_trackers.Count() != 0 )
	{
		LogUserCmd( "\t%s %d\n", "vr_trackers", to->vr_trackers.Count() );

		buf->WriteOneBit( 1 );
		buf->WriteShort( to->vr_trackers.Count() );

		for (int i = 0; i < to->vr_trackers.Count(); i++)
		{
			bool hasPrevTracker = from->vr_trackers.Count() == i + 1;
			
			// less data to send by sending a single short for the name index instead of the full string
			WriteUserCmdDeltaShort( buf, "vr_trackers[i].index", FROM_TRACKER(from->vr_trackers[i].index), to->vr_trackers[i].index );

			WriteUserCmdDeltaFloat( buf, "vr_trackers[i].pos[0]", FROM_TRACKER(from->vr_trackers[i].pos[0]), to->vr_trackers[i].pos[0] );
			WriteUserCmdDeltaFloat( buf, "vr_trackers[i].pos[1]", FROM_TRACKER(from->vr_trackers[i].pos[1]), to->vr_trackers[i].pos[1] );
			WriteUserCmdDeltaFloat( buf, "vr_trackers[i].pos[2]", FROM_TRACKER(from->vr_trackers[i].pos[2]), to->vr_trackers[i].pos[2] );

			WriteUserCmdDeltaFloat( buf, "vr_trackers[i].ang[0]", FROM_TRACKER(from->vr_trackers[i].ang[0]), to->vr_trackers[i].ang[0] );
			WriteUserCmdDeltaFloat( buf, "vr_trackers[i].ang[1]", FROM_TRACKER(from->vr_trackers[i].ang[1]), to->vr_trackers[i].ang[1] );
			WriteUserCmdDeltaFloat( buf, "vr_trackers[i].ang[2]", FROM_TRACKER(from->vr_trackers[i].ang[2]), to->vr_trackers[i].ang[2] );
		}
	}
	else
	{
		buf->WriteOneBit( 0 );
	}

	// Left hand skeleton
	for (int i = 0; i < 5; i++)
	{
		WriteUserCmdDeltaFloat( buf, "vr_fingerCurlsL[i].x", from->vr_fingerCurlsL[i].x, to->vr_fingerCurlsL[0].x );
		WriteUserCmdDeltaFloat( buf, "vr_fingerCurlsL[i].y", from->vr_fingerCurlsL[i].y, to->vr_fingerCurlsL[0].y );
	}

	// Right hand skeleton
	for (int i = 0; i < 5; i++)
	{
		WriteUserCmdDeltaFloat( buf, "vr_fingerCurlsR[i].x", from->vr_fingerCurlsR[i].x, to->vr_fingerCurlsR[0].x );
		WriteUserCmdDeltaFloat( buf, "vr_fingerCurlsR[i].y", from->vr_fingerCurlsR[i].y, to->vr_fingerCurlsR[0].y );
	}
}


#define GET_VEC(output) \
	if ( buf->ReadOneBit() ) output[0] = buf->ReadFloat(); \
	if ( buf->ReadOneBit() ) output[1] = buf->ReadFloat(); \
	if ( buf->ReadOneBit() ) output[2] = buf->ReadFloat()


extern const char* GetTrackerName( short index );

//-----------------------------------------------------------------------------
// Purpose: Read in a delta compressed usercommand.
// Input  : *buf - 
//			*move - 
//			*from - 
// Output : static void ReadUsercmd
//-----------------------------------------------------------------------------
void ReadUsercmdVR( bf_read *buf, CUserCmd *move, CUserCmd *from )
{
	if ( buf->ReadOneBit() )
		move->vr_active = buf->ReadShort();

	if (!move->vr_active)
		return;

	if ( buf->ReadOneBit() )
		move->vr_viewRotation = buf->ReadFloat();

	// Read direction
	if ( buf->ReadOneBit() )
		move->vr_originOffset[0] = buf->ReadFloat();

	if ( buf->ReadOneBit() )
		move->vr_originOffset[1] = buf->ReadFloat();

	if ( buf->ReadOneBit() )
		move->vr_originOffset[2] = buf->ReadFloat();


	if ( buf->ReadOneBit() )
	{
		int count = buf->ReadShort();

		for (int i = 0; i < count; i++)
		{
			move->vr_trackers.AddToTail( CmdVRTracker() );

			if (buf->ReadOneBit()) move->vr_trackers[i].index = buf->ReadShort();

			if (buf->ReadOneBit()) move->vr_trackers[i].pos[0] = buf->ReadFloat();
			if (buf->ReadOneBit()) move->vr_trackers[i].pos[1] = buf->ReadFloat();
			if (buf->ReadOneBit()) move->vr_trackers[i].pos[2] = buf->ReadFloat();

			if (buf->ReadOneBit()) move->vr_trackers[i].ang[0] = buf->ReadFloat();
			if (buf->ReadOneBit()) move->vr_trackers[i].ang[1] = buf->ReadFloat();
			if (buf->ReadOneBit()) move->vr_trackers[i].ang[2] = buf->ReadFloat();

			move->vr_trackers[i].name = GetTrackerName(move->vr_trackers[i].index);
		}
	}

	// Left hand skeleton
	for (int i = 0; i < 5; i++)
	{
		if ( buf->ReadOneBit() )
			move->vr_fingerCurlsL[i].x = buf->ReadFloat();

		if ( buf->ReadOneBit() )
			move->vr_fingerCurlsL[i].y = buf->ReadFloat();
	}

	// Right hand skeleton
	for (int i = 0; i < 5; i++)
	{
		if ( buf->ReadOneBit() )
			move->vr_fingerCurlsR[i].x = buf->ReadFloat();

		if ( buf->ReadOneBit() )
			move->vr_fingerCurlsR[i].y = buf->ReadFloat();
	}
}

