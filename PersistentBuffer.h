#pragma once

/// @file PersistentBuffer.h
/// Contains a utility class that provides persistent and recycled heap buffers.
///
/// @author Bob Hood

#include <map>
#include <list>
#include <vector>
#include <memory>
#include <mutex>
#include <bitset>

#include <time.h>

/// @class PersistentBuffer
/// @brief Management of persistent buffers
///
/// This class will manage the allocation and usage of persistent
/// buffer storage.  Buffers are allocated on the heap, potentially
/// being reused as they are acquired from a free pool and then
/// released back to it by their users when they are no longer
/// needed.  If the 'DropOld' policy is not enabled, its buffer
/// re-use behavior can reduce both run-time heap fragmentation and
/// allocations overhead.
///
/// It is implemented as a Singleton for the following reasons:
///     1. Persistent buffers don't need to be context-specific;
///     2. Persistent buffers are available throughout the application
///        space, so subsystems wishing to employ them needn't have
///        an instance of the manager passed down to them through a
///        long chain, unnecessarily cluttering argument lists.
class PersistentBuffer
{
public: // aliases and enums
	enum Policy
	{
		ZeroBuffer = 0,		// zero-initialize buffers when they are placed into use
		DropOld,			// perform periodic garbage collection in 'ExpandAsNeeded' mode
		TotalPolicies,
	};

	class Buffer
	{
	public: // aliases and enums
		using DataPtr = std::shared_ptr<uint8_t>;

	public: // methods
		DataPtr data() const { return DataPtr(m_buffer); }
		uint32_t size() const { return m_data_size; }
		time_t last_used() const { return m_last_used; }

	private: // methods
		void operator delete(void*) {}
		void reset()
		{
			m_in_use = false;
			m_data_size = 0;
			m_allocated = 0;
			m_buffer.reset();
			m_last_used = 0;
		}

	private: // data members
		// is this buffer currently in use?
		bool m_in_use{false};
		// how many bytes of user data are in the buffer?
		uint32_t m_data_size{0};
		// what is the total allocated size of the buffer?
		uint32_t m_allocated{0};
		// how many times has this buffer been used?
		uint32_t m_usage_count{0};
		// when was this buffer last used?
		time_t m_last_used{0};
		// pointer to (sizeof(uint8_t) * m_size) data
		DataPtr m_buffer;

		friend PersistentBuffer;
	};
	using BufferPtr = std::shared_ptr<Buffer>;

public: // methods
	/*!
	Initialize the state of the buffer PersistentBuffer before beginning to
	use it.  This initialization sets the 'ExpandAsNeeded' and
	'ZeroBuffer' policies as the default policies, and no age-based
	garbage collection.  You will need to call other methods to
	configure the PersistentBuffer for different policies and behaviors,
	if these do not fit your needs, BEFORE you begin using it.
	*/
	static void initialize();

	/*!
	The PersistentBuffer can be instructed to usage-expire buffers by setting
	a time-out value using this method.  Buffers that have not been
	reused within the indicated time range since they were last used
	will have their resources returned to the operating system.

	\note Automatically sets the 'DropOld' policy.

	\param seconds The amount of time that must elapse before the buffer will be released from the pool.
	*/
	static void set_cleanup_timeout(time_t seconds = 0);
	/*!
	This method checks to see if a PersistentBuffer policy is currently
	in effect.

	\param policy The policy to check.
	*/
	static bool policy_is_active(Policy policy) { return m_policies[policy] != 0; }
	/*!
	Enable the indicated PersistentBuffer policy.

	\param policy The policy to enable.
	*/
	static void set_policy(Policy policy);
	/*!
	Enable multiple PersistentBuffer policies at once.  Any currently enabled
	policies are cleared before processing the provided list.

	\param policies An initialize list of policies to enable.
	*/
	static void set_policy(std::initializer_list<Policy> policies);
	/*!
	Disable the indicated PersistentBuffer policy.

	\param policy The policy to disable.
	*/
	static void clear_policy(Policy policy);

	/*!
	Reports the number of buffers that are currently in use

	\return The number of buffers that the PersistentBuffer has in active use.
	*/
	static size_t buffers_in_use() { return m_buffers_in_use; }

	/*!
	Reports the number of buffers that are allocated for use (either active
	or pending).

	\return The number of buffers that the PersistentBuffer has allocated.
	*/
	static size_t buffers_available() { return m_size_list.size(); }

	/*!
	Clear all currently allocated buffers and start from scratch
	*/
	static void reset();

	/*!
	Retrieve a buffer from the PersistentBuffer that contains the minimum
	number of bytes requested.  This may re-use a previously
	allocated buffer if its size matches the requirement.

	\param min_size The minimum amount of bytes the buffer must provide.
	\return A std::shared_ptr to the memory of the PersistentBuffer buffer.
	*/
	static BufferPtr single_buffer(uint32_t min_size);

	/*!
	Retrieve a buffer from the PersistentBuffer that contains at least
	the number of bytes requested.  Additionally, place the provided
	content into the buffer before returning it to the caller.

	\param data The raw data to be placed into the buffer.
	\param size The number of bytes in the provided content.
	\return A std::shared_ptr to the memory of the PersistentBuffer buffer.
	*/
	static BufferPtr single_buffer_from(const uint8_t* data, uint32_t size);

	/*!
	Retrieve a buffer from the PersistentBuffer that contains at least
	the number of bytes requested.  Additionally, place the provided
	content into the buffer before returning it to the caller.

	\param data The raw data to be placed into the buffer.
	\param size The number of bytes in the provided content.
	\return A std::shared_ptr to the memory of the PersistentBuffer buffer.
	*/
	static BufferPtr single_buffer_from(const char* data, size_t size);

	/*!
	Checks to see if a BufferPtr is currently holding valid content.

	\param buffer The buffer to check.
	*/
	static bool buffer_in_use(BufferPtr buffer);

	static bool release_buffer(BufferPtr buffer);
	static bool release_buffers(const std::vector<PersistentBuffer::BufferPtr>& buffers);

private: // aliases and enums
	using BufferMap = std::map<BufferPtr, bool>;
	using BufferMapKey = std::pair<BufferPtr, bool>;
	using SizeList = std::vector<BufferPtr>;

private: // methods
	// release any buffers that haven't been used in a given timeout period
	static void garbage_collect(time_t start_time);

	// this is the single-buffer working method, but it does not lock the mutex.
	// this is so it can be used by multiple public methods
	static BufferPtr single_buffer_unprotected(uint32_t min_size);

private: // data members
	static time_t m_cleanup_timeout; // zero means do not garbage collect; !zero is in seconds
	static time_t m_last_cleanup_check;

	static std::bitset<Policy::TotalPolicies> m_policies;

	static std::mutex m_buffers_lock;
	static BufferMap m_buffers;
	static int m_buffers_in_use;

	// this list is sorted ascending on 'm_allocated' for binary searching
	static SizeList m_size_list;
};
