#include "cbase.h"
#include "kbutton.h"
#include "input.h"
#include "vgui/isurface.h"
#include "vgui_controls/controls.h"
#include "vgui/cursor.h"
#include "cdll_client_int.h"
#include "cdll_util.h"
#include "iclientvehicle.h"
#include "inputsystem/iinputsystem.h"

#include "vr_cl_player.h"
#include "vr.h"
#include "vr_input.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


ConVar vr_allow_mouse("vr_allow_mouse", "0", FCVAR_CLIENTDLL);
ConVar vr_disable_joy("vr_disable_joy", "0", FCVAR_CLIENTDLL);
// ConVar vr_move_mode("vr_move_mode", "0", FCVAR_CLIENTDLL, "0 for normal/non-vr, 1 - Headset for movement direction, 2 or 3 for L/R controller move direction");

ConVar vr_forward_speed("vr_forward_speed", "450", FCVAR_CLIENTDLL);
ConVar vr_side_speed("vr_side_speed", "450", FCVAR_CLIENTDLL);
ConVar vr_back_speed("vr_back_speed", "450", FCVAR_CLIENTDLL);
ConVar vr_turn_speed("vr_turn_speed", "1.5", FCVAR_CLIENTDLL);
// ConVar vr_turn_snap("vr_turn_snap", "0", FCVAR_CLIENTDLL);

extern ConVar vr_scale;

/*
ConVar cl_sidespeed( "cl_sidespeed", "450", FCVAR_CHEAT );
ConVar cl_upspeed( "cl_upspeed", "320", FCVAR_CHEAT );
ConVar cl_forwardspeed( "cl_forwardspeed", "450", FCVAR_CHEAT );
ConVar cl_backspeed( "cl_backspeed", "450", FCVAR_CHEAT );
*/

extern ConVar joy_response_move;
extern ConVar joy_response_move_vehicle;
extern ConVar joy_response_look;

static ConVar joy_forwardthreshold( "joy_forwardthreshold", "0.15", FCVAR_ARCHIVE );
static ConVar joy_sidethreshold( "joy_sidethreshold", "0.15", FCVAR_ARCHIVE );
static ConVar joy_pitchthreshold( "joy_pitchthreshold", "0.15", FCVAR_ARCHIVE );
static ConVar joy_yawthreshold( "joy_yawthreshold", "0.15", FCVAR_ARCHIVE );
static ConVar joy_forwardsensitivity( "joy_forwardsensitivity", "-1", FCVAR_ARCHIVE );
static ConVar joy_sidesensitivity( "joy_sidesensitivity", "1", FCVAR_ARCHIVE );
static ConVar joy_pitchsensitivity( "joy_pitchsensitivity", "1", FCVAR_ARCHIVE | FCVAR_ARCHIVE_XBOX );
static ConVar joy_yawsensitivity( "joy_yawsensitivity", "-1", FCVAR_ARCHIVE | FCVAR_ARCHIVE_XBOX );


// ====================================================================================================
// actual changes
// ====================================================================================================
void CVRInput::JoyStickMove( float frametime, CUserCmd *cmd )
{
	if ( !g_VR.active || vr_disable_joy.GetBool() )
	{
		BaseClass::JoyStickMove( frametime, cmd );
		return;
	}

	// what is this?
	if ( m_flRemainingJoystickSampleTime <= 0 )
		return;
	frametime = min(m_flRemainingJoystickSampleTime, frametime);
	m_flRemainingJoystickSampleTime -= frametime;

	QAngle viewangles;
	C_VRBasePlayer *pPlayer = (C_VRBasePlayer*)C_BasePlayer::GetLocalPlayer();
	VRTracker* hmd = g_VR.GetTrackerByName("hmd");

	if ( !vr_disable_eye_ang.GetBool() )
	{
		VRVector2Action* turning = (VRVector2Action*)g_VR.GetActionByName( "vector2_smoothturn" );
		pPlayer->AddViewRotateOffset( -turning->x * vr_turn_speed.GetFloat() );
		viewangles = pPlayer->EyeAngles();
	}
	else
	{
		engine->GetViewAngles( viewangles );
	}

	VRVector2Action* walkDir = (VRVector2Action*)g_VR.GetActionByName( "vector2_walkdirection" );

	// TODO: if in a vehicle, use the index finger trigger buttons (on the rift controllers)
	float forward = walkDir->y * -1;
	float side    = walkDir->x;

	int iResponseCurve = 0;
	if ( pPlayer && pPlayer->IsInAVehicle() )
	{
		iResponseCurve = pPlayer->GetVehicle() ? pPlayer->GetVehicle()->GetJoystickResponseCurve() : joy_response_move_vehicle.GetInt();
	}
	else
	{
		iResponseCurve = joy_response_move.GetInt();
	}

	float val = ResponseCurve( iResponseCurve, forward, PITCH, joy_forwardsensitivity.GetFloat() );
	float joyForwardMove = val * vr_forward_speed.GetFloat();

	val = ResponseCurve( iResponseCurve, side, YAW, joy_sidesensitivity.GetFloat() );
	float joySideMove = val * vr_side_speed.GetFloat();

	if ( hmd != NULL )
	{
		// doesn't work with the view turning

		Vector hmdPos = hmd->pos * vr_scale.GetFloat();
		pPlayer->SetViewOffset(hmdPos);

		Vector newViewPos = oldViewPos - hmdPos;

		joyForwardMove += joyForwardMove + newViewPos.x;
		joySideMove += joySideMove + newViewPos.y;
		oldViewPos = hmdPos;
	}

	// apply player motion relative to screen space
	cmd->forwardmove += joyForwardMove;
	cmd->sidemove += joySideMove;

	CCommand tmp;
	if ( FloatMakePositive(joyForwardMove) >= joy_autosprint.GetFloat() || FloatMakePositive(joySideMove) >= joy_autosprint.GetFloat() )
	{
		KeyDown( &in_joyspeed, NULL );
	}
	else
	{
		KeyUp( &in_joyspeed, NULL );
	}

	engine->SetViewAngles( viewangles );
}


// ====================================================================================================
// minor changes
// ====================================================================================================
void CVRInput::AccumulateMouse()
{
	if ( !g_VR.active || vr_allow_mouse.GetBool() )
		BaseClass::AccumulateMouse();
}


