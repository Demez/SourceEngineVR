#include "cbase.h"
#include "vr_util.h"


ConVar vr_scale("vr_scale", "1");


// ----------------------------------------------------------------------
// Purpose: Create a 4x4 projection transform from eye projection and distortion parameters
// ----------------------------------------------------------------------
// was inline static
void ComposeProjectionTransform(float fLeft, float fRight, float fTop, float fBottom, float zNear, float zFar, float fovScale, VMatrix *pmProj )
{
	if( fovScale != 1.0f  && fovScale > 0.f )
	{
		float fFovScaleAdjusted = tan( atan( fTop  ) / fovScale ) / fTop;
		fRight *= fFovScaleAdjusted;
		fLeft *= fFovScaleAdjusted;
		fTop *= fFovScaleAdjusted;
		fBottom *= fFovScaleAdjusted;
	}

	float idx = 1.0f / (fRight - fLeft);
	float idy = 1.0f / (fBottom - fTop);
	float idz = 1.0f / (zFar - zNear);
	float sx =  fRight + fLeft;
	float sy =  fBottom + fTop;

	float (*p)[4] = pmProj->m;
	p[0][0] = 2*idx; p[0][1] = 0;     p[0][2] = sx*idx;    p[0][3] = 0;
	p[1][0] = 0;     p[1][1] = 2*idy; p[1][2] = sy*idy;    p[1][3] = 0;
	p[2][0] = 0;     p[2][1] = 0;     p[2][2] = -zFar*idz; p[2][3] = -zFar*zNear*idz;
	p[3][0] = 0;     p[3][1] = 0;     p[3][2] = -1.0f;     p[3][3] = 0;
}


VMatrix OpenVRToSourceCoordinateSystem(const VMatrix& vortex, bool scale)
{
	// const float inchesPerMeter = (float)(39.3700787);

	float inchesPerMeter = (float)(39.3700787);

	if (scale)
		inchesPerMeter *= vr_scale.GetFloat();

	// From Vortex: X=right, Y=up, Z=backwards, scale is meters.
	// To Source: X=forwards, Y=left, Z=up, scale is inches.
	//
	// s_from_v = [ 0 0 -1 0
	//             -1 0 0 0
	//              0 1 0 0
	//              0 0 0 1];
	//
	// We want to compute vmatrix = s_from_v * vortex * v_from_s; v_from_s = s_from_v'
	// Given vortex =
	// [00    01    02    03
	//  10    11    12    13
	//  20    21    22    23
	//  30    31    32    33]
	//
	// s_from_v * vortex * s_from_v' =
	//  22    20   -21   -23
	//  02    00   -01   -03
	// -12   -10    11    13
	// -32   -30    31    33
	//
	const vec_t (*v)[4] = vortex.m;
	VMatrix result(
		v[2][2],  v[2][0], -v[2][1], -v[2][3] * inchesPerMeter,
		v[0][2],  v[0][0], -v[0][1], -v[0][3] * inchesPerMeter,
		-v[1][2], -v[1][0],  v[1][1],  v[1][3] * inchesPerMeter,
		-v[3][2], -v[3][0],  v[3][1],  v[3][3]);

	return result;
}

VMatrix VMatrixFrom44(const float v[4][4])
{
	return VMatrix(
		v[0][0], v[0][1], v[0][2], v[0][3],
		v[1][0], v[1][1], v[1][2], v[1][3],
		v[2][0], v[2][1], v[2][2], v[2][3],
		v[3][0], v[3][1], v[3][2], v[3][3]);
}

VMatrix VMatrixFrom34(const float v[3][4])
{
	return VMatrix(
		v[0][0], v[0][1], v[0][2], v[0][3],
		v[1][0], v[1][1], v[1][2], v[1][3],
		v[2][0], v[2][1], v[2][2], v[2][3],
		0,       0,       0,       1       );
}

VMatrix VMatrixFrom33(const float v[3][3])
{
	return VMatrix(
		v[0][0], v[0][1], v[0][2], 0,
		v[1][0], v[1][1], v[1][2], 0,
		v[2][0], v[2][1], v[2][2], 0,
		0,       0,       0,       1);
}




VMatrix LocalToWorld( const Vector& localPos, const QAngle& localAng, const Vector& originPos, const QAngle& originAng )
{
	VMatrix localTransform;
	localTransform.SetupMatrixOrgAngles( localPos, localAng );
	// localTransform.SetupMatrixOrgAngles( vec3_origin, localAng );
	// localTransform.SetTranslation( localPos );

	VMatrix worldTransform;
	worldTransform.SetupMatrixOrgAngles( originPos, originAng );
	// worldTransform.SetupMatrixOrgAngles( vec3_origin, originAng );
	// worldTransform.SetTranslation( originPos );

	VMatrix localToWorld = worldTransform * localTransform;
	return localToWorld;
}


VMatrix WorldToLocal( const Vector& worldPos, const QAngle& worldAng, const Vector& objectPos, const QAngle& objectAng )
{
	VMatrix worldTransform;
	worldTransform.SetupMatrixOrgAngles( worldPos, worldAng );
	// worldTransform.SetupMatrixOrgAngles( vec3_origin, worldAng );
	// worldTransform.SetTranslation( worldPos );

	VMatrix objectTranform;
	objectTranform.SetupMatrixOrgAngles( objectPos, objectAng );
	// objectTranform.SetupMatrixOrgAngles( vec3_origin, objectAng );
	// objectTranform.SetTranslation( objectPos );

	VMatrix localToWorld = worldTransform.InverseTR() * objectTranform;
	return localToWorld;
}

