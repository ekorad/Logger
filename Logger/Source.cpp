#include <iostream>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <vector>

/* ************************************************************************** */
/* ********************************* TYPES ********************************** */
/* ************************************************************************** */

namespace CBC
{
	/* ------------------------------ STATUS CODE ------------------------------- */

	enum class StatusCode
	{
		Success,
		Interrupted,
		Timeout,
		NotEnoughElements
	};

	/* ----------------------- CONCURRENT BLOCKING QUEUE ------------------------ */

	template <typename T>
	class ConcurrentBlockingQueue
	{
	public:

		/* ************************************************************************** */
		/* ********************************* PUBLIC ********************************* */
		/* ************************************************************************** */

		/* --------------------------- USING DECLARATIONS --------------------------- */

		using ValueType = T;
		using InternalQueueType = std::deque<ValueType>;
		using SizeType = InternalQueueType::size_type;
		using AllocatorType = InternalQueueType::allocator_type;
		using Milliseconds = std::chrono::milliseconds;

		/* ----------------------------- PUSH FUNCTIONS ----------------------------- */

		StatusCode pushOne(const ValueType& value);
		template <template <typename, typename> typename Container>
		StatusCode pushBatch(const Container<ValueType, AllocatorType>& container);
		StatusCode pushBatch(const std::initializer_list<ValueType>& list);

		/* ----------------------------- POP FUNCTIONS ------------------------------ */

		StatusCode popOne(ValueType& output, const bool blocking = true);

		template <template <typename, typename> typename Container>
		StatusCode popBatch(Container<ValueType, AllocatorType>& output,
			const SizeType count,
			const bool blocking = true);

		void setTimeoutDuration(const std::optional<Milliseconds>& duration)
		{
			std::lock_guard lock{ _accessMutex };
			_timeoutDuration = duration;
		}

		std::optional<Milliseconds> getTimeoutDuration() const
		{
			std::lock_guard lock{ _accessMutex };
			return _timeoutDuration;
		}

		void setInterrupted(const bool value = true)
		{
			std::lock_guard lock{ _accessMutex };
			_interrupted = value;
		}

		bool isInterrupted() const
		{
			std::lock_guard lock{ _accessMutex };
			return _interrupted;
		}

		bool isEmpty() const
		{
			std::lock_guard lock{ _accessMutex };
			return _data.empty();
		}

		SizeType getSize() const
		{
			std::lock_guard lock{ _accessMutex };
			return _data.size();
		}

	private:

		/* ************************************************************************** */
		/* ******************************** PRIVATE ********************************* */
		/* ************************************************************************** */

		/* --------------------------- INTERNAL FUNCTIONS --------------------------- */

		template <typename U> StatusCode pushInternal(const U& input);
		template <typename... Args>
		StatusCode popInternal(const bool blocking, const auto waitPredicate,
			Args&&... args);

		/* ------------------------------- OVERLOADS -------------------------------- */

		template <template <typename, typename> typename Container>
		void insertData(const Container<ValueType, AllocatorType>& container);
		inline void insertData(const std::initializer_list<ValueType>& list);
		inline void insertData(const ValueType& value);

		template <template <typename, typename> typename Container>
		void dataGet(Container<ValueType, AllocatorType>& output, const SizeType count);
		void dataGet(ValueType& output);

		/* ------------------------------ DATA MEMBERS ------------------------------ */

		std::mutex _accessMutex;
		std::condition_variable _waitNotification;
		bool _interrupted{ false };
		std::optional<Milliseconds> _timeoutDuration;

		std::deque<T> _data;
	};
}

/* ************************************************************************** */
/* ***************************** IMPLEMENTATION ***************************** */
/* ************************************************************************** */

namespace CBC
{
	/* ************************************************************************** */
	/* ********************************* PUBLIC ********************************* */
	/* ************************************************************************** */

	/* ----------------------------- PUSH FUNCTIONS ----------------------------- */

	template <typename T>
	StatusCode ConcurrentBlockingQueue<T>::pushOne(const ValueType& value)
	{
		return pushInternal(value);
	}

	template <typename T>
	template <template <typename, typename> typename Container>
	StatusCode ConcurrentBlockingQueue<T>::pushBatch(
		const Container<ValueType, AllocatorType>& container)
	{
		if (container.empty())
		{
			return StatusCode::Success;
		}

		return pushInternal(container);
	}

	template <typename T>
	StatusCode ConcurrentBlockingQueue<T>::pushBatch(
		const std::initializer_list<ValueType>& list)
	{
		if (list.size() == 0)
		{
			return StatusCode::Success;
		}

		return pushInternal(list);
	}

	/* ----------------------------- POP FUNCTIONS ------------------------------ */

	template <typename T>
	StatusCode ConcurrentBlockingQueue<T>::popOne(ValueType& output,
		const bool blocking)
	{
		const auto waitPredicate = [this]
		{
			return !_data.empty() || _interrupted;
		};

		return popInternal(blocking, waitPredicate, output);
	}

	template <typename T>
	template <template <typename, typename> typename Container>
	StatusCode ConcurrentBlockingQueue<T>::popBatch(Container<ValueType, AllocatorType>& output,
		const SizeType count,
		const bool blocking)
	{
		if (count == 0)
		{
			return StatusCode::Success;
		}

		const auto waitPredicate = [this, count]
		{
			return (_data.size() >= count) || _interrupted;
		};

		return popInternal(blocking, waitPredicate, output, count);
	}

	/* ************************************************************************** */
	/* ******************************** PRIVATE ********************************* */
	/* ************************************************************************** */

	/* --------------------------- INTERNAL FUNCTIONS --------------------------- */

	template <typename T>
	template <typename U>
	StatusCode ConcurrentBlockingQueue<T>::pushInternal(const U& input)
	{
		std::unique_lock lock{ _accessMutex };

		if (_interrupted)
		{
			return StatusCode::Interrupted;
		}

		insertData(input);
		lock.unlock();
		_waitNotification.notify_all();

		return StatusCode::Success;
	}

	template <typename T>
	template <typename... Args>
	StatusCode ConcurrentBlockingQueue<T>::popInternal(const bool blocking,
		const auto waitPredicate,
		Args&&... args)
	{
		std::unique_lock lock{ _accessMutex };

		if (blocking)
		{
			if (_timeoutDuration.has_value()
				&& (_waitNotification.wait_for(lock, _timeoutDuration.value(), waitPredicate) == false))
			{
				return StatusCode::Timeout;
			}
			else
			{
				_waitNotification.wait(lock, waitPredicate);
			}
		}

		if (_interrupted)
		{
			return StatusCode::Interrupted;
		}

		if (_data.empty())
		{
			return StatusCode::NotEnoughElements;
		}

		dataGet(args...);

		return StatusCode::Success;
	}

	/* ------------------------------- OVERLOADS -------------------------------- */

	template <typename T>
	template <template <typename, typename> typename Container>
	void ConcurrentBlockingQueue<T>::insertData(
		const Container<ValueType, AllocatorType>& container)
	{
		_data.insert(_data.end(), container.begin(), container.end());
	}

	template <typename T>
	inline void ConcurrentBlockingQueue<T>::insertData(
		const std::initializer_list<ValueType>& list)
	{
		_data.insert(_data.end(), list);
	}

	template <typename T>
	inline void ConcurrentBlockingQueue<T>::insertData(const ValueType& value)
	{
		_data.push_back(value);
	}

	template <typename T>
	template <template <typename, typename> typename Container>
	void ConcurrentBlockingQueue<T>::dataGet(Container<ValueType,
		AllocatorType>& output,
		const SizeType count)
	{
		output.resize(count);
		std::move(_data.begin(), _data.begin() + count, output.begin());
		_data.erase(_data.begin(), _data.begin() + count);
	}

	template <typename T>
	void ConcurrentBlockingQueue<T>::dataGet(ValueType& output)
	{
		output = _data.front();
		_data.pop_front();
	}
}

int main()
{
	CBC::ConcurrentBlockingQueue<int> ints;
	ints.pushOne(5);
	ints.pushBatch({ 1, 2, 3, 4, 5 });
	std::vector<int> output;
	ints.popBatch(output, 3);
	for (auto i : output)
	{
		std::cout << i << " ";
	}
	return 0;
}