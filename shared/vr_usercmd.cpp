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
#include "vr_tracker.h"

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

extern EVRTracker GetTrackerEnum( const char* name );

//-----------------------------------------------------------------------------
// Purpose: Write a delta compressed user command.
// Input  : *buf - 
//			*to - 
//			*from - 
// Output : static
//-----------------------------------------------------------------------------
#if 1 // ENGINE_NEW
void WriteUsercmdVR( bf_write *buf, const CUserCmd *to, const CUserCmd *from )
#else
void WriteUsercmdVR( bf_write *buf, CUserCmd *to, CUserCmd *from )
#endif
{
	LogUserCmd( "\t%s %d -> %d\n", "vr_active", from->vr_active, to->vr_active );
	buf->WriteOneBit( to->vr_active );

	if (!to->vr_active)
		return;

	WriteUserCmdDeltaFloat( buf, "vr_viewRotation", from->vr_viewRotation, to->vr_viewRotation );

	// handle trackers
	LogUserCmd( "\t%s %d\n", "vr_trackers", to->vr_trackers.Count() );
	buf->WriteShort( to->vr_trackers.Count() );

	// rare case, no need to be writing an extra bit all the time
	// could probably be the same with the tracker pos and ang below
	if ( to->vr_trackers.Count() == 0 )
	{
		return;
	}

	bool leftHand = false;
	bool rightHand = false;

	for (int i = 0; i < to->vr_trackers.Count(); i++)
	{
		// bool hasPrevTracker = from->vr_trackers.Count() == i + 1;

		if ( GetTrackerEnum(to->vr_trackers[i].index) == EVRTracker::LHAND ) 
		{
			leftHand = true;
		}
		else if ( GetTrackerEnum(to->vr_trackers[i].index) == EVRTracker::RHAND ) 
		{
			rightHand = true;
		}
			
		// less data to send by sending a single short for the name index instead of the full string
		buf->WriteShort( to->vr_trackers[i].index );

		// since items in vectors aren't saved, a bit and a float would always be sent for each one
		// so we can just always write it without those extra bits in that case

		// would WriteBitFloat, WriteBitVec3Coord or even WriteBitCoordMP be more efficent here?
		buf->WriteFloat( to->vr_trackers[i].pos[0] );
		buf->WriteFloat( to->vr_trackers[i].pos[1] );
		buf->WriteFloat( to->vr_trackers[i].pos[2] );

		buf->WriteFloat( to->vr_trackers[i].ang[0] );
		buf->WriteFloat( to->vr_trackers[i].ang[1] );
		buf->WriteFloat( to->vr_trackers[i].ang[2] );

		// this probably can be calculated so we can send less data, but this is the lazy way since it gives the velocity already
		// WriteUserCmdDeltaFloat( buf, "vr_trackers[i].vel[0]", FROM_TRACKER(from->vr_trackers[i].vel[0]), to->vr_trackers[i].vel[0] );
		// WriteUserCmdDeltaFloat( buf, "vr_trackers[i].vel[1]", FROM_TRACKER(from->vr_trackers[i].vel[1]), to->vr_trackers[i].vel[1] );
		// WriteUserCmdDeltaFloat( buf, "vr_trackers[i].vel[2]", FROM_TRACKER(from->vr_trackers[i].vel[2]), to->vr_trackers[i].vel[2] );
	}

	if ( leftHand )
	{
		// Left hand skeleton
		for (int i = 0; i < 5; i++)
		{
			WriteUserCmdDeltaFloat( buf, "vr_fingerCurlsL[i].x", from->vr_fingerCurlsL[i].x, to->vr_fingerCurlsL[0].x );
			WriteUserCmdDeltaFloat( buf, "vr_fingerCurlsL[i].y", from->vr_fingerCurlsL[i].y, to->vr_fingerCurlsL[0].y );
		}
	}

	if ( rightHand )
	{
		// Right hand skeleton
		for (int i = 0; i < 5; i++)
		{
			WriteUserCmdDeltaFloat( buf, "vr_fingerCurlsR[i].x", from->vr_fingerCurlsR[i].x, to->vr_fingerCurlsR[0].x );
			WriteUserCmdDeltaFloat( buf, "vr_fingerCurlsR[i].y", from->vr_fingerCurlsR[i].y, to->vr_fingerCurlsR[0].y );
		}
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
	move->vr_active = buf->ReadOneBit();

	if (!move->vr_active)
		return;

	if ( buf->ReadOneBit() )
		move->vr_viewRotation = buf->ReadFloat();

	int count = buf->ReadShort();

	bool leftHand = false;
	bool rightHand = false;

	for (int i = 0; i < count; i++)
	{
		// ok so apparently, due to how the usercmd works, CUserCmd* move is updated multiple times (maybe twice?)
		// and because i don't bother checking here, i accidently store each tracker twice, and then in CVRBasePlayerShared::UpdateTrackers,
		// it probably ends up updating that tracker twice for no reason, so check to see if that tracker exists already

		short index = buf->ReadShort();
		bool trackerFound = false;

		for (int j = 0; j < move->vr_trackers.Count(); j++)
		{
			if ( move->vr_trackers[j].index == index )
			{
				trackerFound = true;
				break;
			}
		}

		if ( !trackerFound )
		{
			move->vr_trackers.AddToTail( CmdVRTracker() );

			move->vr_trackers[i].index = index;
			move->vr_trackers[i].name = GetTrackerName(move->vr_trackers[i].index);
		}

		if ( GetTrackerEnum(move->vr_trackers[i].index) == EVRTracker::LHAND ) 
		{
			leftHand = true;
		}
		else if ( GetTrackerEnum(move->vr_trackers[i].index) == EVRTracker::RHAND ) 
		{
			rightHand = true;
		}

		move->vr_trackers[i].pos[0] = buf->ReadFloat();
		move->vr_trackers[i].pos[1] = buf->ReadFloat();
		move->vr_trackers[i].pos[2] = buf->ReadFloat();

		move->vr_trackers[i].ang[0] = buf->ReadFloat();
		move->vr_trackers[i].ang[1] = buf->ReadFloat();
		move->vr_trackers[i].ang[2] = buf->ReadFloat();

		// if (buf->ReadOneBit()) move->vr_trackers[i].vel[0] = buf->ReadFloat();
		// if (buf->ReadOneBit()) move->vr_trackers[i].vel[1] = buf->ReadFloat();
		// if (buf->ReadOneBit()) move->vr_trackers[i].vel[2] = buf->ReadFloat();
	}

	// if left hand exists, read the skeleton
	if ( leftHand )
	{
		for (int i = 0; i < 5; i++)
		{
			if ( buf->ReadOneBit() )
				move->vr_fingerCurlsL[i].x = buf->ReadFloat();

			if ( buf->ReadOneBit() )
				move->vr_fingerCurlsL[i].y = buf->ReadFloat();
		}
	}

	// if right hand exists, read the skeleton
	if ( rightHand )
	{
		for (int i = 0; i < 5; i++)
		{
			if ( buf->ReadOneBit() )
				move->vr_fingerCurlsR[i].x = buf->ReadFloat();

			if ( buf->ReadOneBit() )
				move->vr_fingerCurlsR[i].y = buf->ReadFloat();
		}
	}
}

