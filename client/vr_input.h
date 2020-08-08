#ifndef VR_INPUT_H
#define VR_INPUT_H

#pragma once

#include "vr.h"
#include "vr_openvr.h"
#include "kbutton.h"
#include "input.h"

//-----------------------------------------------------------------------------
// Purpose: VR Input interface
//-----------------------------------------------------------------------------
class CVRInput : public CInput
{
public:
	typedef CInput BaseClass;

	CVRInput()
	{
		oldViewPos.Zero();
	}

	// actual input changes
	virtual void JoyStickMove( float frametime, CUserCmd *cmd );
	
	// minor input changes
	virtual void AccumulateMouse();

	Vector oldViewPos;
};


#endif

