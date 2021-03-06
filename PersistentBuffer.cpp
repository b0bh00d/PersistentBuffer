#include <cassert>
#include <sstream>
#include <algorithm>
#include <iostream>

#include "PersistentBuffer.h"

time_t PersistentBuffer::m_cleanup_timeout{0}; // zero means do not garbage collect; >zero is in seconds
time_t PersistentBuffer::m_last_cleanup_check{0};
std::bitset<PersistentBuffer::Policy::TotalPolicies> PersistentBuffer::m_policies;
std::mutex PersistentBuffer::m_buffers_lock;
PersistentBuffer::BufferMap PersistentBuffer::m_buffers;
int PersistentBuffer::m_buffers_in_use{0};
PersistentBuffer::SizeList PersistentBuffer::m_size_list;

static bool m_initialized{false};

#if PERSISTENTBUFFER_TRACKING >= 2
using tracking_data_t = std::pair<std::string, int>;
using tracking_map_t = std::map<const void*, tracking_data_t>;
static tracking_map_t _tracking_map;
#endif

//----------------------------------------------------------------------------
// PersistentBuffer::Buffer methods

uint8_t const * const PersistentBuffer::Buffer::ro() const
{
	if (m_in_use)
		return m_buffer.get();

	assert(false);
	return nullptr;
}

uint8_t * const PersistentBuffer::Buffer::rw() const
{
	if (m_in_use)
		return m_buffer.get();

	assert(false);
	return nullptr;
}

//----------------------------------------------------------------------------
// PersistentBuffer methods

void PersistentBuffer::initialize()
{
	m_policies.set(Policy::ZeroBuffer);

	m_initialized = true;
}

void PersistentBuffer::set_cleanup_timeout(time_t seconds)
{
	m_cleanup_timeout = seconds;
	m_last_cleanup_check = time(nullptr);
	set_policy(Policy::DropOld);
}

void PersistentBuffer::set_policy(Policy policy)
{
	m_policies.set(policy);
}

void PersistentBuffer::set_policy(std::initializer_list<Policy> policies)
{
	for (Policy p : policies)
		m_policies.set(p);
}

void PersistentBuffer::clear_policy(Policy policy)
{
	m_policies.reset(policy);
}

void PersistentBuffer::reset()
{
	m_policies.reset();
	m_policies.set(Policy::ZeroBuffer);
	for (BufferMapKey key : m_buffers)
		key.first->reset();
	m_buffers.clear();
}

PersistentBuffer::BufferPtr PersistentBuffer::single_buffer(uint32_t min_size
#if PERSISTENTBUFFER_TRACKING >= 2
	, const tracking_data_t& caller
#endif
)
{
	std::unique_lock<std::mutex> buffers_lock(m_buffers_lock);
	auto buffer{single_buffer_unprotected(min_size)};
#if PERSISTENTBUFFER_TRACKING >= 2
	if (!caller.first.empty())
	{
		auto key{static_cast<const void *>(buffer->ro())};
		_tracking_map[key] = caller;
		std::cerr << "+++ buffer " << key << " allocated by " << caller.first << ":"
			<< caller.second << std::endl;
		std::cerr.flush();
	}
#endif
#if PERSISTENTBUFFER_TRACKING == 1 || PERSISTENTBUFFER_TRACKING == 3
	std::cerr << "<< " << m_size_list.size() << " buffers allocated, "
		<< m_buffers_in_use << " buffers in use, "
		<< (m_size_list.size() - m_buffers_in_use) << " buffers free."
		<< std::endl;
	std::cerr.flush();
#endif
  return buffer;
}

// get a buffer with the required 'size' holding the provided content
PersistentBuffer::BufferPtr PersistentBuffer::single_buffer_from(const std::string& data
#if PERSISTENTBUFFER_TRACKING >= 2
	, const tracking_data_t& caller
#endif
)
{
	auto buffer{single_buffer_from(reinterpret_cast<const uint8_t*>(data.c_str()), static_cast<uint32_t>(data.length() + 1))};
#if PERSISTENTBUFFER_TRACKING >= 2
	if (!caller.first.empty())
	{
		auto key{static_cast<const void*>(buffer->ro())};
		_tracking_map[key] = caller;
		std::cerr << "+++ buffer " << key << " allocated by " << caller.first << ":" << caller.second << std::endl;
		std::cerr.flush();
	}
#endif
#if PERSISTENTBUFFER_TRACKING == 1 || PERSISTENTBUFFER_TRACKING == 3
	std::cerr << "<< " << m_size_list.size() << " buffers allocated, " << m_buffers_in_use << " buffers in use, "
			<< (m_size_list.size() - m_buffers_in_use) << " buffers free." << std::endl;
	std::cerr.flush();
#endif
	return buffer;
}

PersistentBuffer::BufferPtr PersistentBuffer::single_buffer_from(const char* data, size_t size
#if PERSISTENTBUFFER_TRACKING >= 2
	, const tracking_data_t& caller
#endif
)
{
	auto buffer{single_buffer_from(reinterpret_cast<const uint8_t*>(data), static_cast<uint32_t>(size))};
#if PERSISTENTBUFFER_TRACKING >= 2
	if (!caller.first.empty())
	{
		auto key{static_cast<const void*>(buffer->ro())};
		_tracking_map[key] = caller;
		std::cerr << "+++ buffer " << key << " allocated by " << caller.first << ":" << caller.second << std::endl;
		std::cerr.flush();
	}
#endif
#if PERSISTENTBUFFER_TRACKING == 1 || PERSISTENTBUFFER_TRACKING == 3
	std::cerr << "<< " << m_size_list.size() << " buffers allocated, " << m_buffers_in_use << " buffers in use, "
			  << (m_size_list.size() - m_buffers_in_use) << " buffers free." << std::endl;
	std::cerr.flush();
#endif
	return buffer;
}

PersistentBuffer::BufferPtr PersistentBuffer::single_buffer_from(const uint8_t* data, uint32_t size
#if PERSISTENTBUFFER_TRACKING >= 2
	, const tracking_data_t& caller
#endif
)
{
	std::unique_lock<std::mutex> buffers_lock(m_buffers_lock);
	auto buffer{single_buffer_unprotected(size)};
	auto p{buffer->rw()};
	memcpy(p, data, size);
#if PERSISTENTBUFFER_TRACKING >= 2
	if (!caller.first.empty())
	{
		auto key{static_cast<const void*>(p)};
		_tracking_map[key] = caller;
		std::cerr << "+++ buffer " << key << " allocated by " << caller.first << ":" << caller.second << std::endl;
		std::cerr.flush();
	}
#endif
#if PERSISTENTBUFFER_TRACKING == 1 || PERSISTENTBUFFER_TRACKING == 3
	std::cerr << "<< " << m_size_list.size() << " buffers allocated, " << m_buffers_in_use << " buffers in use, "
			  << (m_size_list.size() - m_buffers_in_use) << " buffers free." << std::endl;
	std::cerr.flush();
#endif
	return buffer;
}

bool PersistentBuffer::buffer_in_use(PersistentBuffer::BufferPtr buffer)
{
	std::unique_lock<std::mutex> buffers_lock(m_buffers_lock);
	return buffer.get() && buffer->m_in_use;
}

void PersistentBuffer::garbage_collect(time_t start_time)
{
	std::vector<BufferPtr> buffers_to_drop;

	// TODO: map 'm_last_used' by age when 'm_cleanup_timeout' to speed up searching
	for (BufferMapKey key : m_buffers)
	{
		if (!key.first->m_in_use && (start_time - key.first->m_last_used) > m_cleanup_timeout)
			buffers_to_drop.push_back(key.first); // drop this one, it's too old
	}

	for (auto iter = buffers_to_drop.begin(); iter != buffers_to_drop.end(); ++iter)
	{
		m_buffers.erase(*iter);
		m_size_list.erase(std::find(m_size_list.begin(), m_size_list.end(), *iter));
	}
	m_size_list.shrink_to_fit();
}

PersistentBuffer::BufferPtr PersistentBuffer::single_buffer_unprotected(uint32_t min_size)
{
	assert(m_initialized);

	// see if any existing free buffers match our 'min_size' requirement

	auto iter = std::lower_bound(m_size_list.begin(), m_size_list.end(), min_size, [](const BufferPtr& buffer, uint32_t value) {
		return buffer.get()->m_allocated < value;
	});

	while (iter != m_size_list.end() && (*iter)->m_in_use)
		++iter;

	if (iter != m_size_list.end())
	{
		BufferPtr buffer = *iter;
		assert(buffer->m_allocated >= min_size);
		buffer->m_in_use = true;
		++m_buffers_in_use;
		buffer->m_data_size = min_size;
		++buffer->m_usage_count;

		// this pathway is the main consumer of CPU time in PersistentBuffer, so
		// invoking memset() here will TREMENDOUSLY impact performance (by multiple
		// orders of magnitude--I kid you not).  if you need it, YOU enable it.

#if 0
		// 1,000,000 iterations: ~165ms -> ~11075ms
		if (m_policies[Policy::ZeroBuffer])
			memset(buffer->data().get(), 0, buffer->m_data_size);
#endif

#if 0
		// 1,000,000 iterations: ~165ms -> ~15383ms
		if (m_policies[Policy::ZeroBuffer])
			memset(buffer->data().get(), 0, buffer->m_allocated);
#endif

		return buffer;
	}

	// if we reach here, there are no free buffers, or there are none that match 'min_size'
	BufferPtr buffer = std::make_shared<Buffer>();
	buffer->m_in_use = true;
	++buffer->m_usage_count;
	buffer->m_allocated = min_size;
	buffer->m_data_size = min_size;
	buffer->m_buffer = Buffer::DataPtr(new uint8_t[min_size], [](uint8_t* p) {
		delete[] p;
	});
	// invoking memset() here doesn't appear to have a noticible impact on performance
	if (m_policies[Policy::ZeroBuffer])
		memset(buffer->rw(), 0, buffer->m_allocated);

	m_buffers[buffer] = true;
	++m_buffers_in_use;

	m_size_list.push_back(buffer);
	std::sort(m_size_list.begin(), m_size_list.end(), [](const BufferPtr& a, const BufferPtr& b) {
		return a->m_allocated < b->m_allocated;
	});

	// do garbage collection, if indicated
	if (m_policies[Policy::DropOld] && m_cleanup_timeout)
	{
		time_t now = time(nullptr);
		if ((now - m_last_cleanup_check) > m_cleanup_timeout)
		{
			m_last_cleanup_check = now;
			garbage_collect(now);
		}
	}

	return buffer;
}

bool PersistentBuffer::release_buffer(PersistentBuffer::BufferPtr buffer
#if PERSISTENTBUFFER_TRACKING >= 2
	, const tracking_data_t &caller
#endif
)
{
	if (buffer.get() && buffer->m_in_use)
	{
		auto p{buffer->ro()};

		std::unique_lock<std::mutex> buffers_lock(m_buffers_lock);

#if PERSISTENTBUFFER_TRACKING >= 2
		if (!caller.first.empty()) {
			auto key{static_cast<const void *>(p)};
			if (_tracking_map.find(key) == _tracking_map.end())
			std::cerr << "!!! FAILED to locate " << key
						<< " in tracking map." << std::endl;
			else {
			std::cerr << "--- buffer " << key << " released by "
						<< caller.first << ":" << caller.second << "."
						<< std::endl;
			_tracking_map.erase(key);
			}
			std::cerr.flush();
		}
#endif

		m_buffers[buffer] = true;

		buffer->m_in_use = false;
		--m_buffers_in_use;
		buffer->m_last_used = time(nullptr);
#if PERSISTENTBUFFER_TRACKING == 1 || PERSISTENTBUFFER_TRACKING == 3
		std::cerr << "<< " << m_size_list.size() << " buffers allocated, " << m_buffers_in_use << " buffers in use, "
				<< (m_size_list.size() - m_buffers_in_use) << " buffers free." << std::endl;
		std::cerr.flush();
#endif
	}
	return true;
}

bool PersistentBuffer::release_buffers(const std::vector<PersistentBuffer::BufferPtr>& buffers
#if PERSISTENTBUFFER_TRACKING >= 2
	, const tracking_data_t& caller
#endif
)
{
	std::unique_lock<std::mutex> buffers_lock(m_buffers_lock);

	for (BufferPtr buffer : buffers)
	{
		if (buffer.get() && buffer->m_in_use)
		{
#if PERSISTENTBUFFER_TRACKING >= 2
			if (!caller.first.empty())
			{
				auto key{static_cast<const void*>(buffer->ro())};
				if (_tracking_map.find(key) == _tracking_map.end())
					std::cerr << "!!! FAILED to locate " << key << " in tracking map." << std::endl;
				else
				{
					std::cerr << "--- buffer " << key << " released by " << caller.first << ":" << caller.second << "." << std::endl;
					_tracking_map.erase(key);
				}
				std::cerr.flush();
			}
#endif
			buffer->m_in_use = false;
			--m_buffers_in_use;
			buffer->m_last_used = time(nullptr);
		}
	}
#if PERSISTENTBUFFER_TRACKING == 1 || PERSISTENTBUFFER_TRACKING == 3
	std::cerr << "<< " << m_size_list.size() << " buffers allocated, " << m_buffers_in_use << " buffers in use, "
			  << (m_size_list.size() - m_buffers_in_use) << " buffers free." << std::endl;
	std::cerr.flush();
#endif
	return true;
}
