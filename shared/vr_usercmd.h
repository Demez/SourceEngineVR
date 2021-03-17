#ifndef VR_USERCMD_H
#define VR_USERCMD_H
#ifdef _WIN32
#pragma once
#endif

#include "mathlib/vector.h"
#include "utlvector.h"
// #include "imovehelper.h"
// #include "usercmd.h"


enum class EVRTracker
{
	LHAND = 0,
	RHAND,
	HIP,
	LFOOT,
	RFOOT,
	COUNT
};


struct CmdVRTracker
{
	const char* name;
	short index;  // enum value is passed into it
	Vector pos;
	QAngle ang;
};


class bf_read;
class bf_write;
class CUserCmd;


// Call these in your Read/WriteUsercmd functions!!!!
// also you have to add the variables yourself since you can't inherit any classes with CUserCmd, sad
void ReadUsercmdVR( bf_read *buf, CUserCmd *move, CUserCmd *from );

#if ENGINE_NEW
void WriteUsercmdVR( bf_write *buf, const CUserCmd *to, const CUserCmd *from );
#else
void WriteUsercmdVR( bf_write *buf, CUserCmd *to, CUserCmd *from );
#endif

#endif // VR_USERCMD_H

