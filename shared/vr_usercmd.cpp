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

// const std::vector<const char*> g_trackerNames = 
const int g_trackerCount = 6;

const char* g_trackerNames[g_trackerCount] = 
{
	"hmd",
	"pose_lefthand",
	"pose_righthand",
	"pose_hip",
	"pose_leftfoot",
	"pose_rightfoot",
};


const char* GetTrackerName(short index)
{
	if (index < 0 || index > g_trackerCount)
	{
		Warning("Invalid tracker index %h", index);
		return "";
	}

	return g_trackerNames[index];
}


short GetTrackerIndex(const char* name)
{
	for (int i = 0; i < g_trackerCount; i++)
	{
		if ( V_strcmp(name, g_trackerNames[i]) == 0 )
		{
			return i;
		}
	}

	return -1;
}


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

	WriteUserCmdDeltaFloat( buf, "vr_viewRotation", from->vr_viewRotation, to->vr_viewRotation );

	// could probably use buf->WriteBitVec3Coord( to );
	WriteUserCmdDeltaFloat( buf, "vr_hmdOrigin[0]", from->vr_hmdOrigin[0], to->vr_hmdOrigin[0] );
	WriteUserCmdDeltaFloat( buf, "vr_hmdOrigin[1]", from->vr_hmdOrigin[1], to->vr_hmdOrigin[1] );
	WriteUserCmdDeltaFloat( buf, "vr_hmdOrigin[2]", from->vr_hmdOrigin[2], to->vr_hmdOrigin[2] );

	WriteUserCmdDeltaFloat( buf, "vr_hmdOriginOffset[0]", from->vr_hmdOriginOffset[0], to->vr_hmdOriginOffset[0] );
	WriteUserCmdDeltaFloat( buf, "vr_hmdOriginOffset[1]", from->vr_hmdOriginOffset[1], to->vr_hmdOriginOffset[1] );
	WriteUserCmdDeltaFloat( buf, "vr_hmdOriginOffset[2]", from->vr_hmdOriginOffset[2], to->vr_hmdOriginOffset[2] );

	// handle trackers
	if ( to->vr_trackers.Count() != 0 )
	{
		LogUserCmd( "\t%s %d\n", "vr_trackers", to->vr_trackers.Count() );

		buf->WriteOneBit( 1 );
		buf->WriteShort( to->vr_trackers.Count() );

		for (int i = 0; i < to->vr_trackers.Count(); i++)
		{
			// const_cast<CUserCmd*>(from)->vr_trackers.AddToTail( CmdVRTracker() );
			// less data to send by sending a single short for the name index instead of the full string
			// const_cast<CUserCmd*>(to)->vr_trackers[i].index = GetTrackerIndex(to->vr_trackers[i].name);
			if ( from->vr_trackers.Count() == i + 1 )
			{
				WriteUserCmdDeltaShort( buf, "vr_trackers[i].index", from->vr_trackers[i].index, to->vr_trackers[i].index );

				WriteUserCmdDeltaFloat( buf, "vr_trackers[i].pos[0]", from->vr_trackers[i].pos[0], to->vr_trackers[i].pos[0] );
				WriteUserCmdDeltaFloat( buf, "vr_trackers[i].pos[1]", from->vr_trackers[i].pos[1], to->vr_trackers[i].pos[1] );
				WriteUserCmdDeltaFloat( buf, "vr_trackers[i].pos[2]", from->vr_trackers[i].pos[2], to->vr_trackers[i].pos[2] );

				WriteUserCmdDeltaFloat( buf, "vr_trackers[i].ang[0]", from->vr_trackers[i].ang[0], to->vr_trackers[i].ang[0] );
				WriteUserCmdDeltaFloat( buf, "vr_trackers[i].ang[1]", from->vr_trackers[i].ang[1], to->vr_trackers[i].ang[1] );
				WriteUserCmdDeltaFloat( buf, "vr_trackers[i].ang[2]", from->vr_trackers[i].ang[2], to->vr_trackers[i].ang[2] );
			}
			else
			{
				WriteUserCmdDeltaShort( buf, "vr_trackers[i].index", -1, to->vr_trackers[i].index );

				WriteUserCmdDeltaFloat( buf, "vr_trackers[i].pos[0]", 0.0, to->vr_trackers[i].pos[0] );
				WriteUserCmdDeltaFloat( buf, "vr_trackers[i].pos[1]", 0.0, to->vr_trackers[i].pos[1] );
				WriteUserCmdDeltaFloat( buf, "vr_trackers[i].pos[2]", 0.0, to->vr_trackers[i].pos[2] );

				WriteUserCmdDeltaFloat( buf, "vr_trackers[i].ang[0]", 0.0, to->vr_trackers[i].ang[0] );
				WriteUserCmdDeltaFloat( buf, "vr_trackers[i].ang[1]", 0.0, to->vr_trackers[i].ang[1] );
				WriteUserCmdDeltaFloat( buf, "vr_trackers[i].ang[2]", 0.0, to->vr_trackers[i].ang[2] );
			}
		}
	}
	else
	{
		buf->WriteOneBit( 0 );
	}
}

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

	if ( buf->ReadOneBit() )
		move->vr_viewRotation = buf->ReadFloat();


	// Read direction
	if ( buf->ReadOneBit() )
		move->vr_hmdOrigin[0] = buf->ReadFloat();

	if ( buf->ReadOneBit() )
		move->vr_hmdOrigin[1] = buf->ReadFloat();

	if ( buf->ReadOneBit() )
		move->vr_hmdOrigin[2] = buf->ReadFloat();


	if ( buf->ReadOneBit() )
		move->vr_hmdOriginOffset[0] = buf->ReadFloat();

	if ( buf->ReadOneBit() )
		move->vr_hmdOriginOffset[1] = buf->ReadFloat();

	if ( buf->ReadOneBit() )
		move->vr_hmdOriginOffset[2] = buf->ReadFloat();


	if ( buf->ReadOneBit() )
	{
		// move->vr_trackers.SetCount( buf->ReadShort() );
		int count = buf->ReadShort();

		int i;
		for (i = 0; i < count; i++)
		{
			move->vr_trackers.AddToTail( CmdVRTracker() );

			if ( buf->ReadOneBit() )
				move->vr_trackers[i].index = buf->ReadShort();
			// move->vr_trackers[i].name = GetTrackerName(move->vr_trackers[i].index);

			/*short nameLength = buf->ReadShort();
			
			char name[32] = "\0";
			buf->ReadString(name, nameLength);
			move->vr_trackers[i].name = name;*/

			move->vr_trackers[i].name = GetTrackerName(move->vr_trackers[i].index);

			if ( buf->ReadOneBit() )
				move->vr_trackers[i].pos[0] = buf->ReadFloat();

			if ( buf->ReadOneBit() )
				move->vr_trackers[i].pos[1] = buf->ReadFloat();

			if ( buf->ReadOneBit() )
				move->vr_trackers[i].pos[2] = buf->ReadFloat();


			if ( buf->ReadOneBit() )
				move->vr_trackers[i].ang[0] = buf->ReadFloat();

			if ( buf->ReadOneBit() )
				move->vr_trackers[i].ang[1] = buf->ReadFloat();

			if ( buf->ReadOneBit() )
				move->vr_trackers[i].ang[2] = buf->ReadFloat();
		}
	}
}

