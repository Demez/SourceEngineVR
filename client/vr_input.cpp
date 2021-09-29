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
#include "in_buttons.h"

#include "vr_cl_player.h"
#include "vr.h"
#include "vr_input.h"

#if DXVK_VR
#include "vr_dxvk.h"
extern IDXVK_VRSystem* g_pDXVK;
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


ConVar vr_mouse_look("vr_mouse_look", "0", FCVAR_CLIENTDLL);
ConVar vr_allow_keyboard("vr_allow_keyboard", "1", FCVAR_CLIENTDLL);
ConVar vr_disable_joy("vr_disable_joy", "0", FCVAR_CLIENTDLL);
// ConVar vr_move_mode("vr_move_mode", "0", FCVAR_CLIENTDLL, "0 for normal/non-vr, 1 - Headset for movement direction, 2 or 3 for L/R controller move direction");

ConVar vr_forward_speed("vr_forward_speed", "450", FCVAR_CLIENTDLL);
ConVar vr_side_speed("vr_side_speed", "450", FCVAR_CLIENTDLL);
ConVar vr_back_speed("vr_back_speed", "450", FCVAR_CLIENTDLL);
ConVar vr_turn_speed("vr_turn_speed", "150", FCVAR_ARCHIVE);
// ConVar vr_turn_snap("vr_turn_snap", "0", FCVAR_CLIENTDLL);

extern ConVar vr_spew_timings;

/*
ConVar cl_sidespeed( "cl_sidespeed", "450", FCVAR_CHEAT );
ConVar cl_upspeed( "cl_upspeed", "320", FCVAR_CHEAT );
ConVar cl_forwardspeed( "cl_forwardspeed", "450", FCVAR_CHEAT );
ConVar cl_backspeed( "cl_backspeed", "450", FCVAR_CHEAT );
*/

// extern ConVar joy_response_move;
extern ConVar joy_response_move_vehicle;
extern ConVar joy_response_look;
static ConVar joy_response_move( "joy_response_move", "1", FCVAR_ARCHIVE, "'Movement' stick response mode: 0=Linear, 1=quadratic, 2=cubic, 3=quadratic extreme, 4=power function(i.e., pow(x,1/sensitivity)), 5=two-stage" );

static ConVar joy_forwardthreshold( "joy_forwardthreshold", "0.15", FCVAR_ARCHIVE );
static ConVar joy_sidethreshold( "joy_sidethreshold", "0.15", FCVAR_ARCHIVE );
static ConVar joy_pitchthreshold( "joy_pitchthreshold", "0.15", FCVAR_ARCHIVE );
static ConVar joy_yawthreshold( "joy_yawthreshold", "0.15", FCVAR_ARCHIVE );
static ConVar joy_forwardsensitivity( "joy_forwardsensitivity", "-1", FCVAR_ARCHIVE );
static ConVar joy_sidesensitivity( "joy_sidesensitivity", "1", FCVAR_ARCHIVE );
static ConVar joy_pitchsensitivity( "joy_pitchsensitivity", "1", FCVAR_ARCHIVE );
static ConVar joy_yawsensitivity( "joy_yawsensitivity", "-1", FCVAR_ARCHIVE );

static float ResponseCurve( int curve, float x, int axis, float sensitivity )
{
	switch ( curve )
	{
	case 1:
		// quadratic
		if ( x < 0 )
			return -(x*x) * sensitivity;
		return x*x * sensitivity;

	case 2:
		// cubic
		return x*x*x*sensitivity;

	case 3:
	{
		// quadratic extreme
		float extreme = 1.0f;
		if ( fabs( x ) >= 0.95f )
		{
			extreme = 1.5f;
		}
		if ( x < 0 )
			return -extreme * x*x*sensitivity;
		return extreme * x*x*sensitivity;
	}
	case 4:
	{
		float flScale = sensitivity < 0.0f ? -1.0f : 1.0f;

		sensitivity = clamp( fabs( sensitivity ), 1.0e-8f, 1000.0f );

		float oneOverSens = 1.0f / sensitivity;

		if ( x < 0.0f )
		{
			flScale = -flScale;
		}

		float retval = clamp( powf( fabs( x ), oneOverSens ), 0.0f, 1.0f );
		return retval * flScale;
	}
	break;
	case 5:
	{
		float out = x;

		if( fabs(out) <= 0.6f )
		{
			out *= 0.5f;
		}

		out = out * sensitivity;
		return out;
	}
	break;
	/*case 6: // Custom for driving a vehicle!
	{
		if( axis == YAW )
		{
			// This code only wants to affect YAW axis (the left and right axis), which 
			// is used for turning in the car. We fall-through and use a linear curve on 
			// the PITCH axis, which is the vehicle's throttle. REALLY, these are the 'forward'
			// and 'side' axes, but we don't have constants for those, so we re-use the same
			// axis convention as the look stick. (sjb)
			float sign = 1;

			if( x  < 0.0 )
				sign = -1;

			x = fabs(x);

			if( x <= joy_vehicle_turn_lowend.GetFloat() )
				x = RemapVal( x, 0.0f, joy_vehicle_turn_lowend.GetFloat(), 0.0f, joy_vehicle_turn_lowmap.GetFloat() );
			else
				x = RemapVal( x, joy_vehicle_turn_lowend.GetFloat(), 1.0f, joy_vehicle_turn_lowmap.GetFloat(), 1.0f );

			return x * sensitivity * sign;
		}
		//else
		//	fall through and just return x*sensitivity below (as if using default curve)
	}
	//The idea is to create a max large walk zone surrounded by a max run zone.
	case 7:
	{
		float xAbs = fabs(x);
		if(xAbs < joy_sensitive_step0.GetFloat())
		{
			return 0;
		}
		else if (xAbs < joy_sensitive_step2.GetFloat())
		{
			return (85.0f/cl_forwardspeed.GetFloat()) * ((x < 0)? -1.0f : 1.0f);
		}
		else
		{
			return ((x < 0)? -1.0f : 1.0f);
		}
	}
	break;
	case 8: //same concept as above but with smooth speeds
	{
		float xAbs = fabs(x);
		if(xAbs < joy_sensitive_step0.GetFloat())
		{
			return 0;
		}
		else if (xAbs < joy_sensitive_step2.GetFloat())
		{
			float maxSpeed = (85.0f/cl_forwardspeed.GetFloat());
			float t = (xAbs-joy_sensitive_step0.GetFloat())
				/ (joy_sensitive_step2.GetFloat()-joy_sensitive_step0.GetFloat());
			float speed = t*maxSpeed;
			return speed * ((x < 0)? -1.0f : 1.0f);
		}
		else
		{
			float maxSpeed = 1.0f;
			float minSpeed = (85.0f/cl_forwardspeed.GetFloat());
			float t = (xAbs-joy_sensitive_step2.GetFloat())
				/ (1.0f-joy_sensitive_step2.GetFloat());
			float speed = t*(maxSpeed-minSpeed) + minSpeed;
			return speed * ((x < 0)? -1.0f : 1.0f);
		}
	}
	break;
	case 9: //same concept as above but with smooth speeds for walking and a hard speed for running
	{
		float xAbs = fabs(x);
		if(xAbs < joy_sensitive_step0.GetFloat())
		{
			return 0;
		}
		else if (xAbs < joy_sensitive_step1.GetFloat())
		{
			float maxSpeed = (85.0f/cl_forwardspeed.GetFloat());
			float t = (xAbs-joy_sensitive_step0.GetFloat())
				/ (joy_sensitive_step1.GetFloat()-joy_sensitive_step0.GetFloat());
			float speed = t*maxSpeed;
			return speed * ((x < 0)? -1.0f : 1.0f);
		}
		else if (xAbs < joy_sensitive_step2.GetFloat())
		{
			return (85.0f/cl_forwardspeed.GetFloat()) * ((x < 0)? -1.0f : 1.0f);
		}
		else
		{
			return ((x < 0)? -1.0f : 1.0f);
		}
	}*/
	break;
	}

	// linear
	return x*sensitivity;
}


CVRInput* GetVRInput()
{
	return (CVRInput*)input;
}


void CVRInput::CreateMove( int sequence_number, float input_sample_frametime, bool active )
{
#if DXVK_VR
	g_pDXVK->StartFrame();
#endif

	BaseClass::CreateMove( sequence_number, input_sample_frametime, active );
}


void CVRInput::ExtraMouseSample( float frametime, bool active )
{
#if DXVK_VR
	g_pDXVK->StartFrame();
#endif

	BaseClass::ExtraMouseSample( frametime, active );
}


// ====================================================================================================
// actual changes
// ====================================================================================================
void CVRInput::JoyStickInput( float frametime, CUserCmd *cmd )
{
	if ( !g_VR.active || vr_disable_joy.GetBool() )
		return;

	// maybe i should use this, makes it so this isn't updated as often, valve used it for a reason
	// if ( m_flRemainingJoystickSampleTime <= 0 )
	// 	return;
	// frametime = MIN(m_flRemainingJoystickSampleTime, frametime);
	// m_flRemainingJoystickSampleTime -= frametime;

	float forward = 0;
	float side = 0;

	VRVector2Action* walkDir = (VRVector2Action*)g_VR.GetActionByName( "vector2_walkdirection" );
	if ( walkDir != NULL )
	{
		forward = walkDir->y * -1;
		side = walkDir->x;
	}

	// TODO: if in a vehicle, use the index finger trigger buttons (on the rift controllers)
	// maybe different steamvr input profiles? people don't really like changing steamvr input,
	// but i don't feel like making some custom input system that you can change in-game for this at the moment, maybe in the future

	C_VRBasePlayer *pPlayer = (C_VRBasePlayer*)C_BasePlayer::GetLocalPlayer();

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

	// apply player motion relative to screen space
	cmd->forwardmove += joyForwardMove;
	cmd->sidemove += joySideMove;

	if ( FloatMakePositive(joyForwardMove) >= joy_autosprint.GetFloat() || FloatMakePositive(joySideMove) >= joy_autosprint.GetFloat() )
	{
		KeyDown( &in_joyspeed, NULL );
	}
	else
	{
		KeyUp( &in_joyspeed, NULL );
	}

	if ( cmd->forwardmove > 0 )
	{
		cmd->buttons |= IN_FORWARD;
	}
	else if ( cmd->forwardmove < 0 )
	{
		cmd->buttons |= IN_BACK;
	}
}

extern short GetTrackerIndex(const char* name);

void WriteTrackerToCmd( CUserCmd *cmd, VRHostTracker* tracker )
{
	if ( tracker == NULL )
		return;

	short index = GetTrackerIndex(tracker->name);

	for (int i = 0; i < cmd->vr_trackers.Count(); i++)
	{
		if (index == cmd->vr_trackers[i].index)
		{
			cmd->vr_trackers[i].ang = tracker->ang;
			cmd->vr_trackers[i].pos = tracker->pos;
			return;
		}
	}

	CmdVRTracker cmdTracker;
	cmdTracker.ang = tracker->ang;
	cmdTracker.pos = tracker->pos;
	cmdTracker.name = tracker->name;
	cmdTracker.index = GetTrackerIndex(tracker->name);
	
	cmd->vr_trackers.AddToTail( cmdTracker );
}


void WriteFingerCurlsToCmd( CUserCmd *cmd, VRSkeletonAction* skeleton, bool rightHand )
{
	if ( skeleton == NULL )
		return;

	for (int i = 0; i < 5; i++)
	{
		if (rightHand)
		{
			cmd->vr_fingerCurlsR[i].x = skeleton->fingerCurls[i].x;
			cmd->vr_fingerCurlsR[i].y = skeleton->fingerCurls[i].y;
		}
		else
		{
			cmd->vr_fingerCurlsL[i].x = skeleton->fingerCurls[i].x;
			cmd->vr_fingerCurlsL[i].y = skeleton->fingerCurls[i].y;
		}
	}
}


// ====================================================================================================
// handle all the vr inputs
// ====================================================================================================
void CVRInput::VRMove( float frametime, CUserCmd *cmd )
{
	if ( !g_VR.active )
		return;

	// TODO: if i can set variables on the client and syncs to the server, then this is really not needed tbh
	cmd->vr_active = 1;

	JoyStickInput( frametime, cmd );

	C_VRBasePlayer *pPlayer = (C_VRBasePlayer*)C_BasePlayer::GetLocalPlayer();

	float viewAngleOffset = 0.0;
	VRVector2Action* turning = (VRVector2Action*)g_VR.GetActionByName( "vector2_smoothturn" );
	if ( turning != NULL )
	{
		viewAngleOffset = frametime * -turning->x * vr_turn_speed.GetFloat();
		pPlayer->AddLocalViewRotation( viewAngleOffset );
	}

	cmd->vr_viewRotation = pPlayer->m_vrViewRotationLocal;

	for (int i = 0; i < g_VR.m_currentTrackers.Count(); i++ )
	{
		WriteTrackerToCmd( cmd, g_VR.m_currentTrackers[i] );
	}

	WriteFingerCurlsToCmd( cmd, g_VR.GetLeftHandSkeleton(), false );
	WriteFingerCurlsToCmd( cmd, g_VR.GetRightHandSkeleton(), true );
}


void CVRInput::CalcButtonBitsBool( const char* action, int in_button )
{
	VRBoolAction* bAction = (VRBoolAction*)g_VR.GetActionByName(action);
	if ( bAction && bAction->value )
	{
		m_bits |= in_button;
	}
}


void CVRInput::CalcButtonBitsToggleCmd( const char* action, bool& bEnabled, const char* cmd )
{
	VRBoolAction* bAction = (VRBoolAction*)g_VR.GetActionByName(action);
	if ( bAction && (bEnabled != bAction->value) )
	{
		if ( !bEnabled )
			engine->ClientCmd(cmd);

		bEnabled = bAction->value;
	}
}


// TODO: handle driving inputs, would need to check the active action set
int CVRInput::GetButtonBits( bool bResetState )
{
	if ( !g_VR.active || !g_VR.m_inMap )
		return BaseClass::GetButtonBits( bResetState );

	int bits = 0;

	if ( vr_allow_keyboard.GetBool() )
		bits = BaseClass::GetButtonBits( bResetState );

	m_bits = bits;

	// VRVector1Action* v1PrimaryFire = (VRVector1Action*)g_VR.GetActionByName("vector1_primaryfire");  // use for driving later?

	CalcButtonBitsBool("boolean_primaryfire", IN_ATTACK);
	CalcButtonBitsBool("boolean_secondaryfire", IN_ATTACK2);
	CalcButtonBitsBool("boolean_sprint", IN_SPEED);
	CalcButtonBitsBool("boolean_jump", IN_JUMP);
	CalcButtonBitsBool("boolean_reload", IN_RELOAD);
	CalcButtonBitsBool("boolean_use", IN_USE);
	CalcButtonBitsBool("boolean_left_pickup", IN_PICKUP_LEFT);
	CalcButtonBitsBool("boolean_right_pickup", IN_PICKUP_RIGHT);

	// TODO: need to setup something better for changing weapon
	CalcButtonBitsToggleCmd("boolean_changeweapon", bInNextWeapon, "invnext");

	// Demez TODO: test in the future when the flashlight works in csgo
	CalcButtonBitsToggleCmd("boolean_flashlight", bFlashlightOn, "impulse 100");

	return m_bits;
}


void CVRInput::VRHeadsetAngles( float frametime )
{
	if ( !g_VR.active && !vr_mouse_look.GetBool() )
		return;

	if ( vr_spew_timings.GetBool() )
		DevMsg( "[VR] CALLED VRINPUT ADJUSTANGLES\n" );

	QAngle viewAngles;
	engine->GetViewAngles( viewAngles );
	
	VRHostTracker* headset = g_VR.GetHeadset();
	C_VRBasePlayer *pPlayer = (C_VRBasePlayer*)C_BasePlayer::GetLocalPlayer();

	if ( headset )
	{
		viewAngles = headset->ang + pPlayer->GetViewRotationAng();
	}
	else
	{
		viewAngles = pPlayer->m_vrViewAngles;
	}

	engine->SetViewAngles( viewAngles );
}

#if ENGINE_NEW
void CVRInput::AdjustAngles( int nSlot, float frametime )
{
	if ( !g_VR.active )
		return BaseClass::AdjustAngles( nSlot, frametime );

	VRHeadsetAngles( frametime );
}
#else
void CVRInput::AdjustAngles( float frametime )
{
	if ( !g_VR.active )
		return BaseClass::AdjustAngles( frametime );

	VRHeadsetAngles( frametime );
}
#endif

