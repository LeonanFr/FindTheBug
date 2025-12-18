#pragma once

#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <vector>

namespace FindTheBug {
	
	class TaskQueue {
	public:
		explicit TaskQueue(size_t numWorkers = std::thread::hardware_concurrency());
		~TaskQueue();

		TaskQueue(const TaskQueue&) = delete;
		void enqueue(std::function<void()> task);

	private:
		std::vector<std::thread> workers;
		std::queue<std::function<void()>> tasks;

		std::mutex queueMutex;
		std::condition_variable cv;

		bool stop;
		void workerLoop();
	};

}