// Copyright © 2014 CCP ehf.

#include "StdAfx.h"

#if defined(__linux__)
#include <mutex>
#include <unordered_map>
#endif

#if CCP_STACKLESS

//
// Custom memory allocators for Python
//
// These are primarily used to track statistics about memory allocated by Python.
//

CCP_STATS_DECLARE( pyMemory, "Blue/Memory/Python", false, CST_MEMORY, "The amount of memory allocated for Python" );

struct MeasuredAllocator
{
	MeasuredAllocator() :
		measurement{ g_ccpStatistics_pyMemory } {};
	PyMemAllocatorEx allocator;
	CcpStaticStatisticsEntry& measurement;
};

namespace Ccp
{

#if defined(__linux__)
namespace
{
std::mutex g_requestedSizeMutex;
std::unordered_map<void*, size_t> g_requestedSizes;

void RememberRequestedSize( void* pointer, size_t size )
{
	if( !pointer )
	{
		return;
	}
	std::lock_guard<std::mutex> lock( g_requestedSizeMutex );
	g_requestedSizes[pointer] = size;
}

size_t TakeRequestedSize( void* pointer )
{
	std::lock_guard<std::mutex> lock( g_requestedSizeMutex );
	auto found = g_requestedSizes.find( pointer );
	if( found == g_requestedSizes.end() )
	{
		return 0;
	}
	size_t size = found->second;
	g_requestedSizes.erase( found );
	return size;
}
}
#endif

void* MeasuredMalloc( void* ctx, size_t size )
{
	auto _this = reinterpret_cast<MeasuredAllocator*>( ctx );
	auto ret = _this->allocator.malloc( _this->allocator.ctx, size );
#if _WIN32
	_this->measurement.Add( int64_t( size ) );
#elif defined(__APPLE__)
	_this->measurement.Add( int64_t( CCPMSize( ret ) ) );
#elif defined(__linux__)
	if( ret )
	{
		RememberRequestedSize( ret, size );
		_this->measurement.Add( int64_t( size ) );
	}
#else
#error "Unsupported platform"
#endif
	return ret;
}

void* MeasuredCalloc( void* ctx, size_t nelem, size_t size )
{
	auto _this = reinterpret_cast<MeasuredAllocator*>( ctx );
	auto ret = _this->allocator.calloc( _this->allocator.ctx, nelem, size );
#if _WIN32
	_this->measurement.Add( int64_t( nelem * size ) );
#elif defined(__APPLE__)
	_this->measurement.Add( int64_t( CCPMSize( ret ) ) );
#elif defined(__linux__)
	if( ret )
	{
		RememberRequestedSize( ret, nelem * size );
		_this->measurement.Add( int64_t( nelem * size ) );
	}
#else
#error "Unsupported platform"
#endif
	return ret;
}

void* MeasuredRealloc( void* ctx, void* ptr, size_t newSize )
{
	auto _this = reinterpret_cast<MeasuredAllocator*>( ctx );
#if defined(__linux__)
	uint64_t prev = ptr ? TakeRequestedSize( ptr ) : 0;
#else
	uint64_t prev = ptr ? CCPMSize( ptr ) : 0;
#endif
	auto ret = _this->allocator.realloc( _this->allocator.ctx, ptr, newSize );
#if _WIN32
	_this->measurement.Add( int64_t( newSize - prev ) );
#elif defined(__APPLE__)
	_this->measurement.Add( int64_t( CCPMSize( ret ) - prev ) );
#elif defined(__linux__)
	if( ret )
	{
		RememberRequestedSize( ret, newSize );
		_this->measurement.Add( int64_t( newSize ) - int64_t( prev ) );
	}
	else if( ptr )
	{
		RememberRequestedSize( ptr, size_t( prev ) );
	}
#else
#error "Unsupported platform"
#endif
	return ret;
}

void MeasuredFree( void* ctx, void* ptr )
{
	auto _this = reinterpret_cast<MeasuredAllocator*>( ctx );
	if ( ptr )
	{
#if defined(__linux__)
		_this->measurement.Add( -int64_t( TakeRequestedSize( ptr ) ) );
#else
		_this->measurement.Add( -int64_t( CCPMSize( ptr ) ) );
#endif
		_this->allocator.free( _this->allocator.ctx, ptr );
	}
}
}

extern "C" BLUEIMPORT void BlueInstallPythonMemoryHooks()
{
	static MeasuredAllocator measurer{};
	static PyMemAllocatorEx override{&measurer, Ccp::MeasuredMalloc, Ccp::MeasuredCalloc, Ccp::MeasuredRealloc, Ccp::MeasuredFree };

	PyMemAllocatorEx temporaryHelper;

	// Only install an allocator for the `PYMEM_DOMAIN_RAW` because this is the allocator Python will always end up calling,
	// even from the _MEM and _OBJ domains. The reason for this is that Python's _MEM and _OBJ domains use an internal heap
	// that works on memory allocated through the _RAW domain.
	PyMem_GetAllocator( PYMEM_DOMAIN_RAW, &temporaryHelper );
	if( temporaryHelper.ctx != &override.ctx )
	{
		measurer.allocator = temporaryHelper;
		PyMem_SetAllocator( PYMEM_DOMAIN_RAW, &override );
	}
}

#endif
