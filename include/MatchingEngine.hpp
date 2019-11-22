/**
 * @file MatchingEngine.hpp
 * @author V.Carletti (vcarletti\@unisa.it)
 * @date   24/10/2017
 * @brief  Declaration of the matching engine.
 */

#ifndef MATCH_H
#define MATCH_H

#include <atomic>
#include <stack>
#include <string>
#include <iostream>
#include <sstream>
#include "ARGraph.hpp"

#ifndef WIN32
#include <unistd.h>
#include <sys/time.h>
#else
#include <Windows.h>
#include <stdint.h>
#endif

namespace vflib
{
	typedef std::vector<std::pair<nodeID_t, nodeID_t> > MatchingSolution;

	template <typename VFState >

	/*
	* @class MatchingVisitor
	* @brief Functor invoked when a new solution is found
	* @details Extends this class to add action to perfom when a new solution is found
	* 	The Matching vistor must be passed to the constructor of the Matching Engine to be used
	*	MatchingEngine e = new MatchingEngine(visitor)
	*/
	class MatchingVisitor
	{
	public:
		/**
		* @brief Definition of the visitor type.\n
		* @details A match visitor is a function that is invoked for each match that has been found.
		* If the function returns FALSE, then the next match is
		* searched; else the seach process terminates.
		* @param [in] s Goal VFState Found
		* @return TRUE If the matching process must be stopped.
		* @return FALSE If the matching process must continue.
		*/
		virtual bool operator()(VFState& state) = 0;

		virtual ~MatchingVisitor();
	};

	template <typename VFState >
	class MatchingEngine
	{
	protected:
		/*
		* @note  core1 and core2 will contain the ids of the corresponding nodes
		* in the two graphs.
		*/
		std::vector<MatchingSolution> solutions;
		MatchingVisitor<VFState> *visit;
		std::atomic<uint32_t> solCount;
		bool storeSolutions;
		struct timeval fist_solution_time;

	public:
		MatchingEngine(bool storeSolutions = false): visit(NULL), 
			solCount(0), storeSolutions(storeSolutions){};

		MatchingEngine(MatchingVisitor<VFState> *visit, bool storeSolutions = false):
		 visit(visit), solCount(0), storeSolutions(storeSolutions){}

		inline size_t GetSolutionsCount() { return (size_t)solCount; }
		
		inline void GetSolutions(std::vector<MatchingSolution>& sols)
		{
			sols = solutions;
		}

		inline void EmptySolutions() {solutions.clear();}

		inline void ResetSolutionCounter()
		{
			solCount = 0;
		}

		inline struct timeval GetFirstSolutionTime()
		{
			return fist_solution_time;
		}

		inline std::string SolutionToString(MatchingSolution& sol)
		{
			MatchingSolution::iterator it;
			std::stringstream ss;
			for(it = sol.begin(); it != sol.end(); it++)
			{
				ss << it->second << "," << it->first << ":";
				/*if(it+1 != sol.end())
				{
					ss << ":";
				}*/
			}
			return ss.str();
		}

		/**
		* @brief  Finds a matching between two graph, if it exists, given the initial state of the matching process.
		* @param [in] s Initial VFState.
		* @return TRUE If the matching process finds a solution.
		* @return FALSE If the matching process doesn't find solutions.
		*/
		bool FindFirstMatching(VFState &s)
		{
			if (s.IsGoal())
			{
				solCount++;
				if(storeSolutions)
				{
					MatchingSolution sol;
					s.GetCoreSet(sol);
					solutions.push_back(sol);
				}
				if (visit)
				{
					(*visit)(s);
				}
				return true;
			}

			if (s.IsDead())
				return false;

			nodeID_t n1 = NULL_NODE, n2 = NULL_NODE;
			bool found = false;

			while (!found && s.NextPair(&n1, &n2, n1, n2))
			{
				if (s.IsFeasiblePair(n1, n2))
				{
					VFState s1(s);
					s1.AddPair(n1, n2);
					found = FindFirstMatching(s1);
				}
			}

			return found;
		}

		/**
		* @brief Visits all the matchings between two graphs, starting from state s.
		* @param [in] s Initial VFState.
		* @param [in/out] usr_data User defined parameter for the visitor.
		* @return TRUE If if the caller must stop the visit.
		* @return FALSE If if the caller must continue the visit.
		*/
		bool FindAllMatchings(VFState &s)
		{
			if (s.IsGoal())
			{
				if(!solCount)
					gettimeofday(&fist_solution_time, NULL);

				solCount++;
				if(storeSolutions)
				{
					MatchingSolution sol;
					s.GetCoreSet(sol);
					solutions.push_back(sol);
				}
				if (visit)
				{
					return (*visit)(s);
				}
				return false;
			}

			if (s.IsDead())
				return false;

			nodeID_t n1 = NULL_NODE, n2 = NULL_NODE;
			while (s.NextPair(&n1, &n2, n1, n2))
			{
				if (s.IsFeasiblePair(n1, n2))
				{
					VFState s1(s);
					s1.AddPair(n1, n2);
					if (FindAllMatchings(s1))
					{
						return true;
					}
				}
			}
			return false;
		}
	};

}
#endif
