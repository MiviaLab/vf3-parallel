/*
 * ThreadPool.hpp
 *
 *  Created on: 21 nov 2017
 *      Author: vcarletti
 */

/*
* Parallel Matching Engine with local and global state stack (no look-free stack)
*/
#ifndef PARALLELMATCHINGTHREADPOOLWLS_HPP
#define PARALLELMATCHINGTHREADPOOLWLS_HPP

#include <atomic>
#include <thread>
#include <mutex>
#include <array>
#include <vector>
#include <stack>
#include <bitset>
#include <limits>
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
class ParallelMatchingEngineWLS
		: public MatchingEngine<VFState>
{
private:
    

	using MatchingEngine<VFState>::solutions;
	using MatchingEngine<VFState>::visit;
	using MatchingEngine<VFState>::solCount;
	using MatchingEngine<VFState>::storeSolutions;
	using MatchingEngine<VFState>::fist_solution_time;

	std::mutex statesMutex;
	std::mutex solutionsMutex;
	std::atomic<bool> once;

	//std::atomic<uint32_t> solCount;
    int16_t cpu;
    int16_t numThreads;
	std::vector<std::thread> pool;
	std::atomic<int16_t> activeWorkerCount;

    uint16_t ssrLimitLevelForGlobalStack; 					//all the states belonging to ssr levels leq the this limit are put inside the global stack
	uint16_t localStackLimitSize;         					//limit size for the local stack. All the exceeding states are stored in the global stack
	std::vector<std::vector<VFState*> >localStateStack; 	//Local stack address by thread-id (ids are assigned by the pool)
	std::vector<bool> workerCountIncrement;
    std::stack<VFState*> globalStateStack;
	struct timeval time;

public:
	ParallelMatchingEngineWLS(unsigned short int numThreads,
        bool storeSolutions=false,
        short int cpu = -1,
        unsigned short int ssrLimitLevelForGlobalStack = 3,
        unsigned short int localStackLimitSize = 50,  
        MatchingVisitor<VFState> *visit = NULL):
		MatchingEngine<VFState>(visit, storeSolutions),
		once(false),
		cpu(cpu),
		numThreads(numThreads),
		pool(numThreads),
		activeWorkerCount(0),
        ssrLimitLevelForGlobalStack(ssrLimitLevelForGlobalStack),
        localStackLimitSize(localStackLimitSize),
        localStateStack(numThreads),
		workerCountIncrement(numThreads, true){}

	~ParallelMatchingEngineWLS(){}

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

	void Run(ThreadId thread_id) 
	{
		VFState* s = NULL;
		do
		{
			if(s)
			{
				//Qui notifico che sto processando
				//std::cout<<"Processing "<< thread_id << std::endl;
				ProcessState(s, thread_id);
				delete s;
			}
		}while(GetState(thread_id, &s));
	}

	bool ProcessState(VFState *s, ThreadId thread_id)
	{
		if (s->IsGoal())
		{
			if(!once.exchange(true))
				gettimeofday(&(this->fist_solution_time), NULL);
			
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
				VFState* s1 = new VFState(*s);
				s1->AddPair(n1, n2);
				PutState(s1, thread_id);
			}
		}		
		return false;
		
	}

	void PutState(VFState* s, ThreadId thread_id) {
		if(thread_id == NULL_THREAD || 
			s->CoreLen() < ssrLimitLevelForGlobalStack || 
			localStateStack[thread_id].size() > localStackLimitSize)
		{
			std::lock_guard<std::mutex> guard(statesMutex);
			globalStateStack.push(s);
		}else
		{
			localStateStack[thread_id].push_back(s);
		}
	}

	//In questo modo, quando sono finiti gli stati i thread rimangono appesi.
	//Come facciamo a definire una condizione di chiusura dei thread?
	bool GetState(ThreadId thread_id, VFState** res)
	{
		*res = NULL;
        //Getting from local stack firts
        if(localStateStack[thread_id].size())
        {
           *res = localStateStack[thread_id].back();
           localStateStack[thread_id].pop_back();
        }
        else
        {
			std::unique_lock<std::mutex> stateLock(statesMutex);
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
        }
		return true;
	}

	void StartPool()
	{
		int current_cpu = cpu; 
		for (ThreadId i = 0; i < numThreads; ++i)
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

#ifndef WIN32
	inline void SetAffinity(int cpu, pthread_t handle)
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

};

}

#endif /* INCLUDE_PARALLEL_PARALLELMATCHINGTHREADPOOL_HPP_ */
