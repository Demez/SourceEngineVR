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
		oldViewPos.Zero();
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

	// minor input changes
#if ENGINE_NEW
	// virtual void AccumulateMouse( int nSlot );
	virtual void AdjustAngles( int nSlot, float frametime );
#else
	// virtual void AccumulateMouse();
	virtual void AdjustAngles( float frametime );
#endif

	Vector oldViewPos;

	Vector headOrigin;
	Vector lastHeadOrigin;

	// test
	QAngle m_hmdPrevAngles;

	int m_bits;
	bool bFlashlightOn;
	bool bInNextWeapon;
};


#endif

