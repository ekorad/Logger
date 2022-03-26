#include <iostream>
#include <vector>
#include <array>

#include "ConcurrentBlockingQueue.hpp"

int main()
{
	std::vector<int> vec{ 1, 2, 3, 5, 6, 7 };
	CBC::ConcurrentBlockingQueue<int> cbq(vec.begin(), vec.end());
	cbq.setTimeoutDuration(std::chrono::milliseconds(1000));
	std::vector<int> outvec;
	outvec.resize(3);

	cbq.popBatch(outvec.begin(), outvec.size());

	for (const auto elem : outvec)
	{
		std::cout << elem << " ";
	}

	return 0;
}