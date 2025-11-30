#pragma once

#include <cassert>
#include <mutex>
#include <memory>
#include <unordered_set>

#include "fast_logger.h"

struct DefaultAllocator
{
	template <class T, class... Args>
	static T* Allocate(Args&&... args)
	{
		return new T(std::forward<Args>(args)...);
	}

	template <class T>
	static void Free(T* ptr)
	{
		delete ptr;
	}
};

struct TrackingAllocator
{
	inline static std::mutex Lock;
	inline static std::unordered_map<void*, const char*> ActiveAllocations;
	
	template <class T, class... Args>
	static T* Allocate(Args&&... args)
	{
		auto* ptr = DefaultAllocator::Allocate<T>(std::forward<Args>(args)...);

		{
			std::unique_lock lock{ Lock };
			const char* tpn = __FUNCSIG__;
			ActiveAllocations.insert({ ptr, tpn });
		}

		return ptr;
	}

	template <class T>
	static void Free(T* ptr)
	{
		if (ptr)
		{
			std::unique_lock lock{ Lock };
			auto erasedCount = ActiveAllocations.erase(ptr);
			assert(erasedCount == 1);
		}

		DefaultAllocator::Free(ptr);
	}

	static void AssertNoOutstandingAllocations()
	{
		std::unique_lock lock{ Lock };

		if (ActiveAllocations.empty())
			return;

		for (auto allocation : ActiveAllocations)
		{
			FAST_LOG(LFStructs::Operation::Leak, reinterpret_cast<size_t>(allocation.first));
		}

		abort();
	}
};

#ifndef ALLOCATOR
	#ifdef TRACK_ALLOCATIONS
		#define ALLOCATOR TrackingAllocator
	#else
		#define ALLOCATOR DefaultAllocator
	#endif
#endif