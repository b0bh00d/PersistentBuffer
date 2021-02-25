#pragma once

#include <array>
#include <iostream>
#include <cassert>
#include <random>
#include <numeric>
#include <functional>

#include "PersistentBuffer.h"

const std::vector<int> max_buffers = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

std::string random_string()
{
	std::string str("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

	std::random_device rd;
	std::mt19937 generator(rd());

	std::shuffle(str.begin(), str.end(), generator);

	return str;
	//return str.substr(0, 32); // assumes 32 < number of characters in str
}

double run_single_buffer_test(std::size_t max_data_size, int iterations = -1)
{
	double total_time{0.0};

	std::random_device rd;
	std::mt19937 rd_mt(rd());

	// generate a random buffer size between 1 and max_data_size
	std::uniform_int_distribution<> buf(1, static_cast<int>(max_data_size));

	while (iterations)
	{
		auto start = std::chrono::steady_clock::now();
		auto buffer = PersistentBuffer::single_buffer(buf(rd_mt));
		auto diff = std::chrono::steady_clock::now() - start;
		total_time += std::chrono::duration<double, std::milli>(diff).count();

		start = std::chrono::steady_clock::now();
		PersistentBuffer::release_buffer(buffer);
		diff = std::chrono::steady_clock::now() - start;
		total_time += std::chrono::duration<double, std::milli>(diff).count();

		if (iterations != -1)
			--iterations;
	}

	return total_time;
}

double run_single_buffer_from_test(std::size_t max_data_size, int iterations = -1)
{
	double total_time{0.0};

	std::random_device rd;
	std::mt19937 rd_mt(rd());

	// generate a random buffer size between 1 and max_data_size
	std::uniform_int_distribution<> buf(1, static_cast<int>(max_data_size));

	while (iterations)
	{
		std::string s = std::move(random_string());

		auto start = std::chrono::steady_clock::now();
		auto buffer = PersistentBuffer::single_buffer_from(s.c_str(), s.length());
		auto diff = std::chrono::steady_clock::now() - start;
		total_time += std::chrono::duration<double, std::milli>(diff).count();

		start = std::chrono::steady_clock::now();
		PersistentBuffer::release_buffer(buffer);
		diff = std::chrono::steady_clock::now() - start;
		total_time += std::chrono::duration<double, std::milli>(diff).count();

		if (iterations != -1)
			--iterations;
	}

	return total_time;
}

double run_release_buffers_test(std::size_t max_data_size, int iterations = -1)
{
	double total_time{0.0};

	std::random_device rd;
	std::mt19937 rd_mt(rd());

	// generate a random buffer size between 1 and max_data_size
	std::uniform_int_distribution<> buf(1, static_cast<int>(max_data_size));

	std::vector<PersistentBuffer::BufferPtr> buffers;
	buffers.resize(max_buffers.size());

	while (iterations)
	{
		std::string s = std::move(random_string());

		for (auto i : max_buffers)
			buffers[i] = PersistentBuffer::single_buffer(buf(rd_mt));

		auto start = std::chrono::steady_clock::now();
		PersistentBuffer::release_buffers(buffers);
		auto diff = std::chrono::steady_clock::now() - start;
		total_time += std::chrono::duration<double, std::milli>(diff).count();

		if (iterations != -1)
			--iterations;
	}

	return total_time;
}

int main(int argc, char* argv[])
{
	PersistentBuffer::initialize();

	// set an upper size for any given buffer size requirement
	auto max = 500000;

	// number of iterations
	auto iter = 1000000;

	// test the speed of the single_buffer() method that will allocate (or reuse)
	// a single buffer
	auto millis = run_single_buffer_test(max, iter);
	std::cout << "     single_buffer(): " << millis << " ms" << std::endl;

	// test the speed of the single_buffer_from() method that will allocate (or reuse)
	// a buffer and the populated with the provided data
	millis = run_single_buffer_from_test(max, iter);
	std::cout << "single_buffer_from(): " << millis << " ms" << std::endl;

	// test the speed of just the release_buffers() method where multiple buffers are
	// returned at the same time
	millis = run_release_buffers_test(max, iter);
	std::cout << "   release_buffers(): " << millis << " ms" << std::endl;

	std::cout << std::endl
			  << PersistentBuffer::buffers_available() << " buffers were allocated out of "
			  << (iter /*run_single_buffer_test()*/ + iter /*run_single_buffer_from_test*/ +
				  (iter * max_buffers.size()) /**run_release_buffers_test*/)
			  << " buffer requests." << std::endl;
}
