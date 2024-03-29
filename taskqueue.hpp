/*******************************************************************************
 * A producer-consumer pattern for the multi-threaded execution
 ******************************************************************************/

#ifndef _TASKQUEUE_HPP_
#define _TASKQUEUE_HPP_
#include <optional>
#include "taskqueue.h"
//use for synch on stdout
static std::mutex _stdoutMutex;
template <typename TItem, typename TAction, typename TOnDisconnect>
TaskQueue<TItem, TAction, TOnDisconnect>::TaskQueue(size_t workerCount, size_t slotCount, TAction& action, TOnDisconnect& onDisconnect) :
	_slotCount{ slotCount },
	_itemCount{ 0 },
	_onDisconnect{ onDisconnect },
	_stay{ true }
{
	for (size_t i = 0; i < workerCount; ++i)
	{
		_workers.emplace_back(&work, std::ref(*this), std::ref(action));
	}
}

template <typename TItem, typename TAction, typename TOnDisconnect>
void TaskQueue<TItem, TAction, TOnDisconnect>::produce(TItem item)
{
	// Non-RAII unique_lock to be blocked by a producer who needs a slot.
	{
		// Wait for an available slot...
		std::unique_lock<std::mutex> slotCountLock{ _slotCountMutex };
		_producers.wait(slotCountLock, [&]() { return _slotCount > 0; });
		--_slotCount;
	}
	// RAII lock_guard locked for buffer.
	{
		// Lock the buffer.
		std::lock_guard<std::mutex> bufferLock{ _bufferMutex };
		_buffer.push(item);
	}
	// RAII lock_guard locked for itemCount.
	{
		// Announce available item.
		std::lock_guard<std::mutex> itemCountLock(_itemCountMutex);
		++_itemCount;
		_consumers.notify_one();
	}
}

template <typename TItem, typename TAction, typename TOnDisconnect>
std::optional<TItem> TaskQueue<TItem, TAction, TOnDisconnect>::consume()
{
	std::optional<TItem> result = std::nullopt;
	// Non-RAII unique_lock to be blocked by a consumer who needs an item.
	{
		// Wait for an available item or termination...
		std::unique_lock<std::mutex> itemCountLock{ _itemCountMutex };
		_consumers.wait(itemCountLock, [&]() { return (_itemCount > 0) || (!_stay); });
		if (_itemCount == 0)
		{
			_consumers.notify_one();
			return result;
		}
		--_itemCount;
	}
	// RAII lock_guard locked for buffer.
	{
		// Lock the buffer.
		std::lock_guard<std::mutex> bufferLock{ _bufferMutex };
		result = _buffer.front();
		_buffer.pop();
	}
	// RAII lock_guard locked for slots.
	{
		// Announce available slot.
		std::lock_guard<std::mutex> slotCountLock{ _slotCountMutex };
		++_slotCount;
		_producers.notify_one();
	}
	return result;
}

template <typename TItem, typename TAction, typename TOnDisconnect>
void TaskQueue<TItem, TAction, TOnDisconnect>::work(TaskQueue<TItem, TAction, TOnDisconnect>& tq, TAction& action)
{
	while (true)
	{
		{
			std::lock_guard<std::mutex> usersLock{ _stdoutMutex };
			std::cout
				<< "Thread ["
				<< std::this_thread::get_id()
				<< "] is waiting for a task."
				<< std::endl;
		}
		std::optional<TItem> item = tq.consume();
		if (!item)
		{
			// Termination of idle threads.
			break;
		}

		{
			std::lock_guard<std::mutex> usersLock{ _stdoutMutex };
			std::cout
				<< "Thread ["
				<< std::this_thread::get_id()
				<< "] is executing a task."
				<< std::endl;
		}

		if (!action(*item))
		{
			// Decision to terminate workers.
			tq.disconnect();
		}
	}

	{
		std::lock_guard<std::mutex> usersLock{ _stdoutMutex };
		std::cout
			<< "Thread ["
			<< std::this_thread::get_id()
			<< "] is exiting."
			<< std::endl;
	}
}

template <typename TItem, typename TAction, typename TOnDisconnect>
void TaskQueue<TItem, TAction, TOnDisconnect>::disconnect()
{
	_stay = false;
	_onDisconnect();
}

template <typename TItem, typename TAction, typename TOnDisconnect>
TaskQueue<TItem, TAction, TOnDisconnect>::~TaskQueue()
{
	disconnect();
	for (std::thread& worker : _workers)
	{
		worker.join();
	}
}

#endif
