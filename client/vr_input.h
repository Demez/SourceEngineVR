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
		bFlashlightOn = false;
		bInNextWeapon = false;
	}

	// actual input changes
	virtual void JoyStickMove( float frametime, CUserCmd *cmd );
	virtual int GetButtonBits( bool bResetState );

	// new
	virtual void VRMove( float frametime, CUserCmd *cmd );
	virtual void CalcButtonBitsBool( const char* action, int in_button );
	virtual void CalcButtonBitsToggleCmd( const char* action, bool& bEnabled, const char* cmd );

	// minor input changes
	virtual void AccumulateMouse();
	virtual void AccumulateMouse( int nSlot );

	Vector oldViewPos;

	Vector headOrigin;
	Vector lastHeadOrigin;

	int m_bits;
	bool bFlashlightOn;
	bool bInNextWeapon;
};


#endif

