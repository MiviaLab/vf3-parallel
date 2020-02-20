/*
 * ThreadPool.hpp
 *
 *  Created on: 21 nov 2017
 *      Author: vcarletti
 */

/*
* VF3P2
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

#include "ARGraph.hpp"
#include "ParallelMatchingEngine.hpp"


namespace vflib {

template<typename VFState>
class ParallelMatchingEngineWLS
		: public ParallelMatchingEngine<VFState>
{
private:

    uint16_t ssrLimitLevelForGlobalStack; 					//all the states belonging to ssr levels leq the this limit are put inside the global stack
	uint16_t localStackLimitSize;         					//limit size for the local stack. All the exceeding states are stored in the global stack
	std::vector<std::vector<VFState*> >localStateStack; 	//Local stack address by thread-id (ids are assigned by the pool)

public:
	ParallelMatchingEngineWLS(unsigned short int numThreads,
        bool storeSolutions=false,
        short int cpu = -1,
        unsigned short int ssrLimitLevelForGlobalStack = 3,
        unsigned short int localStackLimitSize = 50,  
        MatchingVisitor<VFState> *visit = NULL):
		ParallelMatchingEngine<VFState>(numThreads, storeSolutions, cpu, visit),
        ssrLimitLevelForGlobalStack(ssrLimitLevelForGlobalStack),
        localStackLimitSize(localStackLimitSize),
        localStateStack(numThreads){}

	~ParallelMatchingEngineWLS(){}


private:

	void PutState(VFState* s, ThreadId thread_id) {
		if(thread_id == NULL_THREAD || 
			s->CoreLen() < ssrLimitLevelForGlobalStack || 
			localStateStack[thread_id].size() > localStackLimitSize)
		{
			ParallelMatchingEngine<VFState>::PutState(s, thread_id);
		}
		else
		{
			localStateStack[thread_id].push_back(s);
		}
	}

	//In questo modo, quando sono finiti gli stati i thread rimangono appesi.
	//Come facciamo a definire una condizione di chiusura dei thread?
	bool GetState(VFState** res, ThreadId thread_id)
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
			return ParallelMatchingEngine<VFState>::GetState(res, thread_id);
        }
		return true;
	}
};

}

#endif /* INCLUDE_PARALLEL_PARALLELMATCHINGTHREADPOOL_HPP_ */
