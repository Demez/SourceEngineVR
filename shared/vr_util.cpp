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

	// 10.0f/0.254f;
	// 39.37012415030996
	// float inchesPerMeter = (float)(39.3700787);
	double inchesPerMeter = (double)(39.37012415030996);

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


VMatrix WorldToLocalOld( const Vector& worldPos, const QAngle& worldAng, const Vector& objectPos, const QAngle& objectAng )
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


Vector WorldToLocalPos( const Vector& worldPos, const QAngle& worldAng, const Vector& objectPos, const QAngle& objectAng )
{
	VMatrix worldMatrix, childMatrix;
	worldMatrix.SetupMatrixOrgAngles( worldPos, worldAng );
	childMatrix.SetupMatrixOrgAngles( objectPos, objectAng );

	// WorldToLocal
	return worldMatrix.VMul4x3Transpose( objectPos );
}

Vector WorldToLocalPos( const matrix3x4_t& worldCoord, const matrix3x4_t& objectCoord )
{
	Vector objectPos;
	MatrixGetColumn( objectCoord, 3, objectPos );

	// WorldToLocal
	VMatrix worldCoordMat(worldCoord);
	return worldCoordMat.VMul4x3Transpose( objectPos );
}


QAngle WorldToLocalAng( const Vector& worldPos, const QAngle& worldAng, const Vector& objectPos, const QAngle& objectAng )
{
	VMatrix worldMatrix, childMatrix;
	worldMatrix.SetupMatrixOrgAngles( worldPos, worldAng );
	childMatrix.SetupMatrixOrgAngles( objectPos, objectAng );

	// I have the axes of local space in world space. (childMatrix)
	// I want to compute those world space axes in the parent's local space
	// and set that transform (as angles) on the child's object so the net
	// result is that the child is now in parent space, but still oriented the same way
	VMatrix tmp = worldMatrix.Transpose(); // world->parent
	tmp.MatrixMul( childMatrix, worldMatrix ); // child->parent
	QAngle angles;
	MatrixToAngles( worldMatrix, angles );

	return angles;
}



void WorldToLocal( const Vector& worldPos, const QAngle& worldAng, const Vector& objectPos, const QAngle& objectAng, Vector& outPos, QAngle& outAng )
{
	VMatrix worldMatrix, childMatrix;
	worldMatrix.SetupMatrixOrgAngles( worldPos, worldAng );
	childMatrix.SetupMatrixOrgAngles( objectPos, objectAng );

	// WorldToLocal
	outPos = worldMatrix.VMul4x3Transpose( objectPos );

	// I have the axes of local space in world space. (childMatrix)
	// I want to compute those world space axes in the parent's local space
	// and set that transform (as angles) on the child's object so the net
	// result is that the child is now in parent space, but still oriented the same way
	VMatrix tmp = worldMatrix.Transpose(); // world->parent
	tmp.MatrixMul( childMatrix, worldMatrix ); // child->parent
	MatrixToAngles( worldMatrix, outAng );
}


void WorldToLocal( const VMatrix &worldCoord, const VMatrix& objectCoord, Vector& outPos, QAngle& outAng )
{
	Vector objectPos;
	MatrixGetColumn( objectCoord.As3x4(), 3, objectPos );

	// WorldToLocal
	outPos = worldCoord.VMul4x3Transpose( objectPos );

	// I have the axes of local space in world space. (childMatrix)
	// I want to compute those world space axes in the parent's local space
	// and set that transform (as angles) on the child's object so the net
	// result is that the child is now in parent space, but still oriented the same way
	VMatrix tmp = worldCoord.Transpose(); // world->parent
	VMatrix worldCoordCpy(worldCoord);
	tmp.MatrixMul( objectCoord, worldCoordCpy ); // child->parent
	MatrixToAngles( worldCoordCpy, outAng );
}


// change to GetRelCoord()?
void WorldToLocal( const matrix3x4_t& worldCoord, const matrix3x4_t& objectCoord, Vector& outPos, QAngle& outAng )
{
	WorldToLocal( VMatrix(worldCoord), VMatrix(objectCoord), outPos, outAng );
}



void LocalToWorld( const Vector& worldPos, const QAngle& worldAng, const Vector& localPos, const QAngle& localAng, Vector& outPos, QAngle& outAng )
{
	matrix3x4_t matWorldCoord;
	AngleMatrix( worldAng, matWorldCoord );
	MatrixSetColumn( worldPos, 3, matWorldCoord );

	LocalToWorld( matWorldCoord, localPos, localAng, outPos, outAng );
}


void LocalToWorld( const matrix3x4_t& matWorldCoord, const Vector& localPos, const QAngle& localAng, Vector& outPos, QAngle& outAng )
{
	matrix3x4_t matLocalCoord;
	LocalToWorld( matWorldCoord, localPos, localAng, matLocalCoord );

	// pull our absolute position and angles out of the matrix
	MatrixGetColumn( matLocalCoord, 3, outPos ); 
	MatrixAngles( matLocalCoord, outAng );
}


void LocalToWorld( const matrix3x4_t& matWorldCoord, const Vector& localPos, const QAngle& localAng, matrix3x4_t& outCoord )
{
	matrix3x4_t matEntityToParent;

	AngleMatrix( localAng, matEntityToParent );
	MatrixSetColumn( localPos, 3, matEntityToParent );

	// slap the saved local entity origin and angles on top of the hands origin and angles
	ConcatTransforms( matWorldCoord, matEntityToParent, outCoord );
}


void DebugAxis( const Vector &position, const QAngle &angles, float size, bool noDepthTest, float flDuration )
{
#if 0
	Vector xvec, yvec, zvec;
	AngleVectors( angles, &xvec, &yvec, &zvec );

	xvec = position + (size * xvec);
	yvec = position - (size * yvec); // Left is positive
	zvec = position + (size * zvec);

	debugoverlay->AddLineOverlay( position, xvec, 255, 0, 0, noDepthTest, flDuration );
	debugoverlay->AddLineOverlay( position, yvec, 0, 255, 0, noDepthTest, flDuration );
	debugoverlay->AddLineOverlay( position, zvec, 0, 0, 255, noDepthTest, flDuration );
#endif
}


// DEMEZ TODO: should there be an input for the type of controller?
// or should it check openvr for it if im only going to use this on the local client?
QAngle VR_GetPointAng( const QAngle& ang )
{
	QAngle angles = ang;

	VMatrix mat, rotateMat, outMat;
	MatrixFromAngles( angles, mat );
	MatrixBuildRotationAboutAxis( rotateMat, Vector( 0, 1, 0 ), 35 );
	MatrixMultiply( mat, rotateMat, outMat );
	MatrixToAngles( outMat, angles );

	return angles;
}

Vector VR_GetPointDir( const QAngle& ang )
{
	return vec3_origin;
}

Vector VR_GetPointPos( const Vector& pos )
{
	return vec3_origin;
}


#ifdef CLIENT_DLL
extern void DrawPointerQuadratic( const Vector &start, const Vector &control, const Vector &end, float width, const Vector &color, float scrollOffset, float flHDRColorScale );

void VR_DrawPointer( const Vector& pointPos, const Vector& pointDir, Vector& prevDir )
{
	if ( prevDir.IsZero() )
		prevDir = pointDir;

	Vector lerpedPointDir(pointDir * 32);

	lerpedPointDir = Lerp( 0.2, prevDir, lerpedPointDir );

	prevDir = lerpedPointDir;

	DrawPointerQuadratic( pointPos, pointPos + (pointDir * 4), pointPos + lerpedPointDir, 5.0f, Vector(203, 66, 245), 0.5f, 1.0f );
	// DrawBeamQuadratic( pointPos, pointPos + (pointDir * 4), pointPos + lerpedPointDir, 1.0f, Vector(203, 66, 245), 0.5f, 1.0f );
}

void VR_DrawPointer( const Vector& pos, const QAngle& ang, Vector& prevDir )
{
	VR_DrawPointer( VR_GetPointPos(pos), VR_GetPointDir(ang), prevDir );
}
#endif

