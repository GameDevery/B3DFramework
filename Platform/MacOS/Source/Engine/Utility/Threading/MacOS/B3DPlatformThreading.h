//************************************* B3D Framework - Copyright 2026 Marko Pintera *************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//
#pragma once

#include <mutex>
#include <shared_mutex>
#include <condition_variable>

namespace b3d
{
	/** @addtogroup Threading
	 *  @{
	 */

	/** Wrapper for the C++ std::mutex. */
	using Mutex = std::mutex;

	/** Wrapper for the C++ std::shared_mutex. */
	using SharedMutex = std::shared_mutex;

	/** Wrapper for the C++ std::recursive_mutex. */
	using RecursiveMutex = std::recursive_mutex;

	/** Wrapper for the C++ std::condition_variable. */
	using ConditionVariable = std::condition_variable;

	/** @} */
} // namespace b3d
