#ifndef VR_INPUT_H
#define VR_INPUT_H

#pragma once

#include "vr.h"
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
		bFlashlightOn = false;
		bInNextWeapon = false;
	}

	// button inputs
	virtual int GetButtonBits( bool bResetState );
	virtual void CalcButtonBitsBool( const char* action, int in_button );
	virtual void CalcButtonBitsToggleCmd( const char* action, bool& bEnabled, const char* cmd );

	// new
	virtual void VRMove( float frametime, CUserCmd *cmd );
	virtual void JoyStickInput( float frametime, CUserCmd *cmd );
	virtual void VRHeadsetAngles( float frametime );

#if ENGINE_NEW
	virtual void AdjustAngles( int nSlot, float frametime );
#else
	virtual void AdjustAngles( float frametime );
#endif

	int m_bits;
	bool bFlashlightOn;
	bool bInNextWeapon;
};


extern CVRInput* GetVRInput();


#endif

