// Copyright © 2015 CCP ehf.

#include "StdAfx.h"
#include "BlueSysInfo.h"
#include "pdm.h"
#include "pdm/protobuf.h"
#include "BlueMemStream.h"
#ifdef _WIN32
#include "win32.h"
#include <ShlObj.h>
#include <intrin.h>
#endif
#if defined(__linux__)
#include <cpuid.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#endif
#include <sstream>

static CcpLogChannel_t s_ch = CCP_LOG_DEFINE_CHANNEL( "BlueSysInfo" );

BlueSysInfoTaskTimesPtr BlueSysInfo::GetProcessTimes() const
{
	BlueSysInfoTaskTimesPtr times;
	times.CreateInstance();

	int64_t kernelTime, userTime;
	if( !CcpGetProcessTimes( kernelTime, userTime ) )
	{
		PyErr_SetString( PyExc_OSError, "Failed to get process times" );
		return nullptr;
	}
	times->m_systemTime = double( kernelTime ) / 10000000.0;
	times->m_userTime = double( userTime ) / 10000000.0;
	return times;
}

BlueSysInfoTaskTimesPtr BlueSysInfo::GetThreadTimes() const
{
	BlueSysInfoTaskTimesPtr times;
	times.CreateInstance();

	int64_t kernelTime, userTime;
	if( !CcpGetThreadTimes( kernelTime, userTime ) )
	{
		PyErr_SetString( PyExc_OSError, "Failed to get thread times" );
		return nullptr;
	}
	times->m_systemTime = double( kernelTime ) / 10000000.0;
	times->m_userTime = double( userTime ) / 10000000.0;
	return times;
}

BlueSysInfoMemoryPtr BlueSysInfo::GetMemory() const
{
	BlueSysInfoMemoryPtr memory;
	memory.CreateInstance();
	return memory;
}

bool BlueSysInfo::IsRosetta() const
{
    return PDM::IsRosetta();
}

bool BlueSysInfo::IsWine() const
{
	return PDM::IsWine();
}

std::wstring BlueSysInfo::GetWineVersion() const
{
	return UTF8ToWide( PDM::GetWineVersion() );
}

std::wstring BlueSysInfo::GetWineHostOs() const
{
	return UTF8ToWide( PDM::GetWineHostOs() );
}

std::vector<BlueSysInfoNetworkAdapterPtr> BlueSysInfo::GetNetworkAdapters() const
{
	std::vector<BlueSysInfoNetworkAdapterPtr> adapters;

	for( auto& adapter : PDM::GetNetworkAdapterInfo() )
	{
		BlueSysInfoNetworkAdapterPtr ptr;
		ptr.CreateInstance();
		ptr->m_name = UTF8ToWide( adapter.name );
		const auto& mac = adapter.macAddress;
		auto macAddressByteString = std::string( mac.begin(), mac.end() );
		ptr->m_macAddress = PyBytes_FromStringAndSize( macAddressByteString.c_str(), macAddressByteString.size() );
		ptr->m_macAddressString = adapter.macAddressString;
		ptr->m_uuid = adapter.uuidString;

		adapters.push_back( ptr );
	}

	return adapters;
}

std::wstring BlueSysInfo::GetPDMData() const
{
	return UTF8ToWide( PDM::PDMDataToFormattedString( PDM::RetrievePDMData( "EVE", "" ) ) );
}

#ifdef _WIN32

std::wstring BlueSysInfo::GetUserDocumentsDirectory() const
{
    wchar_t path[MAX_PATH];
    if( SUCCEEDED( SHGetFolderPathW( NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, path ) ) )
    {
        return path;
    }
    return L"";
}

std::wstring BlueSysInfo::GetSharedApplicationDataDirectory() const
{
    wchar_t path[MAX_PATH];
    if( SUCCEEDED( SHGetFolderPathW( NULL, CSIDL_COMMON_APPDATA, NULL, SHGFP_TYPE_CURRENT, path ) ) )
    {
        return path;
    }
    return L"";
}

std::wstring BlueSysInfo::GetUserApplicationDataDirectory() const
{
    wchar_t path[MAX_PATH];
    if( SUCCEEDED( SHGetFolderPathW( NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, path ) ) )
    {
        return path;
    }
    return L"";
}

std::wstring BlueSysInfo::GetSharedFontsDirectory() const
{
    wchar_t path[MAX_PATH];
	if( SUCCEEDED( SHGetFolderPathW( NULL, CSIDL_FONTS, NULL, SHGFP_TYPE_CURRENT, path ) ) )
    {
        return path;
    }
    return L"";
}

std::wstring BlueSysInfo::GetSystemFontsDirectory() const
{
	// Windows only has one fonts directory
	return GetSharedFontsDirectory();
}

uint32_t BlueSysInfo::GetProcessBitCount() const
{
	return CcpGetProcessBitCount();
}

uint32_t BlueSysInfo::GetSystemBitCount() const
{
#ifdef _WIN64
	return 64;
#else
	BOOL isWow = FALSE;
	IsWow64Process( GetCurrentProcess(), &isWow );
	return isWow ? 64 : 32;
#endif
}

uint64_t BlueSysInfo::GetProcessStartTime() const
{
	FILETIME creationTime, exitTime, kernelTime, userTime;
	if( ::GetProcessTimes( GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime ) )
	{
		ULARGE_INTEGER lg;
		lg.HighPart = creationTime.dwHighDateTime;
		lg.LowPart = creationTime.dwLowDateTime;
		return lg.QuadPart;
	}
	else
	{
		return 0;
	}
}

std::string BlueSysInfo::GetMachineUuid() const
{
	REGSAM access = KEY_READ;
#ifndef _WIN64
	if( GetSystemBitCount() != 32 )
	{
		access |= KEY_WOW64_64KEY;
	}
#endif
	HKEY key;
	if( RegOpenKeyEx( HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", 0, access, &key ) != ERROR_SUCCESS )
	{
		return "";
	}
	ON_BLOCK_EXIT( [&] { RegCloseKey( key ); } );

	DWORD type;
	char guid[256];
	DWORD size = DWORD( sizeof( guid ) );
	if( RegQueryValueEx( key, "MachineGuid", nullptr, &type, reinterpret_cast<LPBYTE>( guid ), &size ) != ERROR_SUCCESS )
	{
		return "";
	}
	if( type != REG_SZ )
	{
		return "";
	}
	return guid;
}

std::wstring BlueSysInfo::GetMachineName() const
{
	wchar_t buffer[MAX_COMPUTERNAME_LENGTH + 1];
	buffer[0] = 0;
	DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
	GetComputerNameW( buffer, &size );
	return buffer;
}

std::wstring BlueSysInfo::GetDomainName() const
{
	DWORD size = 0;
	GetComputerNameExW( ComputerNamePhysicalDnsDomain, nullptr, &size );

	std::unique_ptr<wchar_t[]> buffer( new wchar_t[size] );
	if( !GetComputerNameExW( ComputerNamePhysicalDnsDomain, buffer.get(), &size ) )
	{
		return {};
	}

	wchar_t name[MAX_COMPUTERNAME_LENGTH + 1];
	name[0] = 0;
	size = MAX_COMPUTERNAME_LENGTH + 1;
	DnsHostnameToComputerNameW( buffer.get(), name, &size );
	return name;
}



BlueSysInfoCpu::BlueSysInfoCpu() :
	m_extensions( PDM::GetCPUInfo().extensions )
{
	auto pdmCpu = PDM::GetCPUInfo();
	m_mHz = pdmCpu.frequency;
	SYSTEM_INFO info;
	GetNativeSystemInfo( &info );

	m_family = info.wProcessorLevel;
	m_revision = info.wProcessorRevision;
	m_logicalCpuCount = info.dwNumberOfProcessors;

	int cpuInfo[4] = { 0 };
	__cpuid( cpuInfo, 0x80000000 );
	int idMax = cpuInfo[0];
	if ( idMax >= 0x80000001 )
	{
		__cpuidex( cpuInfo, 0x80000001, 0 );
		m_bitCount = cpuInfo[3] & ( 1 << 29 ) ? 64 : 32;
	}
	else
	{
		m_bitCount = 32;
	}

	char brand[0x40] = { 0 };

	if ( idMax >= 0x80000004 )
	{
		__cpuidex( reinterpret_cast<int*>( brand ), 0x80000002, 0 );
		__cpuidex( reinterpret_cast<int*>( brand + 16 ), 0x80000003, 0 );
		__cpuidex( reinterpret_cast<int*>( brand + 32 ), 0x80000004, 0 );
	}
	m_brand = brand;

	char vendor[0x20] = { 0 };
	__cpuidex( cpuInfo, 0, 0 );
	*reinterpret_cast<int*>(vendor) = cpuInfo[1];
    *reinterpret_cast<int*>(vendor + 4) = cpuInfo[3];
    *reinterpret_cast<int*>(vendor + 8) = cpuInfo[2];

	int model = 0;
	int stepping = 0;

	__cpuid( cpuInfo, 0 );
	idMax = cpuInfo[0];
	if( idMax > 0 )
	{
		__cpuidex( cpuInfo, 1, 0 );
		model = ( ( cpuInfo[0] >> 4 ) & 0xf ) | ( ( cpuInfo[0] >> 12 ) & 0xf0 );
		stepping = cpuInfo[0] & 0xf;
	}


	const char* platform;
	switch( info.wProcessorArchitecture )
	{
	case PROCESSOR_ARCHITECTURE_AMD64:
		m_architecture = "AMD64";
		platform = strstr( vendor, "Intel" ) ? "Intel64" : "AMD64";
		break;
	case PROCESSOR_ARCHITECTURE_ARM:
		m_architecture = "ARM";
		platform = "ARM";
		break;
	case PROCESSOR_ARCHITECTURE_IA64:
		m_architecture = "IA64";
		platform = "IA64";
		break;
	case PROCESSOR_ARCHITECTURE_INTEL:
		m_architecture = "x86";
		platform = "x86";
		break;
	case PROCESSOR_ARCHITECTURE_ARM64:
		m_architecture = "ARM64";
		platform = "ARM64";
		break;
	default:
		platform = "Unknown";
	}
	char identifier[128];
	sprintf_s( identifier, "%s Family %i Model %i Stepping %i, %s", platform, m_family, model, stepping, vendor );
	m_identifier = identifier;
}


BlueSysInfoOs::BlueSysInfoOs()
{
	m_platform = WINDOWS;

	OSVERSIONINFOEX info = { 0 };
	info.dwOSVersionInfoSize = sizeof( info );
	GetWindowsVersion( info );

	m_majorVersion = info.dwMajorVersion;
	m_minorVersion = info.dwMinorVersion;
	m_buildNumber = info.dwBuildNumber;
	m_patch = info.szCSDVersion;
	if( info.wProductType > 1 )
	{
		m_suite = SERVER;
	}
	else if( ( info.wSuiteMask & 0x00000200 ) != 0 )
	{
		m_suite = DESKTOP;
	}
	else
	{
		m_suite = WORKSTATION;
	}
}

BlueSysInfoMemory::BlueSysInfoMemory()
{
	CcpProcessMemoryInfo info;
	if ( CcpGetProcessMemoryInfo( info ) ) {
		m_workingSet = uint64_t( info.workingSetSize );
		m_pageFile = uint64_t( info.pageFileUsage );
	}

	MEMORYSTATUSEX status;
	status.dwLength = DWORD( sizeof( status ) );
	GlobalMemoryStatusEx( &status );
	m_totalPhysical = uint64_t( status.ullTotalPhys );
	m_availablePhysical = uint64_t( status.ullAvailPhys );
}

#elif defined(__linux__)

namespace
{
bool LinuxIsDirectory( const std::string& path )
{
	struct stat info;
	return !path.empty() && stat( path.c_str(), &info ) == 0 && S_ISDIR( info.st_mode );
}

std::wstring LinuxExistingDirectory( const std::string& preferred, const std::string& homeFallback )
{
	if( LinuxIsDirectory( preferred ) )
	{
		return std::wstring( preferred.begin(), preferred.end() );
	}
	if( LinuxIsDirectory( homeFallback ) )
	{
		return std::wstring( homeFallback.begin(), homeFallback.end() );
	}
	const char* home = std::getenv( "HOME" );
	if( home && LinuxIsDirectory( home ) )
	{
		return std::wstring( home, home + std::strlen( home ) );
	}
	return L"/tmp";
}

std::string LinuxHomePath( const char* suffix )
{
	const char* home = std::getenv( "HOME" );
	if( !home || !home[0] )
	{
		return "";
	}
	std::string path( home );
	if( suffix && suffix[0] )
	{
		if( path.back() != '/' )
		{
			path.push_back( '/' );
		}
		path += suffix;
	}
	return path;
}

std::string LinuxReadLine( const char* path )
{
	std::ifstream input( path );
	std::string line;
	if( !std::getline( input, line ) )
	{
		return "";
	}
	while( !line.empty() && ( line.back() == '\r' || line.back() == ' ' ) )
	{
		line.pop_back();
	}
	return line;
}
}

std::wstring BlueSysInfo::GetUserDocumentsDirectory() const
{
	const char* documents = std::getenv( "XDG_DOCUMENTS_DIR" );
	const std::string preferred = documents && documents[0] ? documents : LinuxHomePath( "Documents" );
	return LinuxExistingDirectory( preferred, LinuxHomePath( "Documents" ) );
}

std::wstring BlueSysInfo::GetSharedApplicationDataDirectory() const
{
	return L"/usr/local/share";
}

std::wstring BlueSysInfo::GetUserApplicationDataDirectory() const
{
	const char* dataHome = std::getenv( "XDG_DATA_HOME" );
	const std::string preferred = dataHome && dataHome[0] ? dataHome : LinuxHomePath( ".local/share" );
	return LinuxExistingDirectory( preferred, LinuxHomePath( ".local/share" ) );
}

std::wstring BlueSysInfo::GetSharedFontsDirectory() const
{
	return L"/usr/local/share/fonts";
}

std::wstring BlueSysInfo::GetSystemFontsDirectory() const
{
	return L"/usr/share/fonts";
}

uint32_t BlueSysInfo::GetProcessBitCount() const
{
#if defined(__LP64__)
	return 64;
#else
	return 32;
#endif
}

uint32_t BlueSysInfo::GetSystemBitCount() const
{
	utsname name = {};
	if( uname( &name ) == 0 )
	{
		if( std::strcmp( name.machine, "x86_64" ) == 0 || std::strcmp( name.machine, "aarch64" ) == 0 )
		{
			return 64;
		}
		return 32;
	}
	return GetProcessBitCount();
}

uint64_t BlueSysInfo::GetProcessStartTime() const
{
	std::ifstream selfStat( "/proc/self/stat" );
	std::string text;
	if( !std::getline( selfStat, text ) )
	{
		return 0;
	}
	const auto commandEnd = text.rfind( ')' );
	if( commandEnd == std::string::npos )
	{
		return 0;
	}
	std::istringstream fields( text.substr( commandEnd + 1 ) );
	std::string token;
	unsigned long long startTicks = 0;
	for( int field = 0; field < 20; ++field )
	{
		if( !( fields >> token ) )
		{
			return 0;
		}
		if( field == 19 )
		{
			startTicks = std::strtoull( token.c_str(), nullptr, 10 );
		}
	}

	std::ifstream procStat( "/proc/stat" );
	std::string key;
	unsigned long long bootSeconds = 0;
	while( procStat >> key )
	{
		if( key == "btime" )
		{
			procStat >> bootSeconds;
			break;
		}
		std::string rest;
		std::getline( procStat, rest );
	}
	const long ticksPerSecond = sysconf( _SC_CLK_TCK );
	if( ticksPerSecond <= 0 || bootSeconds == 0 )
	{
		return 0;
	}
	const unsigned long long unixSeconds = bootSeconds + ( startTicks / static_cast<unsigned long long>( ticksPerSecond ) );
	const unsigned long long fileTimeEpoch = 116444736000000000ULL;
	return unixSeconds * 10000000ULL + fileTimeEpoch;
}

std::string BlueSysInfo::GetMachineUuid() const
{
	const char* paths[] = {
		"/etc/machine-id",
		"/var/lib/dbus/machine-id",
		"/sys/class/dmi/id/product_uuid",
		"/proc/sys/kernel/random/boot_id"
	};
	for( const char* path : paths )
	{
		std::string id = LinuxReadLine( path );
		if( !id.empty() )
		{
			return id;
		}
	}
	return "";
}

std::wstring BlueSysInfo::GetMachineName() const
{
	char buffer[256] = {};
	if( gethostname( buffer, sizeof( buffer ) - 1 ) != 0 )
	{
		return L"";
	}
	if( char* dot = std::strchr( buffer, '.' ) )
	{
		*dot = '\0';
	}
	return std::wstring( buffer, buffer + std::strlen( buffer ) );
}

std::wstring BlueSysInfo::GetDomainName() const
{
	char buffer[256] = {};
	if( gethostname( buffer, sizeof( buffer ) - 1 ) != 0 )
	{
		return L"";
	}
	char* start = buffer;
	if( char* dot = std::strchr( buffer, '.' ) )
	{
		start = dot + 1;
		if( char* next = std::strchr( start, '.' ) )
		{
			*next = '\0';
		}
	}
	return std::wstring( start, start + std::strlen( start ) );
}

BlueSysInfoCpu::BlueSysInfoCpu() :
	m_extensions( PDM::GetCPUInfo().extensions )
{
	auto pdmCpu = PDM::GetCPUInfo();
	m_mHz = pdmCpu.frequency;
	long processors = sysconf( _SC_NPROCESSORS_ONLN );
	m_logicalCpuCount = processors > 0 ? uint32_t( processors ) : 1;
	m_bitCount = 64;
	m_architecture = "AMD64";

	unsigned int eax = 0;
	unsigned int ebx = 0;
	unsigned int ecx = 0;
	unsigned int edx = 0;
	__cpuid( 0, eax, ebx, ecx, edx );
	char vendor[13] = {};
	std::memcpy( vendor, &ebx, 4 );
	std::memcpy( vendor + 4, &edx, 4 );
	std::memcpy( vendor + 8, &ecx, 4 );
	const unsigned int maxBasic = eax;

	unsigned int family = 0;
	unsigned int model = 0;
	unsigned int stepping = 0;
	if( maxBasic >= 1 )
	{
		__cpuid( 1, eax, ebx, ecx, edx );
		stepping = eax & 0xf;
		model = ( ( eax >> 4 ) & 0xf ) | ( ( eax >> 12 ) & 0xf0 );
		family = ( eax >> 8 ) & 0xf;
		if( family == 0xf )
		{
			family += ( eax >> 20 ) & 0xff;
		}
	}
	m_family = family;
	m_revision = ( model << 8 ) | stepping;

	char brand[49] = {};
	__cpuid( 0x80000000, eax, ebx, ecx, edx );
	if( eax >= 0x80000004 )
	{
		for( unsigned int leaf = 0; leaf < 3; ++leaf )
		{
			__cpuid( 0x80000002 + leaf, eax, ebx, ecx, edx );
			std::memcpy( brand + leaf * 16, &eax, 4 );
			std::memcpy( brand + leaf * 16 + 4, &ebx, 4 );
			std::memcpy( brand + leaf * 16 + 8, &ecx, 4 );
			std::memcpy( brand + leaf * 16 + 12, &edx, 4 );
		}
	}
	m_brand = brand;

	const char* platform = std::strstr( vendor, "Intel" ) ? "Intel64" : "AMD64";
	char identifier[160];
	std::snprintf( identifier, sizeof( identifier ), "%s Family %u Model %u Stepping %u, %s", platform, family, model, stepping, vendor );
	m_identifier = identifier;
}

BlueSysInfoOs::BlueSysInfoOs()
{
	m_platform = LINUX;
	m_majorVersion = 0;
	m_minorVersion = 0;
	m_buildNumber = 0;
	m_suite = DESKTOP;

	std::ifstream release( "/etc/os-release" );
	std::string line;
	while( std::getline( release, line ) )
	{
		if( line.compare( 0, 11, "VERSION_ID=" ) == 0 )
		{
			std::string value = line.substr( 11 );
			if( !value.empty() && value.front() == '"' && value.back() == '"' )
			{
				value = value.substr( 1, value.size() - 2 );
			}
			int major = 0;
			int minor = 0;
			std::sscanf( value.c_str(), "%d.%d", &major, &minor );
			m_majorVersion = major;
			m_minorVersion = minor;
		}
		else if( line.compare( 0, 16, "VERSION_CODENAME=" ) == 0 )
		{
			m_patch = line.substr( 16 );
		}
	}
}

BlueSysInfoMemory::BlueSysInfoMemory()
{
	m_workingSet = 0;
	m_pageFile = 0;
	m_totalPhysical = 0;
	m_availablePhysical = 0;

	CcpProcessMemoryInfo info;
	if( CcpGetProcessMemoryInfo( info ) )
	{
		m_workingSet = uint64_t( info.workingSetSize );
		m_pageFile = uint64_t( info.pageFileUsage );
	}

	std::ifstream status( "/proc/self/status" );
	std::string statusLine;
	while( std::getline( status, statusLine ) )
	{
		const auto field = statusLine.find( "VmRSS:" );
		if( field != std::string::npos )
		{
			m_workingSet = std::strtoull( statusLine.c_str() + field + 6, nullptr, 10 ) * 1024ULL;
			break;
		}
	}
	if( m_workingSet == 0 )
	{
		std::ifstream statm( "/proc/self/statm" );
		unsigned long long virtualPages = 0;
		unsigned long long residentPages = 0;
		if( statm >> virtualPages >> residentPages )
		{
			const long pageSize = sysconf( _SC_PAGESIZE );
			if( pageSize > 0 )
			{
				m_workingSet = residentPages * static_cast<unsigned long long>( pageSize );
			}
		}
	}
	if( m_pageFile > m_workingSet )
	{
		m_pageFile = 0;
	}

	std::ifstream meminfo( "/proc/meminfo" );
	std::string memoryLine;
	while( std::getline( meminfo, memoryLine ) )
	{
		if( memoryLine.compare( 0, 9, "MemTotal:" ) == 0 )
		{
			m_totalPhysical = std::strtoull( memoryLine.c_str() + 9, nullptr, 10 ) * 1024ULL;
		}
		else if( memoryLine.compare( 0, 13, "MemAvailable:" ) == 0 )
		{
			m_availablePhysical = std::strtoull( memoryLine.c_str() + 13, nullptr, 10 ) * 1024ULL;
		}
	}
}

#endif



BlueSysInfoNetworkAdapter::BlueSysInfoNetworkAdapter() {}

const BlueSysInfoCpu& BlueSysInfo::GetCpuInfo() const
{
    return m_cpu;
}

const BlueSysInfoOs& BlueSysInfo::GetOsInfo() const
{
    return m_os;
}

BlueStdResult GetPDMByteData( IBlueStream** pdm_stream )
{
	BlueStdResult result;

	std::stringstream buffer;

	auto can_serialize = pdm_proto::GetEVEPublicData( &buffer );
	if( can_serialize )
	{
		std::string pdm_data = buffer.str();
		MemStreamPtr contents;
		contents.CreateInstance();
		contents->Write( pdm_data.data(), pdm_data.size() );
		contents->Seek( 0, ICcpStream::SO_BEGIN );
		*pdm_stream = contents.Detach();
		result = BlueStdResult( BLUE_STD_RESULT_OK );
	}
	else
	{
		result = BlueStdResult( BLUE_STD_RESULT_IO_ERROR, "Failed to serialize pdm data" );
		CCP_LOGERR_CH( s_ch, result.GetMessage() );
	}

	return result;
}
