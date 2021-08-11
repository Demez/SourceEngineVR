#include "cbase.h"
#include "vr_util.h"
#include "vr_tracker.h"
#include "hl2_shareddefs.h"  // TODO: move LINK_ENTITY_TO_CLASS_DUMB out of here
#include "vr_gamemovement.h"
#include "debugoverlay_shared.h"
#include "bone_setup.h"

#include "mathlib/vmatrix.h"
#include "mathlib/mathlib.h"

#include "vr_player_shared.h"

#ifdef CLIENT_DLL
#include "model_types.h"
#endif


// LINK_ENTITY_TO_CLASS_DUMB( vr_tracker, CVRTracker );


extern CMoveData* g_pMoveData;


#ifdef CLIENT_DLL
ConVar vr_interp_trackers("vr_interp_trackers", "0.1");
#endif

ConVar vr_tracker_rotation("vr_tracker_rotation", "1", FCVAR_ARCHIVE, "Rotate the tracker models from direct compiling of steamvr device models");


// ==============================================================


const int g_trackerCount = 6;

const char* g_trackerNames[g_trackerCount] = 
{
    "hmd",
    "pose_lefthand",
    "pose_righthand",
    "pose_waist",
    "pose_leftfoot",
    "pose_rightfoot",
};


const char* GetTrackerName(short index)
{
    if (index < 0 || index > g_trackerCount)
    {
        Warning("Invalid tracker index %h", index);
        return "";
    }

    return g_trackerNames[index];
}


short GetTrackerIndex(const char* name)
{
    for (int i = 0; i < g_trackerCount; i++)
    {
        if ( V_strcmp(name, g_trackerNames[i]) == 0 )
        {
            return i;
        }
    }

    Warning("Invalid tracker name %s", name);
    return -1;
}


EVRTracker GetTrackerEnum( const char* name )
{
    return (EVRTracker)(GetTrackerIndex(name) + 1);
}

EVRTracker GetTrackerEnum( short index )
{
    if ( index < 0 || index > (int)EVRTracker::COUNT )
        return EVRTracker::INVALID;

    return (EVRTracker)(index + 1);
}


// ==============================================================




CVRTracker::CVRTracker()
{
    m_pPlayer = NULL;
    m_hmd = NULL;

    m_posOffset.Init();
    m_pos.Init();
    m_ang.Init();
    m_absPos.Init();
    m_absAng.Init();

#ifdef CLIENT_DLL
    m_hStudioHdr = MDLHANDLE_INVALID;
    m_pStudioHdr = NULL;
    m_ModelInstance = MODEL_INSTANCE_INVALID;
#endif
}

CVRTracker::~CVRTracker()
{
    if ( m_bInit && !IsHeadset() )
    {
#ifdef CLIENT_DLL
        if ( m_inRenderList )
            ClientLeafSystem()->RemoveRenderable( RenderHandle() );

        UnlockStudioHdr();
#endif
    }
}


const matrix3x4_t& CVRTracker::GetWorldCoordinate()
{
    AngleMatrix( m_absAng, m_absPos, m_absCoord );
    return m_absCoord;
}


const Vector& CVRTracker::GetAbsOrigin()
{
    return m_absPos;
}


const QAngle& CVRTracker::GetAbsAngles()
{
    return m_absAng;
}


void CVRTracker::SetAbsOrigin( Vector pos )
{
    m_absPos = pos;
}


void CVRTracker::SetAbsAngles( QAngle ang )
{
    m_absAng = ang;
}


void CVRTracker::InitTracker(CmdVRTracker& cmdTracker, CVRBasePlayerShared* pPlayer)
{
    m_pPlayer = pPlayer;

    m_trackerName = cmdTracker.name;
    m_type = GetTrackerEnum(cmdTracker.index);

    m_posOffset = cmdTracker.pos;
    m_pos = cmdTracker.pos;
    m_ang = cmdTracker.ang;

    SetAbsOrigin(pPlayer->GetAbsOrigin() + m_posOffset);
    SetAbsAngles(m_ang);

#ifdef CLIENT_DLL
    // VR: oh boy, this is only used so i can get the model name, i don't have any other way of doing this right now without hackiness
    m_hostTracker = g_VR.GetTracker(m_type);
    if ( m_hostTracker )
        m_modelName = m_hostTracker->GetModelName();
    else
        m_modelName = "";  // yes, this can happen

    SetupBoneName();

    if ( !IsHeadset() )
    {
        LoadModel();

        // Hook it into the rendering system...
/*#if ENGINE_NEW
        ClientLeafSystem()->AddRenderable( this, false, RENDERABLE_IS_OPAQUE, RENDERABLE_MODEL_UNKNOWN_TYPE );  // RENDERABLE_MODEL_STUDIOMDL
#else
        ClientLeafSystem()->AddRenderable( this, RENDER_GROUP_OPAQUE_ENTITY );  // RENDERABLE_MODEL_STUDIOMDL
#endif*/
    }

#endif

    m_bInit = true;
}


void CVRTracker::UpdateTracker(CmdVRTracker& cmdTracker)
{
    if ( m_pPlayer == NULL )
    {
        // tf happened
        Warning("[VR] Tracker: where did the player go??\n");
        return;
    }

    Vector originOffset;
    originOffset.Init();

    if ( IsHeadset() )
    {
        originOffset = m_pos;
    }
    else
    {
        CVRTracker* hmd = GetHeadset();
        if ( hmd )
        {
            originOffset = hmd->m_pos;
        }
    }

    Vector trackerPos = cmdTracker.pos;
    QAngle trackerAng = cmdTracker.ang;

// TODO: maybe get rid of this?
#if 0 // def CLIENT_DLL
    if ( m_pPlayer->IsLocalPlayer() )
    {
        // rip data directly from g_VR (is this even faster?)
        // the only way this would be worth it is if i didn't send tracker updates every frame
        // and only every x frames, which i doubt i would do for this
        VRHostTracker* localTracker = g_VR.GetTrackerByName( m_trackerName );

        // did it get nuked somehow?
        if ( localTracker == NULL )
        {
            Warning("[VR] Local Tracker \"%s\" does not exist, but the entity does?", m_trackerName );
        }
        else
        {
            trackerPos = localTracker->pos;
            trackerAng = localTracker->ang;
        }
    }
#endif

    // float viewRotation = GetVRMoveData()->vr_viewRotation + m_pPlayer->m_vrViewRotation;
    float viewRotation = m_pPlayer->m_vrViewRotation;

    m_pos = trackerPos;

    m_posOffset = trackerPos;
    m_posOffset.x -= originOffset.x;
    m_posOffset.y -= originOffset.y;

    Vector finalPos;
    VectorYawRotate(m_posOffset, viewRotation, finalPos);
    m_posOffset = finalPos;

    m_posOffset.x += m_pPlayer->m_viewOriginOffset.GetX();
    m_posOffset.y += m_pPlayer->m_viewOriginOffset.GetY();

    m_absAng = m_ang = trackerAng;
    m_absAng.y += viewRotation;

    Vector playerOrigin = m_pPlayer->GetAbsOrigin();
    playerOrigin.z += m_pPlayer->VRHeightOffset();

    SetAbsOrigin(playerOrigin + m_posOffset);

#ifdef CLIENT_DLL
    if ( !IsHeadset() )
    {
        NDebugOverlay::Axis( GetAbsOrigin(), GetAbsAngles(), 4, true, 0.0f );
        // NDebugOverlay::Text( GetAbsOrigin(), m_trackerName, true, 0.0f );

        /*// if ( r_drawrenderboxes.GetInt() )
        {
            Vector vecRenderMins, vecRenderMaxs;
            GetRenderBounds( vecRenderMins, vecRenderMaxs );
            debugoverlay->AddBoxOverlay( GetRenderOrigin(), vecRenderMins, vecRenderMaxs,
                                        GetRenderAngles(), 255, 0, 255, 0, 0.01 );
        }*/

        if ( ShouldDraw() )
        {
            if ( m_inRenderList )
            {
                ClientRenderHandle_t handle = RenderHandle();
                if ( handle != INVALID_CLIENT_RENDER_HANDLE )
                {
                    ClientLeafSystem()->RenderableChanged( handle );
                }
            }
            else
            {
                // Hook it into the rendering system...
#if ENGINE_NEW
                ClientLeafSystem()->AddRenderable( this, false, RENDERABLE_IS_OPAQUE, RENDERABLE_MODEL_UNKNOWN_TYPE );
#else
                ClientLeafSystem()->AddRenderable( this, RENDER_GROUP_OPAQUE_ENTITY );
#endif
                m_inRenderList = true;
            }
        }
        else if ( m_inRenderList )
        {
            ClientLeafSystem()->RemoveRenderable( RenderHandle() );
            m_inRenderList = false;
        }
    }
#endif
}


// might not even be that much faster, plus if i ever have trackers be deleted
// then this would be an issue, unless i exclude the headset from being deleted
CVRTracker* CVRTracker::GetHeadset()
{
    if ( !m_hmd && m_pPlayer )
        m_hmd = m_pPlayer->GetHeadset();

    return m_hmd;
}


#ifdef CLIENT_DLL
void CVRTracker::LoadModel()
{
    UnlockStudioHdr();

#ifdef CLIENT_DLL
    // maybe use something else in the future
    m_beamMaterial = materials->FindMaterial( "cable/cable", TEXTURE_GROUP_OTHER );
#endif

    const char* modelName = GetModelName();

    if ( V_strcmp(modelName, "") != 0 )
    {
#ifdef CLIENT_DLL
        m_model = (model_t *)engine->LoadModel( modelName );
#else
        // needs to be precached first though?
        int modelIndex = modelinfo->GetModelIndex( modelName );
        m_model = (model_t *)modelinfo->GetModel( modelIndex );
#endif
    }
    else
    {
        m_model = NULL;
    }

    LockStudioHdr();
}

void CVRTracker::LockStudioHdr()
{
    // AUTO_LOCK( m_StudioHdrInitLock );
    const model_t *mdl = GetModel();
    if (mdl)
    {
        m_hStudioHdr = modelinfo->GetCacheHandle( mdl );
        if ( m_hStudioHdr != MDLHANDLE_INVALID )
        {
            const studiohdr_t *pStudioHdr = mdlcache->LockStudioHdr( m_hStudioHdr );
            CStudioHdr *pStudioHdrContainer = NULL;
            if ( !m_pStudioHdr )
            {
                if ( pStudioHdr )
                {
                    pStudioHdrContainer = new CStudioHdr;
                    pStudioHdrContainer->Init( pStudioHdr, mdlcache );
                }
                else
                {
                    m_hStudioHdr = MDLHANDLE_INVALID;
                }
            }
            else
            {
                pStudioHdrContainer = m_pStudioHdr;
            }

            Assert( ( pStudioHdr == NULL && pStudioHdrContainer == NULL ) || pStudioHdrContainer->GetRenderHdr() == pStudioHdr );

            if ( pStudioHdrContainer && pStudioHdrContainer->GetVirtualModel() )
            {
                MDLHandle_t hVirtualModel = (MDLHandle_t)(int)(pStudioHdrContainer->GetRenderHdr()->virtualModel)&0xffff;
                mdlcache->LockStudioHdr( hVirtualModel );
            }
            m_pStudioHdr = pStudioHdrContainer; // must be last to ensure virtual model correctly set up
        }
    }
}

void CVRTracker::UnlockStudioHdr()
{
    if ( m_pStudioHdr )
    {
        const model_t *mdl = GetModel();
        if (mdl)
        {
            mdlcache->UnlockStudioHdr( m_hStudioHdr );
            if ( m_pStudioHdr->GetVirtualModel() )
            {
                MDLHandle_t hVirtualModel = (MDLHandle_t)(int)m_pStudioHdr->GetRenderHdr()->virtualModel&0xffff;
                mdlcache->UnlockStudioHdr( hVirtualModel );
            }
        }
    }
}


//-----------------------------------------------------------------------------
// Purpose: Get the index of the attachment point with the specified name
//-----------------------------------------------------------------------------
int CVRTracker::LookupAttachment( const char *pAttachmentName )
{
    CStudioHdr *hdr = GetModelPtr();
    if ( !hdr )
    {
        return -1;
    }

    // NOTE: Currently, the network uses 0 to mean "no attachment" 
    // thus the client must add one to the index of the attachment
    // UNDONE: Make the server do this too to be consistent.
    return Studio_FindAttachment( hdr, pAttachmentName ) + 1;
}


void CVRTracker::GetBoneTransform( int iBone, matrix3x4_t &pBoneToWorld )
{
    CStudioHdr *pStudioHdr = GetModelPtr( );

    if (!pStudioHdr)
    {
        Assert(!"CBaseAnimating::GetBoneTransform: model missing");
        return;
    }

    if (iBone < 0 || iBone >= pStudioHdr->numbones())
    {
        Assert(!"CBaseAnimating::GetBoneTransform: invalid bone index");
        return;
    }

    mstudiobone_t* bone = m_pStudioHdr->pBone( iBone );

    if ( !bone )
    {
        MatrixCopy( GetWorldCoordinate(), pBoneToWorld );
        return;
    }

    matrix3x4_t pmatrix = m_pStudioHdr->pBone( iBone )->poseToBone;
    MatrixCopy( pmatrix, pBoneToWorld );
}


//-----------------------------------------------------------------------------
// Purpose: Returns the world location and world angles of an attachment
// Input  : attachment index
// Output :	location and angles
//-----------------------------------------------------------------------------
bool CVRTracker::GetAttachment( int iAttachment, matrix3x4_t &attachmentToWorld )
{
    CStudioHdr *pStudioHdr = GetModelPtr( );
    if (!pStudioHdr)
    {
        MatrixCopy( GetWorldCoordinate(), attachmentToWorld );
        AssertOnce(!"CBaseAnimating::GetAttachment: model missing");
        return false;
    }

    if (iAttachment < 1 || iAttachment > pStudioHdr->GetNumAttachments())
    {
        MatrixCopy( GetWorldCoordinate(), attachmentToWorld );
        //		Assert(!"CBaseAnimating::GetAttachment: invalid attachment index");
        return false;
    }

    const mstudioattachment_t &pattachment = pStudioHdr->pAttachment( iAttachment-1 );
    int iBone = pStudioHdr->GetAttachmentBone( iAttachment-1 );

    matrix3x4_t bonetoworld;
    GetBoneTransform( iBone, bonetoworld );
    if ( (pattachment.flags & ATTACHMENT_FLAG_WORLD_ALIGN) == 0 )
    {
        ConcatTransforms( bonetoworld, pattachment.local, attachmentToWorld ); 
    }
    else
    {
        Vector vecLocalBonePos, vecWorldBonePos;
        MatrixGetColumn( pattachment.local, 3, vecLocalBonePos );
        VectorTransform( vecLocalBonePos, bonetoworld, vecWorldBonePos );

        SetIdentityMatrix( attachmentToWorld );
        MatrixSetColumn( vecWorldBonePos, 3, attachmentToWorld );
    }

    return true;
}


const Vector& CVRTracker::GetRenderOrigin( void )
{
    return GetAbsOrigin();
}

const QAngle& CVRTracker::GetRenderAngles( void )
{
    return GetAbsAngles();
}

const matrix3x4_t &CVRTracker::RenderableToWorldTransform()
{
    return GetWorldCoordinate();
}

float CVRTracker::GetModelScale()
{
    // NOTE: this will only work on the local player unless i send it through usercmd, yay
    return g_VR.GetScale();
}

void CVRTracker::GetRenderBounds( Vector& theMins, Vector& theMaxs )
{
    modelinfo->GetModelRenderBounds( GetModel(), theMins, theMaxs );

    // might use later, idk
#if 0
    CStudioHdr *pStudioHdr = GetModelPtr();
	if ( !pStudioHdr|| !pStudioHdr->SequencesAvailable() /*|| GetSequence() == -1*/ )
	{
		theMins = vec3_origin;
		theMaxs = vec3_origin;
		return;
	} 
	if (!VectorCompare( vec3_origin, pStudioHdr->view_bbmin() ) || !VectorCompare( vec3_origin, pStudioHdr->view_bbmax() ))
	{
		// clipping bounding box
		VectorCopy ( pStudioHdr->view_bbmin(), theMins);
		VectorCopy ( pStudioHdr->view_bbmax(), theMaxs);
	}
	else
	{
		// movement bounding box
		VectorCopy ( pStudioHdr->hull_min(), theMins);
		VectorCopy ( pStudioHdr->hull_max(), theMaxs);
	}

	mstudioseqdesc_t &seqdesc = pStudioHdr->pSeqdesc( -1 );
	VectorMin( seqdesc.bbmin, theMins, theMins );
	VectorMax( seqdesc.bbmax, theMaxs, theMaxs );
#endif

	// Scale this up depending on if our model is currently scaling
	const float flScale = GetModelScale();
	theMaxs *= flScale;
	theMins *= flScale;
}

void CVRTracker::CreateModelInstance()
{
    if ( m_ModelInstance == MODEL_INSTANCE_INVALID )
    {
        m_ModelInstance = modelrender->CreateInstance( this );
    }
}

void CVRTracker::DestroyModelInstance()
{
    if (m_ModelInstance != MODEL_INSTANCE_INVALID)
    {
        modelrender->DestroyInstance( m_ModelInstance );
        m_ModelInstance = MODEL_INSTANCE_INVALID;
    }
}

#if ENGINE_NEW && VR_TRACKER_FAST_PATH
//----------------------------------------------------------------------------
// Hooks into the fast path render system
//----------------------------------------------------------------------------
bool CVRTracker::GetRenderData( void *pData, ModelDataCategory_t nCategory )
{
    switch ( nCategory )
    {
        case MODEL_DATA_LIGHTING_MODEL:
            // Necessary for lighting blending
            CreateModelInstance();
            *(RenderableLightingModel_t*)pData = LIGHTING_MODEL_STANDARD;
            return true;

        default:
            return false;
    }
}
#endif

ConVar vr_draw_trackers_all_players("vr_draw_trackers_all_players", "1", FCVAR_CLIENTDLL);

bool CVRTracker::ShouldDraw() 
{
    // should never draw if not the local player, at least for now
    if ( !m_pPlayer->IsLocalPlayer() && !vr_draw_trackers_all_players.GetBool() )
        return false;

    return g_VRRenderer.ShouldDrawTracker( m_type );
}


// just here so there's no extra entity check
void CVRTracker::GetRenderBoundsWorldspace( Vector &absMins, Vector &absMaxs )
{
    Vector mins, maxs;
    GetRenderBounds( mins, maxs );

    // FIXME: Should I just use a sphere here?
    // Another option is to pass the OBB down the tree; makes for a better fit
    // Generate a world-aligned AABB
    const QAngle& angles = GetRenderAngles();
    if (angles == vec3_angle)
    {
        const Vector& origin = GetRenderOrigin();
        VectorAdd( mins, origin, absMins );
        VectorAdd( maxs, origin, absMaxs );
    }
    else
    {
        TransformAABB( RenderableToWorldTransform(), mins, maxs, absMins, absMaxs );
    }
    Assert( absMins.IsValid() && absMaxs.IsValid() );
}


bool CVRTracker::SetupBones( matrix3x4a_t *pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime )
{
    if (!m_model)
        return false;

    // Setup our transform.
    matrix3x4a_t parentTransform;
    const QAngle &vRenderAngles = GetRenderAngles();
    const Vector &vRenderOrigin = GetRenderOrigin();
    AngleMatrix( vRenderAngles, parentTransform );
    parentTransform[0][3] = vRenderOrigin.x;
    parentTransform[1][3] = vRenderOrigin.y;
    parentTransform[2][3] = vRenderOrigin.z;

    CStudioHdr *pStudioHdr = GetModelPtr();
    for (int i = 0; i < pStudioHdr->numbones(); i++) 
    {
        MatrixCopy( parentTransform, pBoneToWorldOut[i] );

        if (pStudioHdr->boneParent(i) == -1) 
        {
            ApplyBoneMatrixTransform( parentTransform );
        }
    }

    return true;
}

void CVRTracker::ApplyBoneMatrixTransform( matrix3x4_t& transform )
{
    if ( vr_tracker_rotation.GetBool() )
    {
        QAngle trackerAng;
        Vector trackerPos;
        MatrixAngles( transform, trackerAng, trackerPos );

        QAngle outAngles;
        VMatrix rotateMat, outMat;

        // can this be done in one rotation matrix?
        MatrixBuildRotationAboutAxis( rotateMat, Vector(0, 1, 0), 270 );
        MatrixMultiply( transform, rotateMat, outMat );
        MatrixToAngles( outMat, outAngles );

        AngleMatrix( outAngles, trackerPos, transform );

        MatrixBuildRotationAboutAxis( rotateMat, Vector(0, 0, 1), 270 );
        MatrixMultiply( transform, rotateMat, outMat );
        MatrixToAngles( outMat, outAngles );

        AngleMatrix( outAngles, trackerPos, transform );
    }

    float scale = GetModelScale();
    if ( scale > 1.0f+FLT_EPSILON || scale < 1.0f-FLT_EPSILON )
    {
        // The bone transform is in worldspace, so to scale this, we need to translate it back
        Vector pos;
        MatrixGetColumn( transform, 3, pos );
        pos -= GetRenderOrigin();
        pos *= scale;
        pos += GetRenderOrigin();
        MatrixSetColumn( pos, 3, transform );

        VectorScale( transform[0], scale, transform[0] );
        VectorScale( transform[1], scale, transform[1] );
        VectorScale( transform[2], scale, transform[2] );
    }
}


int CVRTracker::DrawModel( int flags RENDER_INSTANCE_INPUT )
{
    // VR TODO: sometimes there's a bug where if i breakpoint too long or something,
    // a tracker can be lost from the player tracker list, but still exist
    // so then you just see a floating tracker just drawing here
    // not really sure if this test will help
    // might be a better idea to have a global tracker list in VRSystemShared so we can see this on the server too
#if 0

    bool foundSelf = false;
    for ( int i = 0; i < m_pPlayer->m_VRTrackers.Count(); i++ )
    {
        if ( m_pPlayer->m_VRTrackers[i] == this )
            foundSelf = true;

        if ( foundSelf )
            break;
    }

    if ( !foundSelf )
    {
        Warning("[VR] Detached tracker?\n");
        // remove self?
    }

#endif

    if ( !ShouldDraw() )
        return 0;

    if ( !GetModel() )
        return 0;

    // Make sure hdr is valid for drawing
    if ( !GetModelPtr() )
        return 0;

    CreateModelInstance();

    ClientModelRenderInfo_t info;
    ClientModelRenderInfo_t *pInfo;

    pInfo = &info;
    studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( GetModel() );
    if (!pStudioHdr)
        return false;

    pInfo->flags = flags;
    pInfo->pRenderable = this;
    pInfo->instance = GetModelInstance();
    pInfo->entity_index = m_pPlayer->index;
    pInfo->pModel = GetModel();
    pInfo->origin = GetRenderOrigin();
    pInfo->angles = GetRenderAngles();
    pInfo->skin = GetSkin();
    pInfo->body = 0;
    pInfo->hitboxset = 0;

    bool bMarkAsDrawn = false;

    // if ( OnInternalDrawModel( pInfo ) )
    {
        pInfo->pModelToWorld = &pInfo->modelToWorld;

        // Turns the origin + angles into a matrix
        AngleMatrix( pInfo->angles, pInfo->origin, pInfo->modelToWorld );

        // Suppress unlocking
        //CMatRenderDataReference rd( pRenderContext );
        DrawModelState_t state;
        matrix3x4_t *pBoneToWorld;
#if ENGINE_NEW
        bMarkAsDrawn = modelrender->DrawModelSetup( *pInfo, &state, &pBoneToWorld );
#else
        bMarkAsDrawn = modelrender->DrawModelSetup( *pInfo, &state, NULL, &pBoneToWorld );
#endif

        // Scale the base transform if we don't have a bone hierarchy
        if ( GetModelScale() > 1.0f+FLT_EPSILON || GetModelScale() < 1.0f-FLT_EPSILON )
        {
            if ( m_pStudioHdr && pBoneToWorld && m_pStudioHdr->numbones() == 1 )
            {
                // Scale the bone to world at this point
                float flScale = GetModelScale();
                VectorScale( (*pBoneToWorld)[0], flScale, (*pBoneToWorld)[0] );
                VectorScale( (*pBoneToWorld)[1], flScale, (*pBoneToWorld)[1] );
                VectorScale( (*pBoneToWorld)[2], flScale, (*pBoneToWorld)[2] );
            }
        }

        DrawModelState_t *pState = ( bMarkAsDrawn && ( pInfo->flags & STUDIO_RENDER ) ) ? &state : NULL; 
        if (pState)
        {
            modelrender->DrawModelExecute( *pState, *pInfo, pBoneToWorld );
        }
    }

    return bMarkAsDrawn;
}


void CVRTracker::SetupBoneName()
{
    m_boneName = "";
    m_boneRootName = "";

    C_VRBasePlayer* ply = (C_VRBasePlayer*)m_pPlayer;

    if ( m_type == EVRTracker::HMD )
    {
        m_boneName = ply->GetBoneName( EVRBone::Head );
        m_boneRootName = ply->GetBoneName( EVRBone::Spine ); // idk
    }
    else if ( m_type == EVRTracker::LHAND )
    {
        m_boneName = ply->GetBoneName( EVRBone::LHand );
        m_boneRootName = ply->GetBoneName( EVRBone::LShoulder );
    }
    else if ( m_type == EVRTracker::RHAND )
    {
        m_boneName = ply->GetBoneName( EVRBone::RHand );
        m_boneRootName = ply->GetBoneName( EVRBone::RShoulder );
    }
    else if ( m_type == EVRTracker::HIP )
    {
        m_boneName = ply->GetBoneName( EVRBone::Pelvis );
        m_boneRootName = ply->GetBoneName( EVRBone::Pelvis );
    }
    else if ( m_type == EVRTracker::LFOOT )
    {
        m_boneName = ply->GetBoneName( EVRBone::LFoot );
        m_boneRootName = ply->GetBoneName( EVRBone::LThigh );
    }
    else if ( m_type == EVRTracker::RFOOT )
    {
        m_boneName = ply->GetBoneName( EVRBone::RFoot );
        m_boneRootName = ply->GetBoneName( EVRBone::RThigh );
    }
}
#endif

