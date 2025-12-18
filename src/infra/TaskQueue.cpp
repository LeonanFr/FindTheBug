#include "TaskQueue.hpp"
#include <print>

namespace FindTheBug {
	
	TaskQueue::TaskQueue(size_t numWorkers)
		: stop(false) {
		
		size_t threadsToCreate = numWorkers > 0 ? numWorkers : 1;

		for (size_t i = 0; i < threadsToCreate; ++i) {
			workers.emplace_back(&TaskQueue::workerLoop, this);
		}

	}

	TaskQueue::~TaskQueue() {
		{
			std::unique_lock<std::mutex> lock(queueMutex);
			stop = true;
		}
		cv.notify_all();
		for (auto& worker : workers) {
			if (worker.joinable()) {
				worker.join();
			}
		}
	}

	void TaskQueue::enqueue(std::function<void()> task) {
		{
			std::unique_lock<std::mutex> lock(queueMutex);
			tasks.push(std::move(task));
		}
		cv.notify_one();
	}

	void TaskQueue::workerLoop() {
		while (true) {
			std::function<void()> task;
			{
				std::unique_lock<std::mutex> lock(queueMutex);
				cv.wait(lock, [this]() { return stop || !tasks.empty(); });
				if (stop && tasks.empty()) {
					return;
				}
				task = std::move(tasks.front());
				tasks.pop();
			}
			try {
				task();
			}
			catch (const std::exception& e) {
				std::print("[TaskQueue] Exception in task: {}", e.what());
			}
			catch (...) {
				std::print("[TaskQueue] Unknown exception in task");
			}
		}
	}
}