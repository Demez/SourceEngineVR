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

// VMatrix LocalToWorldOld( const Vector& localPos, const QAngle& localAng, const Vector& originPos, const QAngle& originAng );
// VMatrix WorldToLocalOld( const Vector& worldPos, const QAngle& worldAng, const Vector& objectPos, const QAngle& objectAng );

void WorldToLocal( const Vector& worldPos, const QAngle& worldAng, const Vector& objectPos, const QAngle& objectAng, Vector& outPos, QAngle& outAng );

Vector WorldToLocalPos( const matrix3x4_t& worldCoord, const matrix3x4_t& objectCoord );
Vector WorldToLocalPos( const Vector& worldPos, const QAngle& worldAng, const Vector& objectPos, const QAngle& objectAng );
QAngle WorldToLocalAng( const Vector& worldPos, const QAngle& worldAng, const Vector& objectPos, const QAngle& objectAng );

void WorldToLocal( const VMatrix &worldCoord, const VMatrix& objectCoord, Vector& outPos, QAngle& outAng );
void WorldToLocal( const matrix3x4_t& worldCoord, const matrix3x4_t& objectCoord, Vector& outPos, QAngle& outAng );


void LocalToWorld( const Vector& worldPos, const QAngle& worldAng, const Vector& localPos, const QAngle& localAng, Vector& outPos, QAngle& outAng );
void LocalToWorld( const matrix3x4_t& worldCoord, const Vector& localPos, const QAngle& localAng, Vector& outPos, QAngle& outAng );
void LocalToWorld( const matrix3x4_t& worldCoord, const Vector& localPos, const QAngle& localAng, matrix3x4_t& outCoord );



// not sure why ASW doesn't have these

inline void NormalizeAngle( QAngle &angle )
{
	angle.x = AngleNormalize( angle.x );
	angle.y = AngleNormalize( angle.y );
	angle.z = AngleNormalize( angle.z );
}

#if ENGINE_ASW
inline void MatrixScaleByZero( matrix3x4_t &out )
{
	out[0][0] = 0.0f;
	out[1][0] = 0.0f;
	out[2][0] = 0.0f;
	out[0][1] = 0.0f;
	out[1][1] = 0.0f;
	out[2][1] = 0.0f;
	out[0][2] = 0.0f;
	out[1][2] = 0.0f;
	out[2][2] = 0.0f;
}
#endif

void DebugAxis( const Vector &position, const QAngle &angles, float size, bool noDepthTest, float flDuration );

QAngle VR_GetPointAng( const QAngle& ang );
Vector VR_GetPointDir( const QAngle& ang );
Vector VR_GetPointPos( const Vector& pos );

#ifdef CLIENT_DLL
void VR_DrawPointer( const Vector& pointPos, const Vector& pointDir, Vector& prevDir );
void VR_DrawPointer( const Vector& pos, const QAngle& ang, Vector& prevDir );
#endif

