/*
 * ThreadPool.hpp
 *
 *  Created on: 21 nov 2017
 *      Author: vcarletti
 */

/*
Parallel Matching Engine with global state stack only (no look-free stack)
*/

#ifndef PARALLELMATCHINGTHREADPOOL_HPP
#define PARALLELMATCHINGTHREADPOOL_HPP

#include <atomic>
#include <thread>
#include <mutex>
#include <array>
#include <vector>
#include <stack>
#include <cstdint>

#ifndef WIN32
#include <unistd.h>
#include <sys/time.h>
#else
#include <Windows.h>
#include <stdint.h>
#endif

#include "ARGraph.hpp"
#include "MatchingEngine.hpp"

namespace vflib {

typedef unsigned short ThreadId;
constexpr ThreadId NULL_THREAD = (std::numeric_limits<ThreadId>::max)();

template<typename VFState>
class ParallelMatchingEngine
		: public MatchingEngine<VFState>
{
protected:
	typedef unsigned short ThreadId;
	using MatchingEngine<VFState>::solutions;
	using MatchingEngine<VFState>::visit;
	using MatchingEngine<VFState>::solCount;
	using MatchingEngine<VFState>::storeSolutions;
	using MatchingEngine<VFState>::fist_solution_time;

	std::mutex statesMutex;
	std::mutex solutionsMutex;
	std::atomic<bool> once;

	int16_t cpu;
	int16_t numThreads;
	std::vector<std::thread> pool;
	std::atomic<int16_t> activeWorkerCount;
	std::stack<VFState*> globalStateStack;
	struct timeval time;

	std::vector<bool> workerCountIncrement;
	
public:
	ParallelMatchingEngine(unsigned short int numThreads, 
		bool storeSolutions=false, 
		short int cpu = -1,
		MatchingVisitor<VFState> *visit = NULL):
		MatchingEngine<VFState>(visit, storeSolutions),
		once(false),
		cpu(cpu),
		numThreads(numThreads),
		pool(numThreads),
		activeWorkerCount(0),
		workerCountIncrement(numThreads,true){}

	~ParallelMatchingEngine(){}

	bool FindAllMatchings(VFState& s)
	{
		ProcessState(&s, NULL_THREAD);
		StartPool();

		//Waiting for process thread
		for (auto &th : pool) {
			if (th.joinable()) {
				th.join();
			}
		}

		//Exiting
		return true;
	}

	inline size_t GetThreadCount() const {
		return pool.size();
	}

	inline void ResetSolutionCounter()
	{
		solCount = 0;
		once = false;
	}

private: 

	inline unsigned GetRemainingStates() {
		std::lock_guard<std::mutex> guard(statesMutex);
		return globalStateStack.size();
	}

	void Run(ThreadId thread_id) 
	{
		VFState* s = NULL;
		do
		{
			if(s)
			{
				ProcessState(s, thread_id);
				delete s;	
			}
		}while(GetState(&s, thread_id));
	}

	inline void GenerateState(VFState *s, nodeID_t n1, nodeID_t n2, ThreadId thread_id)
	{
		VFState* s1 = new VFState(*s);
		s1->AddPair(n1, n2);
		PutState(s1, thread_id);
	}

	bool ProcessState(VFState *s, ThreadId thread_id)
	{
		if (s->IsGoal())
		{
			if(!once.exchange(true, std::memory_order_acq_rel))
			{
				gettimeofday(&(this->fist_solution_time),NULL);
			}

			solCount++;

			if(storeSolutions)
			{
				std::lock_guard<std::mutex> guard(solutionsMutex);
				MatchingSolution sol;
				s->GetCoreSet(sol);
				solutions.push_back(sol);
			}
			if (visit)
			{
				return (*visit)(*s);
			}
			return true;
		}

		if (s->IsDead())
			return false;

		nodeID_t n1 = NULL_NODE, n2 = NULL_NODE;
		while (s->NextPair(&n1, &n2, n1, n2))
		{
			if (s->IsFeasiblePair(n1, n2))
			{
				GenerateState(s, n1, n2, thread_id);
			}
		}		
		return false;
		
	}

	void PutState(VFState* s, ThreadId thread_id) {
		std::lock_guard<std::mutex> guard(statesMutex);
		globalStateStack.push(s);
	}

	bool GetState(VFState** res, ThreadId thread_id)
	{
		*res = NULL;
		std::lock_guard<std::mutex> stateLock(statesMutex);
		
		if(globalStateStack.size())
		{
			*res = globalStateStack.top();
			globalStateStack.pop();
			if(workerCountIncrement[thread_id])
			{
				activeWorkerCount++;
				workerCountIncrement[thread_id]=false;
			}
		}
		else
		{
			if(!workerCountIncrement[thread_id])
			{
				activeWorkerCount--;
				workerCountIncrement[thread_id]=true;
			}

			if(activeWorkerCount <= 0)
			{
				return false;
			}
		}
		return true;
	}

#ifndef WIN32
	void SetAffinity(int cpu, pthread_t handle)
	{
		cpu_set_t cpuset;
    	CPU_ZERO(&cpuset);
    	CPU_SET(cpu, &cpuset);
		int rc = pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpuset);
		if (rc != 0) 
		{
			std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
		}
	}
#endif

	void StartPool()
	{
		int current_cpu = cpu; 

		for (size_t i = 0; i < numThreads; ++i)
		{
			pool[i] = std::thread( [this,i]{ this->Run(i); } );
#ifndef WIN32
			//If cpu is not -1 set the thread affinity starting from the cpu
			if(current_cpu > -1)
			{
				SetAffinity(current_cpu, pool[i].native_handle());
				current_cpu++;
			}
#endif
		}
	}

};

}

#endif /* INCLUDE_PARALLEL_PARALLELMATCHINGTHREADPOOL_HPP_ */
