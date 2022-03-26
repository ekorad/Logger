#pragma once

/* ************************************************************************** */
/* ******************************** INCLUDES ******************************** */
/* ************************************************************************** */

#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <optional>

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
		InsufficientElements
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
		using Container = std::deque<ValueType>;
		using AllocatorType = Container::allocator_type;
		using SizeType = Container::size_type;
		using Milliseconds = std::chrono::milliseconds;

		/* ----------------------- CONSTRUCTION / DESTRUCTION ----------------------- */

		ConcurrentBlockingQueue() = default;
		ConcurrentBlockingQueue(const SizeType count, const ValueType& value = {});
		ConcurrentBlockingQueue(const std::initializer_list<ValueType>& initializer);
		ConcurrentBlockingQueue(std::initializer_list<ValueType>&& initializer);
		template <typename InputIterator>
		ConcurrentBlockingQueue(InputIterator first, InputIterator last);
		ConcurrentBlockingQueue(const ConcurrentBlockingQueue& other);
		ConcurrentBlockingQueue(ConcurrentBlockingQueue&& other);

		/* ------------------------------- FUNCTIONS -------------------------------- */

		StatusCode pushOne(const ValueType& value);
		StatusCode pushOne(ValueType&& value);
		StatusCode pushBatch(const std::initializer_list<ValueType>& list);
		StatusCode pushBatch(std::initializer_list<ValueType>&& list);
		template <typename InputIterator>
		StatusCode pushBatch(InputIterator first, InputIterator last);

		StatusCode popOne(ValueType& output, const bool blocking = true);
		template <typename OutputIterator>
		StatusCode popBatch(OutputIterator destination,
			const SizeType count,
			const bool blocking = true);
		StatusCode getFront(ValueType& output, const bool blocking = true);
		template <typename OutputIterator>
		StatusCode getFrontBatch(OutputIterator destination,
			const SizeType count,
			const bool blocking = true);

		void setTimeoutDuration(const std::optional<Milliseconds>& duration);
		std::optional<Milliseconds> getTimeoutDuration() const;

		void setInterrupted(const bool value = true);
		bool isInterrupted() const;

		SizeType getSize() const;
		bool isEmpty() const;
		void clear();

	private:

		/* ************************************************************************** */
		/* ******************************** PRIVATE ********************************* */
		/* ************************************************************************** */

		/* ------------------------------- FUNCTIONS -------------------------------- */

		template <typename... Args>
		StatusCode pushInternal(Args&&... args);

		template <typename... Args>
		StatusCode getFrontInternal(const bool blocking,
			const auto waitPredicate,
			Args&&... args);

		template <typename InputIterator>
		void dataInsert(InputIterator first, InputIterator last);
		template <typename U> void dataInsert(U&& value);

		template <typename OutputIterator>
		void dataGet(OutputIterator destination, const SizeType count, const bool pop);
		void dataGet(ValueType& output, const bool pop);

		/* ------------------------------ DATA MEMBERS ------------------------------ */

		std::mutex _accessMutex;
		std::condition_variable _waitNotification;
		bool _interrupted{ false };

		Container _data;

		std::optional<Milliseconds> _timeoutDuration;
	};
}

/* ************************************************************************** */
/* ***************************** IMPLEMENTATION ***************************** */
/* ************************************************************************** */

/* ----------------------- CONCURRENT BLOCKING QUEUE ------------------------ */

namespace CBC
{
	/* ************************************************************************** */
	/* ********************************* PUBLIC ********************************* */
	/* ************************************************************************** */

	/* ----------------------- CONSTRUCTION / DESTRUCTION ----------------------- */

	template <typename T>
	ConcurrentBlockingQueue<T>::ConcurrentBlockingQueue(const SizeType count,
		const ValueType& value)
		: _data(count, value) {}

	template <typename T>
	ConcurrentBlockingQueue<T>::ConcurrentBlockingQueue(
		const std::initializer_list<ValueType>& initializer)
		: _data{ initializer } {}

	template <typename T>
	ConcurrentBlockingQueue<T>::ConcurrentBlockingQueue(
		std::initializer_list<ValueType>&& initializer)
		: _data{ std::move(initializer) } {}

	template <typename T>
	template <typename InputIterator>
	ConcurrentBlockingQueue<T>::ConcurrentBlockingQueue(InputIterator first,
		InputIterator last)
		: _data{ first, last } {}

	template <typename T>
	ConcurrentBlockingQueue<T>::ConcurrentBlockingQueue(
		const ConcurrentBlockingQueue& other)
		: _data{ other._data } {}

	template <typename T>
	ConcurrentBlockingQueue<T>::ConcurrentBlockingQueue(
		ConcurrentBlockingQueue&& other)
		: _data{ std::move(other._data) } {}

	/* ------------------------------- FUNCTIONS -------------------------------- */

	template <typename T>
	StatusCode ConcurrentBlockingQueue<T>::pushOne(const ValueType& value)
	{
		return pushInternal(value);
	}

	template <typename T>
	StatusCode ConcurrentBlockingQueue<T>::pushOne(ValueType&& value)
	{
		return pushInternal(value);
	}

	template <typename T>
	StatusCode ConcurrentBlockingQueue<T>::pushBatch(
		const std::initializer_list<ValueType>& list)
	{
		return pushInternal(list.begin(), list.end());
	}

	template <typename T>
	StatusCode ConcurrentBlockingQueue<T>::pushBatch(
		std::initializer_list<ValueType>&& list)
	{
		return pushInternal(std::make_move_iterator(list.begin()),
			std::make_move_iterator(list.end()));
	}

	template <typename T>
	template <typename InputIterator>
	StatusCode ConcurrentBlockingQueue<T>::pushBatch(InputIterator first,
		InputIterator last)
	{
		return pushInternal(first, last);
	}

	template <typename T>
	StatusCode ConcurrentBlockingQueue<T>::popOne(ValueType& output,
		const bool blocking)
	{
		const auto waitPredicate = [this]
		{
			return !_data.empty() || _interrupted;
		};

		return getFrontInternal(blocking, waitPredicate, output, true);
	}

	template <typename T>
	template <typename OutputIterator>
	StatusCode ConcurrentBlockingQueue<T>::popBatch(OutputIterator destination,
		const SizeType count,
		const bool blocking)
	{
		const auto waitPredicate = [this, count]
		{
			return (_data.size() >= count) || _interrupted;
		};

		return getFrontInternal(blocking, waitPredicate, destination, count, true);
	}

	template <typename T>
	StatusCode ConcurrentBlockingQueue<T>::getFront(ValueType& output,
		const bool blocking)
	{
		const auto waitPredicate = [this]
		{
			return !_data.empty() || _interrupted;
		};

		return getFrontInternal(blocking, waitPredicate, output, false);
	}

	template <typename T>
	template <typename OutputIterator>
	StatusCode ConcurrentBlockingQueue<T>::getFrontBatch(OutputIterator destination,
		const SizeType count,
		const bool blocking)
	{
		const auto waitPredicate = [this, count]
		{
			return (_data.size() >= count) || _interrupted;
		};

		return getFrontInternal(blocking, waitPredicate, destination, count, false);
	}

	template <typename T>
	void ConcurrentBlockingQueue<T>::setTimeoutDuration(
		const std::optional<Milliseconds>& duration)
	{
		std::lock_guard lock{ _accessMutex };
		_timeoutDuration = duration;
	}

	template <typename T>
	std::optional<typename ConcurrentBlockingQueue<T>::Milliseconds>
		ConcurrentBlockingQueue<T>::getTimeoutDuration() const
	{
		std::lock_guard lock{ _accessMutex };
		return _timeoutDuration;
	}

	template <typename T>
	void ConcurrentBlockingQueue<T>::setInterrupted(const bool value)
	{
		std::lock_guard lock{ _accessMutex };
		_interrupted = value;
	}

	template <typename T>
	bool ConcurrentBlockingQueue<T>::isInterrupted() const
	{
		std::lock_guard lock{ _accessMutex };
		return _interrupted;
	}

	template <typename T>
	typename ConcurrentBlockingQueue<T>::SizeType
		ConcurrentBlockingQueue<T>::getSize() const
	{
		std::lock_guard lock{ _accessMutex };
		return _data.size();
	}

	template <typename T>
	bool ConcurrentBlockingQueue<T>::isEmpty() const
	{
		std::lock_guard lock{ _accessMutex };
		return _data.empty();
	}

	template <typename T>
	void ConcurrentBlockingQueue<T>::clear()
	{
		std::lock_guard lock{ _accessMutex };
		_data.clear();
	}

	/* ************************************************************************** */
	/* ******************************** PRIVATE ********************************* */
	/* ************************************************************************** */

	/* ------------------------------- FUNCTIONS -------------------------------- */

	template <typename T>
	template <typename... Args>
	StatusCode ConcurrentBlockingQueue<T>::pushInternal(Args&&... args)
	{
		std::unique_lock lock{ _accessMutex };

		if (_interrupted)
		{
			return StatusCode::Interrupted;
		}

		dataInsert(std::forward<Args>(args)...);
		lock.unlock();
		_waitNotification.notify_all();

		return StatusCode::Success;
	}

	template <typename T>
	template <typename... Args>
	StatusCode ConcurrentBlockingQueue<T>::getFrontInternal(const bool blocking,
		const auto waitPredicate,
		Args&&... args)
	{
		std::unique_lock lock{ _accessMutex };

		if (blocking)
		{
			if (_timeoutDuration.has_value()
				&& (_waitNotification.wait_for(lock, _timeoutDuration.value(),
					waitPredicate) == false))
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
			return StatusCode::InsufficientElements;
		}

		dataGet(args...);

		return StatusCode::Success;
	}

	template <typename T>
	template <typename InputIterator>
	void ConcurrentBlockingQueue<T>::dataInsert(InputIterator first,
		InputIterator last)
	{
		_data.insert(_data.end(), first, last);
	}

	template <typename T>
	template <typename U>
	void ConcurrentBlockingQueue<T>::dataInsert(U&& value)
	{
		_data.push_back(value);
	}

	template <typename T>
	template <typename OutputIterator>
	void ConcurrentBlockingQueue<T>::dataGet(OutputIterator destination,
		const SizeType count,
		const bool pop)
	{
		if (pop)
		{
			std::move(_data.begin(), _data.begin() + count, destination);
			_data.erase(_data.begin(), _data.begin() + count);
		}
		else
		{
			std::copy(_data.begin(), _data.begin() + count, destination);
		}
	}

	template <typename T>
	void ConcurrentBlockingQueue<T>::dataGet(ValueType& output,
		const bool pop)
	{		
		if (pop)
		{
			output = std::move(_data.front());
			_data.pop_front();
		}
		else
		{
			output = _data.front();
		}
	}
}