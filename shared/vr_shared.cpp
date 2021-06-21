#include "cbase.h"
#include "vr_shared.h"
#include "vr_tracker.h"
#include "filesystem.h"


VRSystemShared g_VRShared;


VRDeviceType::VRDeviceType()
{
}

VRDeviceType::~VRDeviceType()
{
}


void VRDeviceType::Init( const char* m_path )
{
    // i would like to use CUtlMap, but for some reason that just crashes on the 2nd element added, idk why
    m_trackerModels.SetCount( (int)EVRTracker::COUNT );

    for ( int i = 0; i < m_trackerModels.Count(); i++ )
    {
        m_trackerModels.Element( i ) = "";
    }
}


// soda can time
#define FALLBACK_TRACKER_MODEL "models/props_junk/PopCan01a.mdl"

const char* VRDeviceType::GetTrackerModelName( EVRTracker tracker )
{
    if ( m_trackerModels.Find( GetTrackerName((short)tracker) ) )
    {
        return m_trackerModels.Element( (short)tracker - 1 );
    }

    // no model assigned, so you get a soda can instead
    return FALLBACK_TRACKER_MODEL;
}


// ==============================================================


VRSystemShared::VRSystemShared()
{
}

VRSystemShared::~VRSystemShared()
{
}


bool VRSystemShared::Init()
{
    LoadDeviceTypes();

    return true;
}


void VRSystemShared::LoadDeviceTypes()
{
    m_deviceTypes.PurgeAndDeleteElements();

    FileFindHandle_t fh;
    const char* fn;
    const char* directory = "resource/vr";
    char search[MAX_PATH];
    Q_snprintf( search, sizeof(search), "%s/device_type_*.res", directory );

    for (fn = g_pFullFileSystem->FindFirst( search, &fh ); fn; fn = g_pFullFileSystem->FindNext( fh ) )
    {
        if ( !stricmp( fn, ".") || !stricmp( fn, "..") || g_pFullFileSystem->FindIsDirectory( fh ) )
        {
            continue;
        }

        char fullFileName[MAX_PATH];
        Q_snprintf( fullFileName, sizeof(fullFileName), "%s/%s", directory, fn );

        V_FixSlashes( fullFileName );

        CreateDeviceType( fullFileName );
    }
}


void VRSystemShared::CreateDeviceType( const char* path )
{
    VRDeviceType* deviceType = nullptr;
    // KeyValues *kvDeviceFile = new KeyValues( path );
    KeyValues *kvDeviceFile = new KeyValues( "DeviceType" );

    // what
    if ( !kvDeviceFile )
    {
        Warning("[VR] Failed to create KeyValues instance??\n");
        return;
    }

    kvDeviceFile->LoadFromFile( g_pFullFileSystem, path );

    KeyValues* nameKey = kvDeviceFile->FindKey( "name" );
    if ( !nameKey || V_strcmp(nameKey->GetString(), "") == 0 )
    {
        Warning("[VR] No name assigned to device in file \"%s\"\n", path);
        return;
    }

    deviceType = GetDeviceType( nameKey->GetString() );
    if ( !deviceType )
    {
        deviceType = new VRDeviceType;
        deviceType->Init( path );
    }

    deviceType->m_name = strdup(nameKey->GetString());

    KeyValues* trackersKey = kvDeviceFile->FindKey( "trackers" );
    if ( !trackersKey )
    {
        Warning("[VR] No trackers assigned to device \"%s\" in file \"%s\"\n", deviceType->m_name, path);
        return;
    }

    for ( KeyValues* tracker = trackersKey->GetFirstSubKey(); tracker; tracker = tracker->GetNextKey() )
    {
        const char* pose = tracker->GetName();

        if ( V_strcmp(pose, "") == 0 || GetTrackerEnum(pose) == EVRTracker::INVALID )
        {
            Warning("[VR] Invalid tracker/pose in device \"%s\" in file \"%s\"\n", deviceType->m_name, path);
            continue;
        }

        const char* model = strdup(tracker->GetString());

        deviceType->m_trackerModels.Element( GetTrackerIndex(pose) ) = model; //( GetTrackerIndex(pose), model );
    }

    m_deviceTypes.AddToTail( deviceType );
    kvDeviceFile->deleteThis();
}


// TODO: will have to network device types?
// i really hope i can do it somewhere other than CUserCmd, that would be awful
VRDeviceType* VRSystemShared::GetDeviceType( const char* name )
{
    for ( int i = 0; i < m_deviceTypes.Count(); i++ )
    {
        if ( V_strcmp(name, m_deviceTypes[i]->m_name) == 0 )
        {
            return m_deviceTypes[i];
        }
    }

    return nullptr;
}


