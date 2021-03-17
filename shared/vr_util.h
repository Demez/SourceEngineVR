#pragma once

#include <mathlib/vector.h>
#include <mathlib/vmatrix.h>


#define	IN_PICKUP_LEFT		(1 << 26)
#define	IN_PICKUP_RIGHT		(1 << 27)


void ComposeProjectionTransform(float fLeft, float fRight, float fTop, float fBottom, float zNear, float zFar, float fovScale, VMatrix *pmProj );
VMatrix OpenVRToSourceCoordinateSystem(const VMatrix& vortex, bool scale = true);
VMatrix VMatrixFrom44(const float v[4][4]);
VMatrix VMatrixFrom34(const float v[3][4]);
VMatrix VMatrixFrom33(const float v[3][3]);

VMatrix LocalToWorld( const Vector& localPos, const QAngle& localAng, const Vector& originPos, const QAngle& originAng );
VMatrix WorldToLocal( const Vector& worldPos, const QAngle& worldAng, const Vector& objectPos, const QAngle& objectAng );


// not sure why ASW doesn't have these

inline void NormalizeAngle( QAngle &angle )
{
	angle.x = AngleNormalize( angle.x );
	angle.y = AngleNormalize( angle.y );
	angle.z = AngleNormalize( angle.z );
}
