/* Main driver program for Prophet.
   Copyright2008 DongZhaoyu, GaoBing.

This file is part of Prophet.

Prophet is free software developed based on VMIPS, you can redistribute
it and/or modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

Prophet is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with VMIPS; if not, write to the Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "speculativelogic.h"
#include <algorithm>
#include <assert.h>
#include <sstream>
#include "prophetcpustat.h"

SpeculativeLogic* SpeculativeLogic::m_Instance = NULL;

SpeculativeLogic::SpeculativeLogic() : m_CPU(NULL)
{}

SpeculativeLogic* SpeculativeLogic::GetInstance()
{
	if(m_Instance == NULL)
		m_Instance = new SpeculativeLogic();
	return m_Instance;
}

void SpeculativeLogic::SwapToSpList(SpeculativeCPU *parentcpu, SpeculativeCPU *subcpu)
{
	assert(subcpu != NULL);
	SpListIterator it;

	//delete from free list if it exist
	if(!m_FreeList.empty())
	{
		it = std::find(m_FreeList.begin(), m_FreeList.end(), subcpu);
		if(it != m_FreeList.end())
			m_FreeList.erase(it);
	}

	//test if the subcpu has already in the splist
	it = std::find(m_SpList.begin(), m_SpList.end(), subcpu);
	if(it != m_SpList.end())
	{
		pwarn<<"the subcpu has already in splist!"<<pendl;
		return;
	}

	//add the cpu to speculative list imediately after parentcpu
	if(parentcpu == NULL)
	{
		m_SpList.push_back(subcpu);	//NULL parent means just add subthread to the back of splist
										//		but why like this?
	}else{
		it = std::find(m_SpList.begin(), m_SpList.end(), parentcpu);
		if(it == m_SpList.end())
		{
			perror<<"can not find parentcpu in splist!"<<pendl;
			return;
		}
		it++;
		m_SpList.insert(it, subcpu);
	}
}

void SpeculativeLogic::SwapToFreeList(SpeculativeCPU *cpu)
{
	assert(cpu != NULL);
	SpListIterator it;

	//delete from splist if exist
	if(!m_SpList.empty())
	{
		it = std::find(m_SpList.begin(), m_SpList.end(), cpu);
		if(it != m_SpList.end())
			m_SpList.erase(it);
	}

	//test if cpu has already in free list
	it = std::find(m_FreeList.begin(), m_FreeList.end(), cpu);
	if(it != m_FreeList.end())
	{
		pwarn<<"cpu has already in free list!"<<pendl;
		return;
	}

	//add to free list
	m_FreeList.push_back(cpu);
}

SpeculativeCPU* SpeculativeLogic::GetParent(SpeculativeCPU *cpu)
{
	assert(cpu != NULL);
	SpListIterator cur = std::find(m_SpList.begin(), m_SpList.end(), cpu);

	if(cur == m_SpList.end())
	{
		pwarn<<"cpu points a spcpu which does not exist!"<<pendl;
		return NULL;
	}

	if(cur == m_SpList.begin())
		return NULL;
	--cur;
	return *cur;
}

SpeculativeCPU* SpeculativeLogic::GetSubthread(SpeculativeCPU *cpu)
{
	assert(cpu != NULL);
	SpListIterator cur = std::find(m_SpList.begin(), m_SpList.end(), cpu);

	if(cur == m_SpList.end())
	{
		pwarn<<"cpu points a spcpu which does not exist!"<<pendl;
		return NULL;
	}

	cur++;
	if(cur == m_SpList.end())
		return NULL;
	return *cur;
}

SpeculativeCPU* SpeculativeLogic::NotifySpawn(SpeculativeCPU *sender)
{
	assert(sender != NULL);
	SpListIterator it;

	if(m_FreeList.empty())
	{
		it = m_SpList.end();
		if(!m_SpList.empty())
		{
			it--;	//point to the last cpu
		}else{
			perror<<"No CPU in the simulator at all!"<<pendl;
			return NULL;
		}

		if(sender != *it){
			(*it)->OnSquash(sender, SpeculativeCPU::LOW_PRIORITY);	//squash the last thread to free the cpu

			//keyming 20131210
			// 求出被撤销的子线程的个数.
#define KEYMING_STATICS
#ifdef KEYMING_STATICS
				int cpuid = sender->getId();
				ProphetStat::ProphetCpuStat::getProCpuStatInstance()->addSquashLen(cpuid,1);
#endif
		}

	}

	if(m_FreeList.empty())
	{
		pwarn<<"No free cpu at all!"<<pendl;
		return NULL;
	}
	it = m_FreeList.begin();
	(*it)->OnSpawn(sender);
	return *it;
}

void SpeculativeLogic::SquashSubthread(SpeculativeCPU *sender, SpeculativeCPU::SQUASH_REASON reason)
{
	assert(sender != NULL);

	if(std::find(m_SpList.begin(), m_SpList.end(), sender) == m_SpList.end())
	{
		pwarn<<"the sender:"<<sender->GetID()<<" is not in splist!"<<pendl;
		return;
	}

	pinfo<<"ready to squash all subthreads of thread:"<<sender->GetID()<<pendl;

	int i = 0;
	SpListIterator it = m_SpList.end();
	while(*(--it) != sender)
	{
		(*it)->OnSquash(sender, reason);
		it = m_SpList.end();
		pinfo<<"squash "<<pendl;
		i++;
	}
//keyming 20131210
// 求出被撤销的子线程的个数.
#define KEYMING_STATICS
#ifdef KEYMING_STATICS
	int cpuid = sender->getId();
	ProphetStat::ProphetCpuStat::getProCpuStatInstance()->addSquashLen(cpuid,i);
#endif

	//m_Squash = true;
	//m_LastSquashSender = sender;
}

//return ture is success!
bool SpeculativeLogic::VerifySubthread(SpeculativeCPU *sender)
{
	assert(sender != NULL);

	SpeculativeCPU *sub = GetSubthread(sender);
	if(sub == NULL)
	{
		pinfo<<"thread "<<sender->GetID()<<"has no subthread!"<<pendl;
		return false;
	}

	return sub->RcVerification(sender);
}

bool SpeculativeLogic::PassStableToken(SpeculativeCPU *sender)
{
	assert(sender != NULL);

	SpeculativeCPU *sub = GetSubthread(sender);
	if(sub == NULL)
	{
		pinfo<<"thread "<<sender->GetID()<<"has no subthread!"<<pendl;
		return false;
	}

	sub->OnStableToken(sender);
	return true;
}

uint32 SpeculativeLogic::GetStackTop(SpeculativeCPU *cpu)
{
	uint32 min = 0xffffffff;
	uint32 top;

	for(SpListIterator it = m_SpList.begin(); it != m_SpList.end(); it++)
	{
		top = (*it)->stacktop();
		if(top < min)
			min = top;
	}
	if(min == 0xffffffff)
		perror<<"error stack top!"<<pendl;
	return min;
}

SpeculativeCPU* SpeculativeLogic::GetRunner()
{
	return m_CPU;
}

void SpeculativeLogic::Step(void)
{
	//poll each sp-cpu to process an instruction
	if(m_SpList.empty())
		return;

	static char* StateMap[] = {"UNDEFINE",
		"IDLE",
		"INITIALIZATON",
		"PRE_COMPUTATION",
		"SP_EXECUTION",
		"RESTART",
		"WAIT",
		"SQUASH",
		"STABLE_TOKEN",
		"STABLE_EXECUTION",
		"SUBTHREAD_VERIFICATION",
		"COMMIT"};
	SpListIterator it, it1;
	std::stringstream s;

	for(it = m_SpList.begin(); it != m_SpList.end(); it++)
	{
		s<<(*it)->GetID()<<":"<<StateMap[(int)((*it)->GetState())];
//keyming 1025_2013 practise
//#define DEBUG_KEYMING
#ifdef DEBUG_KEYMING
		std::cout <<"*****DEBUG_KEYMING*****" << "\t" <<  __FILE__ << ": " << __LINE__ << ": " << std::endl;
		std::cout << (*it)->GetID()<<":"<<StateMap[(int)((*it)->GetState())] << std::endl;
		std::cout << m_SpList.size() << std::endl;
#undef DEBUG_KEYMING
#endif
		it1 = it;
		it1++;
		if(it1 != m_SpList.end())
			s<<"--->";
	}
	pinfo<<"SpList: "<<s.str()<<pendl;

	it = m_SpList.end();
	for(it--; it != m_SpList.begin(); it--)
	{
		m_CPU = *it;
		(*it)->step();
	}
	m_CPU = *it;
	(*it)->step();
}

void SpeculativeLogic::Reset(void)
{
	SpListIterator it;

	//squash all speculative thread
	for(it = m_SpList.begin(); it != m_SpList.end(); it++)
		(*it)->OnSquash(NULL, SpeculativeCPU::RESET);

	//reset the first cpu of free list, 为什么只是reset第一个Freelist上的CPU
	it = m_FreeList.begin();
	(*it)->reset();
	SwapToSpList(NULL, *it);

	//keyming 1025_2013 practise
	#define DEBUG_KEYMING
	#ifdef DEBUG_KEYMING
	std::cout <<"*****DEBUG_KEYMING*****" << "\t" <<  __FILE__ << ": " << __LINE__ << ": " << std::endl;
	std::cout << "m_SpList.size() : " << SpeculativeLogic::GetInstance()->SpListSize()<< std::endl;
	std::cout << "m_freeSpList.size() : " << SpeculativeLogic::GetInstance()->FreeListSize()<< std::endl;
	#undef DEBUG_KEYMING
	#endif
}

bool SpeculativeLogic::CheckStateOnQuit()
{
	bool right;

	if(!m_SpList.empty())
	{
		SpListIterator it;
		while(!m_SpList.empty())
		{
			it = m_SpList.begin();
			(*it)->OnSquash(NULL, SpeculativeCPU::QUIT);
		}
		pwarn<<"there are still speculative threads when quit!"<<pendl;
		right = false;
	}else{
		pinfo<<"quit normally!"<<pendl;
		right = true;
	}
	return right;
}

//keyming 1025_2013 by test
size_t
SpeculativeLogic::FreeListSize()
{
	return m_FreeList.size();
}
size_t
SpeculativeLogic::SpListSize()
{
	return m_SpList.size();
}


//keyming 1126_2013
// 返回cpu在推测链表里的位置， 倒数的位置。 如在最后 则返回-1.
int
SpeculativeLogic::PosInSpList(SpeculativeCPU* sender, SpeculativeCPU* cpu)
{
	assert(cpu != NULL);
	if(NULL == sender){
		return (0);
	}

	typedef std::list<SpeculativeCPU*>::const_reverse_iterator RSpListIter;
	int subpos = 0, sendpos = 0, i = 0;
	RSpListIter rit;
	for(rit = this->m_SpList.rbegin(), i = 0; rit != this->m_SpList.rend(); rit++, i++){
		if(*rit == cpu){
			subpos = i;
		}
		else if(*rit == sender){
			sendpos = i;
			break;
		}
	}
	assert(sendpos > subpos && subpos >= 0 );
	assert(rit != this->m_SpList.rend());
	return (sendpos - subpos);
}


int
SpeculativeLogic::getSpDegree(SpeculativeCPU *cpu)
{
	assert(cpu!=NULL);
	int i = 0;
	for(SpListConstIterator citer = this->m_SpList.begin(); citer != this->m_SpList.end(); citer++){
		if(cpu == *citer){
			return i;
		}
		i++;
	}
	assert(0);
}

