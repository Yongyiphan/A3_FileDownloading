/*******************************************************************************
 * A producer-consumer pattern for the multi-threaded execution
 ******************************************************************************/

#ifndef _TASKQUEUE_H_
#define _TASKQUEUE_H_

#include <vector>
#include <queue>
#include <mutex>
#include <optional>
#include <thread>

template <typename TItem, typename TAction, typename TOnDisconnect>
class TaskQueue
{
public:
	TaskQueue(size_t workerCount, size_t slotCount, TAction& action, TOnDisconnect& disconnect);
	~TaskQueue();

	std::optional<TItem> consume();
	void produce(TItem item);

	TaskQueue() = delete;
	TaskQueue(const TaskQueue&) = delete;
	TaskQueue(TaskQueue&&) = delete;
	TaskQueue& operator=(const TaskQueue&) = delete;
	TaskQueue& operator=(TaskQueue&&) = delete;

private:

	static void work(TaskQueue<TItem, TAction, TOnDisconnect>& tq, TAction& action);
	void disconnect();

	// Pool of worker threads.
	std::vector<std::thread> _workers;

	// Buffer of slots for items.
	std::mutex _bufferMutex;
	std::queue<TItem> _buffer;

	// Count of available slots.
	std::mutex _slotCountMutex;
	size_t _slotCount;
	// Critical section condition for decreasing slots.
	std::condition_variable _producers;

	// Count of available items.
	std::mutex _itemCountMutex;
	size_t _itemCount;
	// Critical section condition for decreasing items.
	std::condition_variable _consumers;

	volatile bool _stay;

	TOnDisconnect& _onDisconnect;
};

#include "taskqueue.hpp"

#endif
