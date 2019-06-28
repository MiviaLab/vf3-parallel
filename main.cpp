#include <stdio.h>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <string>
#include <time.h>
#ifndef WIN32
#include <unistd.h>
#include <sys/time.h>
#else
#include <Windows.h>
#include <stdint.h>
#endif

#include "ARGLoader.hpp"
#include "ARGraph.hpp"
#include "NodeSorter.hpp"
#include "VF3NodeSorter.hpp"
#include "RINodeSorter.hpp"
#include "State.hpp"
#include "ProbabilityStrategy.hpp"
#include "NodeClassifier.hpp"
#include "MatchingEngine.hpp"
#include "VF3SubState.hpp"
#include "VF3KSubState.hpp"
#include "VF3LightSubState.hpp"
#include "parallel/VF3ParallelSubState.hpp"

using namespace vflib;

#define TIME_LIMIT 1

#define NUM_OF_THREADS 8

#ifndef VF3BIO
typedef int32_t data_t;
#else
typedef std::string data_t;
#endif

#if defined(VF3PV1)
#include "parallel/ParallelMatchingEngine.hpp"
typedef VF3ParallelSubState<data_t, data_t, Empty, Empty> state_t;
#define MATCHING_INIT ParallelMatchingEngine<state_t > me(numOfThreads, false, cpu)
#elif defined(VF3PV2)
#include "parallel/ParallelMatchingEngineWLS.hpp"
typedef VF3ParallelSubState<data_t, data_t, Empty, Empty> state_t;
#define MATCHING_INIT ParallelMatchingEngineWLS<state_t > me(numOfThreads, false, cpu, 2, 100)
#elif defined(VF3L)
typedef VF3LightSubState<data_t, data_t, Empty, Empty> state_t;
#define MATCHING_INIT MatchingEngine<state_t > me(false)
#endif

#ifdef WIN32
int gettimeofday(struct timeval * tp, struct timezone * tzp)
{
	// Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
	// This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
	// until 00:00:00 January 1, 1970 
	static const uint64_t EPOCH = ((uint64_t)116444736000000000ULL);

	SYSTEMTIME  system_time;
	FILETIME    file_time;
	uint64_t    time;

	GetSystemTime(&system_time);
	SystemTimeToFileTime(&system_time, &file_time);
	time = ((uint64_t)file_time.dwLowDateTime);
	time += ((uint64_t)file_time.dwHighDateTime) << 32;

	tp->tv_sec = (long)((time - EPOCH) / 10000000L);
	tp->tv_usec = (long)(system_time.wMilliseconds * 1000);
	return 0;
}
#endif

static long long state_counter = 0;

int32_t main(int32_t argc, char** argv)
{

	char *pattern, *target;
	int numOfThreads = 1;
	short int cpu = -1;
	float limit = TIME_LIMIT;
	double timeAll = 0;
	struct timeval start, end;

	state_counter = 0;
	size_t sols = 0;
#ifndef VF3L
	if (argc < 3)
	{
		std::cout << "Usage: vf3 [pattern] [target] [num of threads (opt)] [cpu (opt)]\n";
		return -1;
	}

	numOfThreads = atoi(argv[3]);
	cpu = atoi(argv[4]);
#else
	if (argc < 2)
	{
		std::cout << "Usage: vf3 [pattern] [target] \n";
		return -1;
	}
#endif
	pattern = argv[1];
	target = argv[2];

	std::ifstream graphInPat(pattern);
	std::ifstream graphInTarg(target);

	std::vector<MatchingSolution> solutions;

	StreamARGLoader<data_t, Empty> pattloader(graphInPat);
	StreamARGLoader<data_t, Empty> targloader(graphInTarg);

	ARGraph<data_t, Empty> patt_graph(&pattloader);
	ARGraph<data_t, Empty> targ_graph(&targloader);

	NodeClassifier<data_t, Empty> classifier(&targ_graph);
	NodeClassifier<data_t, Empty> classifier2(&patt_graph, classifier);
	std::vector<uint32_t> class_patt = classifier2.GetClasses();
	std::vector<uint32_t> class_targ = classifier.GetClasses();

	MATCHING_INIT;
	VF3NodeSorter<data_t, Empty, SubIsoNodeProbability<data_t, Empty> > sorter(&targ_graph);
	std::vector<nodeID_t> sorted = sorter.SortNodes(&patt_graph);

	gettimeofday(&start, NULL);
	state_t s0(&patt_graph, &targ_graph, class_patt.data(), class_targ.data(), classifier.CountClasses(), sorted.data());
	me.FindAllMatchings(s0);
	sols = me.GetSolutionsCount();

	gettimeofday(&end,NULL);
    timeAll = ((end.tv_sec  - start.tv_sec) * 1000000u + 
         end.tv_usec - start.tv_usec) / 1.e6;

	/*me.GetSolutions(solutions);

	std::cout << "Solution Found" << std::endl;
	std::vector<MatchingSolution>::iterator it;
	for(it = solutions.begin(); it != solutions.end(); it++)
	{
		std::cout<< me.SolutionToString(*it) << std::endl;
	}*/
	/*std::cout << "SORT: ";
	for(uint32_t i = 0; i < sorted.size(); i++)
	{
		std::cout<<sorted[i]<<" ";
	}
	std::cout << std::endl;
	std::cout << "END" << std::endl;*/

	std::cout << sols << " " << timeAll;

	//system("PAUSE");

	return 0;
}
