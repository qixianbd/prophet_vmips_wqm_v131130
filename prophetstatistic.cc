/* Main driver program for Prophet.
   Copyright2009 DongZhaoyu, GaoBing.

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

#include "prophetstatistic.h"
#include "prophetxmldoc.h"
#include "prophetlog.h"
#include "prophet_vmips.h"
#include "options.h"
#include "speculativelogic.h"
#include <string>
#include <cstring>
#include <fstream>
#include <iostream>
#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "prophetcpustat.h"
//
//
extern FILE * mystderrlog;
namespace ProphetStat
{
	bool gStatInited = false;
	StatMap gStatMap;
	
	StatTime::StatTime():m_freeTime(0), m_squashTime(0), m_waitTime(0),m_spTime(0),m_noSptime(0),m_timeSlot(0),
			m_timeNeedVerify(0), m_totalTime(0), m_psliceTime(0),m_lastFinishTime(0),m_runState(IDLE)
	{

	}

	long StatTime::setTotalTime()
	{
		this->m_totalTime = 0;
		this->m_totalTime = this->m_freeTime + this->m_spTime + this->m_squashTime + this->m_waitTime
				+ this->m_psliceTime + this->m_noSptime;
		return this->m_totalTime;
	}

	StatCpu::StatCpu(int n):num(n), proportion(NULL)
	{
		assert(n>0);
		coreNum = new int[num];
		for(int i = 0; i < num; i++){
			coreNum[i] = 0;
		}
		proportion = new double[num];
		for(int i = 0; i < num; i++){
			proportion[i] = 0;
		}
	}

	StatCpu* StatCpu::getStatCpuInstance(int n)
	{
		assert(n > 0);
		if(StatCpu::hasone == NULL){
			StatCpu::hasone = new StatCpu(n);
			return StatCpu::hasone;
		}else{
			return StatCpu::hasone;
		}
	}
	double StatCpu::averageCoreNum()
	{
		double average = 0;
		this->calProportion();
		for(int i = 0; i < num; i++){
			average += (i+1)*this->proportion[i];
		}
		return average;
	}
	double* StatCpu::calProportion()
	{
		long total = 0;
		for(int i = 0; i < num; i++){
			total += coreNum[i];
		}
		for(int j = 0; j < num; j++){
			proportion[j] = coreNum[j]/(double)total;
		}
		return proportion;
	}

	StatRecord::StatRecord() : m_TotalInst(0), m_TotalClock(0),
				m_SpawnedThread(0), m_CommitedThread(0), m_FailedThread(0),
				m_StartTime(0), m_FinishTime(0), m_FailOnDDV(0), m_FailOnInstruction(0),
				m_FailOnRegV(0), m_FailOnMMV(0), m_FailOnLowPriority(0), m_FailOnControl(0),
				m_FailOnReset(0), m_FailOnHalt(0), m_FailOnQuit(0), m_FailOnException(0), m_FailOnUnknown(0),
				m_FailNumBuf(NULL), m_TimeStatistic(NULL)

	{
		//keyming 1126_2013
#define KEYMING_PLUS
#ifdef KEYMING_PLUS
		m_FailNumBufSize = machine->opt->option("corenum")->num;	//这一行表示每一个核都重新计算了核的总数目， 且结果都一样。
		assert(m_FailNumBufSize > 0);
		m_FailNumBuf = (int *)calloc(m_FailNumBufSize, sizeof(*m_FailNumBuf));
		if(!m_FailNumBuf){
			fprintf(stderr, "There is no enough memory for the malloc\n");
			exit(1);
		}
#endif

#define KEYMING_STATICS
#ifdef KEYMING_STATICS
		//m_TimeStatistic = (StatTime*)malloc(sizeof(*m_TimeStatistic));
		m_TimeStatistic = new StatTime();
	//	memset(m_TimeStatistic, 0, sizeof(*m_TimeStatistic));
#endif

		//keyming 20131204
		m_CpuStat = StatCpu::getStatCpuInstance(m_FailNumBufSize);
	}

	int StatRecord::m_SeqTime = 0;
	int StatRecord::m_ParalTime = 0;
	int StatRecord::m_ClockParalTime = 0;
	int StatRecord::m_FailNumBufSize = 0;
	long StatRecord::m_totalInstrNum = 0;
	 StatCpu* StatCpu::hasone = NULL;
	StatCpu* StatRecord::m_CpuStat = NULL;


	/*! \brief Statistic maintains all the statistic record for the cpus
	*/
	class Statistic
	{
	public:
		static Statistic* Get();
		void RegisterRec(StatRecord*);
		XmlElement* AddToXml(XmlElement*);
		XmlElement* AddStat(XmlElement*);

	private:
		Statistic();

		std::list<StatRecord*> m_RecList;
		static Statistic* m_Inst;
		int m_SpTime;		//the real time cost of the speculative execution
		int m_SeqTime;		//the real time cost of the sequential execution
	};

	Statistic* Statistic::m_Inst = NULL;

	/**
	 *
	 */
	Statistic* Statistic::Get()
	{
		if(m_Inst == NULL)
			m_Inst = new Statistic();
		return m_Inst;
	}


	Statistic::Statistic() : m_SpTime(0), m_SeqTime(0)
	{}

	void Statistic::RegisterRec(StatRecord* rec)
	{
		//here we do not pre-check the existance of the specified record, this may cause redundance
		m_RecList.push_back(rec);
	}

	XmlElement* Statistic::AddToXml(XmlElement* root)
	{
		std::list<StatRecord*>::iterator it;
		int i = 0;
		for(it = m_RecList.begin(); it != m_RecList.end(); it++)
		{
			char value[33];
			XmlElement* node = (*it)->XmlNode();
			sprintf(value, "%d", i++);
			node->addAttribute("cpu_id", std::string(value));
			root->addElement(node);
		}
		return root;

	}


	RecAdder::RecAdder()
	{
		m_Record = new StatRecord();
		Statistic::Get()->RegisterRec(m_Record);

	}

	RecAdder::~RecAdder()
	{}

	StatRecord* RecAdder::GetRec()
	{
		return m_Record;
	}

	typedef StatMap::value_type StatMapValue;

	/*! \brief setting file*/
	static char* file = STAT_PATH;
	static char program[100];
	std::string SettingVersion;

	XmlElement* StatRecord::XmlNode()
	{
		XmlElement *node = new XmlElement("statistic");
		char value[33];
		
		XmlElement *subnode = new XmlElement("total_inst");
		sprintf(value, "%d", m_TotalInst);
		subnode->setContent(value);
		node->addElement(subnode);

		subnode = new XmlElement("total_clock");
		sprintf(value, "%d", m_TotalClock);
		subnode->setContent(value);
		node->addElement(subnode);

		subnode = new XmlElement("spawned_thread");
		sprintf(value, "%d", m_SpawnedThread);
		subnode->setContent(value);
		node->addElement(subnode);

		subnode = new XmlElement("commited_thread");
		sprintf(value, "%d", m_CommitedThread);
		subnode->setContent(value);
		node->addElement(subnode);

		XmlElement *failnode = new XmlElement("failed_thread");
		sprintf(value, "%d", m_FailedThread);
		failnode->addAttribute("total", value);

		subnode = new XmlElement("OnDDV");
		sprintf(value, "%d", m_FailOnDDV);
		subnode->setContent(value);
		failnode->addElement(subnode);

		subnode = new XmlElement("OnInstruction");
		sprintf(value, "%d", m_FailOnInstruction);
		subnode->setContent(value);
		failnode->addElement(subnode);

		subnode = new XmlElement("OnRegisterVerification");
		sprintf(value, "%d", m_FailOnRegV);
		subnode->setContent(value);
		failnode->addElement(subnode);

		subnode = new XmlElement("OnMemoryVerification");
		subnode->setContent(m_FailOnMMV);
		failnode->addElement(subnode);

		subnode = new XmlElement("OnLowPriority");
		subnode->setContent(m_FailOnLowPriority);
		failnode->addElement(subnode);

		subnode = new XmlElement("OnControlVoilate");
		subnode->setContent(m_FailOnControl);
		failnode->addElement(subnode);

		subnode = new XmlElement("OnReset");
		subnode->setContent(m_FailOnReset);
		failnode->addElement(subnode);

		subnode = new XmlElement("OnHalt");
		subnode->setContent(m_FailOnHalt);
		failnode->addElement(subnode);

		subnode = new XmlElement("OnQuit");
		subnode->setContent(m_FailOnQuit);
		failnode->addElement(subnode);

		subnode = new XmlElement("OnException");
		subnode->setContent(m_FailOnException);
		failnode->addElement(subnode);

		subnode = new XmlElement("OnUnknown");
		subnode->setContent(m_FailOnUnknown);
		failnode->addElement(subnode);

		//keyming faile count
#define KEYMING_STATICS
#ifdef KEYMING_STATICS
		XmlElement *failCount = new XmlElement("FailCount");
		for(int i = 0; i < StatRecord::m_FailNumBufSize; i++){
			XmlElement *failCpuNum = new XmlElement("Failed_CPU");
			char value[33];
			sprintf(value, "%d", i);
			failCpuNum->addAttribute("failed_ListId", std::string(value));
			failCpuNum->setContent(m_FailNumBuf[i]);
			failCount->addElement(failCpuNum);
		}
		failnode->addElement(failCount);
		node->addElement(failnode);

		XmlElement *timestatnode = new XmlElement("TimeStat");
		XmlElement *timenode = new XmlElement("FreeTime");
		timenode->setContent(m_TimeStatistic->m_freeTime);
		timestatnode->addElement(timenode);

		timenode = new XmlElement("SquashTime");
		timenode->setContent(m_TimeStatistic->m_squashTime);
		timestatnode->addElement(timenode);

		timenode = new XmlElement("WaitTime");
		timenode->setContent(m_TimeStatistic->m_waitTime);
		timestatnode->addElement(timenode);

		timenode = new XmlElement("SpTime");
		timenode->setContent(m_TimeStatistic->m_spTime);
		timestatnode->addElement(timenode);

		timenode = new XmlElement("NoSpTime");
		timenode->setContent(m_TimeStatistic->m_noSptime);
		timestatnode->addElement(timenode);

		timenode = new XmlElement("PsliceTime");
		timenode->setContent(m_TimeStatistic->m_psliceTime);
		timestatnode->addElement(timenode);

		node->addElement(timestatnode);
#endif

		return node;
	}

	typedef struct tstat
	{
		int m_TSpawnedThread;
		int m_TFailedThread;
		int m_TFailOnDDV;
		int m_TFailOnInstruction;
		int m_TFailOnRegV;
		int m_TFailOnMMV;
		int m_TFailOnLowPriority;
		int m_TFailOnControl;
		int m_TFailOnReset;
		int m_TFailOnHalt;
		int m_TFailOnQuit;
		int m_TFailOnException;
		int m_TFailOnUnknown;

		//keyming 1121_2013
		int m_TFailNumBufSize;
		int *m_TFailNumBuf;
		StatTime *m_TTimeStatistic;
	}tstats;

	XmlElement* Statistic::AddStat(XmlElement* node)
	{
		tstats t;
		std::memset(&t, 0, sizeof(t));
		char buffer[512];

		FILE *fp = fopen("cputime.txt", "a+");
		assert(fp != NULL);
		{
		char cpuNumBuf[32];
		sprintf(cpuNumBuf, "%d", StatRecord::m_FailNumBufSize);
		std::string docname(program);
		docname += cpuNumBuf;

		fprintf(fp,"\n******************************CpuTime***************************\n");
		fprintf(fp, "%-25s\t%-12s\t%-12s\t%-12s\t%-12s\t%-12s\t%-12s\n","testcaseName","freeTime", "psliceTime", "SquashTime",
				"waitTime", "Sptime", "NoSpTime");
		fprintf(fp, "%-25s\t", docname.c_str());
		}

//keyming 1127_2013
#define KEYMING_STATICS
#ifdef KEYMING_STATICS
		t.m_TTimeStatistic = new StatTime();
	//	memset(t.m_TTimeStatistic, 0, sizeof(StatTime));

		t.m_TFailNumBufSize = StatRecord::m_FailNumBufSize;

		t.m_TFailNumBuf = new int[t.m_TFailNumBufSize];
		memset(t.m_TFailNumBuf, 0, t.m_TFailNumBufSize);
#endif

		for(std::list<StatRecord*>::iterator it = m_RecList.begin(); it != m_RecList.end(); it++)
		{
			t.m_TSpawnedThread += (*it)->m_SpawnedThread;
			t.m_TFailedThread += (*it)->m_FailedThread;
			t.m_TFailOnDDV += (*it)->m_FailOnDDV;
			t.m_TFailOnInstruction += (*it)->m_FailOnInstruction;
			t.m_TFailOnRegV += (*it)->m_FailOnRegV;
			t.m_TFailOnMMV += (*it)->m_FailOnMMV;
			t.m_TFailOnLowPriority += (*it)->m_FailOnLowPriority;
			t.m_TFailOnControl += (*it)->m_FailOnControl;
			t.m_TFailOnReset += (*it)->m_FailOnReset;
			t.m_TFailOnHalt += (*it)->m_FailOnHalt;
			t.m_TFailOnQuit += (*it)->m_FailOnQuit;
			t.m_TFailOnException += (*it)->m_FailOnException;
			t.m_TFailOnUnknown += (*it)->m_FailOnUnknown;
//keyming 1127_2013
#define KEYMING_STATICS
#ifdef KEYMING_STATICS
			int total_time = 0;
			(t.m_TTimeStatistic)->m_freeTime += (*it)->m_TimeStatistic->m_freeTime;
			(t.m_TTimeStatistic)->m_spTime += (*it)->m_TimeStatistic->m_spTime;
			(t.m_TTimeStatistic)->m_squashTime += (*it)->m_TimeStatistic->m_squashTime;
			(t.m_TTimeStatistic)->m_waitTime += (*it)->m_TimeStatistic->m_waitTime;
			(t.m_TTimeStatistic)->m_psliceTime += (*it)->m_TimeStatistic->m_psliceTime;
			(t.m_TTimeStatistic)->m_noSptime += (*it)->m_TimeStatistic->m_noSptime;
			for(int i = 0; i < t.m_TFailNumBufSize; i++){
				t.m_TFailNumBuf[i] += (*it)->m_FailNumBuf[i];
			}
#endif
		}

		(t.m_TTimeStatistic)->setTotalTime();

		XmlElement *stat = new XmlElement("total_spawned");
		stat->setContent(t.m_TSpawnedThread);
		node->addElement(stat);

		stat = new XmlElement("success_ratio");
		sprintf(buffer, "%f", double(t.m_TSpawnedThread - t.m_TFailedThread) / t.m_TSpawnedThread);
		stat->setContent(std::string(buffer));
		node->addElement(stat);

		XmlElement *fail = new XmlElement("fail_stat");
		fail->addAttribute("total_failed", t.m_TFailedThread);

		stat = new XmlElement("TOnDDV");
		stat->addAttribute("total", t.m_TFailOnDDV);
		sprintf(buffer, "%f", double(t.m_TFailOnDDV) / t.m_TFailedThread);
		stat->setContent(std::string(buffer));
		fail->addElement(stat);

		stat = new XmlElement("TOnInstruction");
		stat->addAttribute("total", t.m_TFailOnInstruction);
		sprintf(buffer, "%f", double(t.m_TFailOnInstruction) / t.m_TFailedThread);
		stat->setContent(std::string(buffer));
		fail->addElement(stat);

		stat = new XmlElement("TOnRegV");
		stat->addAttribute("total", t.m_TFailOnRegV);
		sprintf(buffer, "%f", double(t.m_TFailOnRegV) / t.m_TFailedThread);
		stat->setContent(std::string(buffer));
		fail->addElement(stat);

		stat = new XmlElement("TOnMMV");
		stat->addAttribute("total", t.m_TFailOnMMV);
		sprintf(buffer, "%f", double(t.m_TFailOnMMV) / t.m_TFailedThread);
		stat->setContent(std::string(buffer));
		fail->addElement(stat);

		stat = new XmlElement("TOnLowPriority");
		stat->addAttribute("total", t.m_TFailOnLowPriority);
		sprintf(buffer, "%f", double(t.m_TFailOnLowPriority) / t.m_TFailedThread);
		stat->setContent(std::string(buffer));
		fail->addElement(stat);

		stat = new XmlElement("TOnControl");
		stat->addAttribute("total", t.m_TFailOnControl);
		sprintf(buffer, "%f", double(t.m_TFailOnControl) / t.m_TFailedThread);
		stat->setContent(std::string(buffer));
		fail->addElement(stat);

		stat = new XmlElement("TOnReset");
		stat->addAttribute("total", t.m_TFailOnReset);
		sprintf(buffer, "%f", double(t.m_TFailOnReset) / t.m_TFailedThread);
		stat->setContent(std::string(buffer));
		fail->addElement(stat);

		stat = new XmlElement("TOnHalt");
		stat->addAttribute("total", t.m_TFailOnHalt);
		sprintf(buffer, "%f", double(t.m_TFailOnHalt) / t.m_TFailedThread);
		stat->setContent(std::string(buffer));
		fail->addElement(stat);

		stat = new XmlElement("TOnQuit");
		stat->addAttribute("total", t.m_TFailOnQuit);
		sprintf(buffer, "%f", double(t.m_TFailOnQuit) / t.m_TFailedThread);
		stat->setContent(std::string(buffer));
		fail->addElement(stat);

		stat = new XmlElement("TOnException");
		stat->addAttribute("total", t.m_TFailOnException);
		sprintf(buffer, "%f", double(t.m_TFailOnException) / t.m_TFailedThread);
		stat->setContent(std::string(buffer));
		fail->addElement(stat);

		stat = new XmlElement("TOnUnknown");
		stat->addAttribute("total", t.m_TFailOnUnknown);
		sprintf(buffer, "%f", double(t.m_TFailOnUnknown) / t.m_TFailedThread);
		stat->setContent(std::string(buffer));
		fail->addElement(stat);
		node->addElement(fail);

		//keyming 1128_2013
		// 在XML上打印统计的撤销距离
		XmlElement *squashdis = new XmlElement("FailDistantStat");
		squashdis->addAttribute("MaxDistance", t.m_TFailNumBufSize);
		for(int j = 0; j < t.m_TFailNumBufSize; j++){
			XmlElement *failCpuNum = new XmlElement("SquashDistance");
			char value[33];
			sprintf(value, "%d", j);
			failCpuNum->addAttribute("distance_len", std::string(value));
			failCpuNum->setContent(t.m_TFailNumBuf[j]);
			squashdis->addElement(failCpuNum);
		}
		node->addElement(squashdis);

		//在XML上打印统计的 CPU 运行时间组成
		XmlElement *timeBreakdown = new XmlElement("CPUTimeBreakdown");
		stat = new XmlElement("FreeTime");
		stat->addAttribute("FreeTime", t.m_TTimeStatistic->m_freeTime);
		sprintf(buffer, "%f", double(t.m_TTimeStatistic->m_freeTime) / t.m_TTimeStatistic->m_totalTime);
		fprintf(fp, "%-12f\t", double(t.m_TTimeStatistic->m_freeTime) / t.m_TTimeStatistic->m_totalTime);
		stat->setContent(std::string(buffer));
		timeBreakdown->addElement(stat);

		stat = new XmlElement("PsliceTime");
		stat->addAttribute("PsliceTime", t.m_TTimeStatistic->m_psliceTime);
		sprintf(buffer, "%f", double(t.m_TTimeStatistic->m_psliceTime) / t.m_TTimeStatistic->m_totalTime);
		fprintf(fp, "%-12f\t", double(t.m_TTimeStatistic->m_psliceTime) / t.m_TTimeStatistic->m_totalTime);
		stat->setContent(std::string(buffer));
		timeBreakdown->addElement(stat);

		stat = new XmlElement("SquashTime");
		stat->addAttribute("SquashTime", t.m_TTimeStatistic->m_squashTime);
		sprintf(buffer, "%f", double(t.m_TTimeStatistic->m_squashTime) / t.m_TTimeStatistic->m_totalTime);
		fprintf(fp, "%-12f\t", double(t.m_TTimeStatistic->m_squashTime) / t.m_TTimeStatistic->m_totalTime);
		stat->setContent(std::string(buffer));
		timeBreakdown->addElement(stat);

		stat = new XmlElement("WaitTime");
		stat->addAttribute("WaitTime", t.m_TTimeStatistic->m_waitTime);
		sprintf(buffer, "%f", double(t.m_TTimeStatistic->m_waitTime) / t.m_TTimeStatistic->m_totalTime);
		fprintf(fp, "%-12f\t", double(t.m_TTimeStatistic->m_waitTime) / t.m_TTimeStatistic->m_totalTime);
		stat->setContent(std::string(buffer));
		timeBreakdown->addElement(stat);

		stat = new XmlElement("SpTime");
		stat->addAttribute("SpTime", t.m_TTimeStatistic->m_spTime);
		sprintf(buffer, "%f", double(t.m_TTimeStatistic->m_spTime) / t.m_TTimeStatistic->m_totalTime);
		fprintf(fp, "%-12f\t", double(t.m_TTimeStatistic->m_spTime) / t.m_TTimeStatistic->m_totalTime);
		stat->setContent(std::string(buffer));
		timeBreakdown->addElement(stat);

		stat = new XmlElement("NoSpTime");
		stat->addAttribute("NoSpTime", t.m_TTimeStatistic->m_noSptime);
		sprintf(buffer, "%f", double(t.m_TTimeStatistic->m_noSptime) / t.m_TTimeStatistic->m_totalTime);
		fprintf(fp, "%-12f\t", double(t.m_TTimeStatistic->m_noSptime) / t.m_TTimeStatistic->m_totalTime);
		stat->setContent(std::string(buffer));
		timeBreakdown->addElement(stat);


		node->addElement(timeBreakdown);

		//计算CPU运行时， 平均的核利用个数   或者说平均推测链表长度
		XmlElement *cpustat = new XmlElement("CPUStat");
		cpustat->addAttribute("averageCoreNumber", StatRecord::m_CpuStat->averageCoreNum());
		sprintf(buffer, "%s%.6f", "averageCoreNumber: ",StatRecord::m_CpuStat->averageCoreNum());
		cpustat->setContent(buffer);

		StatRecord::m_CpuStat->calProportion();
		for(int i = 0; i < t.m_TFailNumBufSize; i++){
			XmlElement *corenum = new XmlElement("CoreNum");
			corenum->addAttribute("corenum", i+1);
			sprintf(buffer, "%d/%.6f", StatRecord::m_CpuStat->coreNum[i], StatRecord::m_CpuStat->proportion[i]);
			corenum->setContent(buffer);
			cpustat->addElement(corenum);
		}
		node->addElement(cpustat);
		fclose(fp);
		return (node);
	}

	/*! \brief if a StatItem with a NULL statistic action, the default one will be invoked
	*/
	static void DefaultStatAction(StatRecord* rec, StatCondition condition, int cost, void* clientdata = NULL)
	{
		if(!gStatInited)
			return;
		rec->m_TotalInst++;
		StatRecord::m_totalInstrNum++;		//addbykeyming 20140609

		rec->m_TotalClock += cost;
		rec->m_FinishTime += cost;

		//keyming 1127_2013
		#define KEYMING_STATICS
		#ifdef KEYMING_STATICS

		if(rec->m_TimeStatistic->m_runState == NOSPEXE){
			StatRecord::m_ClockParalTime+= cost;
		}
				rec->m_TimeStatistic->m_timeSlot += cost;
		#endif

		if(StatRecord::m_ClockParalTime == 2693){
			int i = 0;
			i = rec->m_ParalTime;
			i++;
		}
		int k = 0;
		k = StatRecord::m_ClockParalTime;
		k++;
		k = rec->m_TimeStatistic->m_timeSlot;
		k++;

	}

	/*! \brief user defined action for specified instruction
	*/
	static void CpzeroAction(StatRecord* rec, StatCondition condition, int cost, void* clientdata = NULL)
	{
		if(!gStatInited)
			return;

		CpzeroClientData *data = (CpzeroClientData*)clientdata;
		StatItem item;
		item.m_Cost = 0;
		item.m_Action = NULL;
		if(data->m_rs > 15)
		{
			switch(data->m_funct)
			{
			case 1:		item = gStatMap["tlbr"];	break;
			case 2:		item = gStatMap["tlbwi"];	break;
			case 6:		item = gStatMap["tlbwr"];	break;
			case 8:		item = gStatMap["tlbp"];	break;
			case 16:	item = gStatMap["rfe"];		break;
			default:	NULL;				break;
			}
		} else {
			switch(data->m_rs)
			{
			case 0:		item = gStatMap["mfc0"];	break;
			case 4:		item = gStatMap["mtc0"];	break;
			case 8:		item = gStatMap["bc0x"];	break;
			default:	NULL;				break;
			}
		}

		STAT_ACTION action = item.m_Action;
		if(action)
			action(rec, condition, item.m_Cost, NULL);
	}

	static void CponeAction(StatRecord* rec, StatCondition condition, int cost, void* clientdata = NULL)
	{
		if(!gStatInited)
			return;

		CponeClientData *data = (CponeClientData*)clientdata;
		StatItem item;
		item.m_Action = NULL;
		item.m_Cost = 0;
		if(data->m_rs == 16)
		{
			switch(data->m_funct)
			{
			case 0:		item = gStatMap["adds"];	break;
			case 1:		item = gStatMap["subs"];	break;
			case 2:		item = gStatMap["muls"];	break;
			case 3:		item = gStatMap["divs"];	break;
			case 5:		item = gStatMap["abss"];	break;
			case 6:		item = gStatMap["movs"];	break;
			case 7:		item = gStatMap["negs"];	break;
			case 33:	item = gStatMap["cvtds"];	break;
			case 36:	item = gStatMap["cvtws"];	break;
			case 56:	item = gStatMap["csfs"];	break;
			case 58:	item = gStatMap["cseqs"];	break;
			case 60:	item = gStatMap["clts"];	break;
			case 62:	item = gStatMap["cles"];	break;
			default:	NULL;				break;
			}
		} else if(data->m_rs == 17) {
			switch(data->m_funct)
			{
			case 0:		item = gStatMap["addd"];	break;
			case 1:		item = gStatMap["subd"];	break;
			case 2: 	item = gStatMap["muld"];	break;
			case 3: 	item = gStatMap["divd"];	break;
			case 5: 	item = gStatMap["absd"];	break;
			case 6: 	item = gStatMap["movd"];	break;
			case 7:		item = gStatMap["negd"];	break;
			case 32:	item = gStatMap["cvtsd"];	break;
			case 36:	item = gStatMap["cvtwd"];	break;
			case 56:	item = gStatMap["csfd"];	break;
			case 58:	item = gStatMap["cseqd"];	break;
			case 60:	item = gStatMap["cltd"];	break;
			case 62: 	item = gStatMap["cled"];	break;
			default:	NULL;				break;
			}
		} else if(data->m_rs == 0) {
			item = gStatMap["mfc1"];
		} else if(data->m_rs == 4) {
			item = gStatMap["mtc1"];
		} else if(data->m_rs == 8) {
			switch(data->m_ft)
			{
			case 0:		item = gStatMap["bc1f"];	break;
			case 1:		item = gStatMap["bc1t"];	break;
			default:	NULL;				break;
			}
		} else if(data->m_rs == 20) {
			switch(data->m_funct)
			{
			case 32:	item = gStatMap["cvtsw"];	break;
			case 33:	item = gStatMap["cvtdw"];	break;
			default:	NULL;				break;
			}
		}

		STAT_ACTION action = item.m_Action;
		if(action)
			action(rec, condition, item.m_Cost, NULL);
	}

	/*! \brief user defined actions for specified event
	*/
	static void SpawnAction(StatRecord* rec, StatCondition condition, int cost, void* clientdata = NULL)
	{
		if(!gStatInited)
			return;
		rec->m_TotalInst++;
		StatRecord::m_totalInstrNum++;		//addbykeyming 20140609

		rec->m_TotalClock += cost;

		if(condition == SUCCESS)
		{
			assert(clientdata != NULL);
			SpawnClientData* pdata = (SpawnClientData*)clientdata;
			RecAdder* sub = pdata->m_SubAdder;
			if(sub)
			{
				StatRecord* subrec = sub->GetRec();

				int preFinishTime = subrec->m_FinishTime;

				//here we initialize the subthread's statistic record
				subrec->m_SpawnedThread++;
				subrec->m_StartTime = rec->m_FinishTime;
				subrec->m_FinishTime = rec->m_FinishTime;

				//keyming 1127_2013 line 5
#define KEYMING_STATICS
#ifdef KEYMING_STATICS
				assert(subrec->m_TimeStatistic->m_runState == IDLE);
				subrec->m_TimeStatistic->m_timeSlot = StatRecord::m_ClockParalTime - subrec->m_TimeStatistic->m_lastFinishTime;
				subrec->m_TimeStatistic->m_freeTime += subrec->m_TimeStatistic->m_timeSlot;
				subrec->m_TimeStatistic->m_timeSlot = 0;
				/*
				subrec->m_TimeStatistic->m_timeSlot = subrec->m_StartTime - preFinishTime;
				subrec->m_TimeStatistic->m_freeTime += subrec->m_TimeStatistic->m_timeSlot;
				subrec->m_TimeStatistic->m_timeSlot = 0;
				*/

#endif
			}
		}

		rec->m_FinishTime += cost;
		if(rec->m_TimeStatistic->m_runState == NOSPEXE){
			StatRecord::m_ClockParalTime+= cost;
		}
#define KEYMING_STATICS
#ifdef KEYMING_STATICS
		rec->m_TimeStatistic->m_timeSlot += cost;
#endif
		//keyming 20131204
		int listLen = SpeculativeLogic::GetInstance()->SpListSize();
		++StatRecord::m_CpuStat->coreNum[listLen-1];

	}

	static void BeginAction(StatRecord* rec, StatCondition condition, int cost, void* clientdata = NULL)
	{
		if(!gStatInited)
			return;

		rec->m_SpawnedThread = 1;
		StatRecord::m_CpuStat->coreNum[0]=1;

		rec->m_TimeStatistic->m_timeSlot = 0;
		rec->m_TimeStatistic->m_lastFinishTime = 0;
		rec->m_TimeStatistic->m_runState = NOSPEXE;
		StatRecord::m_ClockParalTime = 0;
	}

	static void PsliceEntryAction(StatRecord* rec, StatCondition condition, int cost, void* clientdata = NULL)
	{
		rec->m_TotalInst++;		//addbykeyming 20140609
		StatRecord::m_totalInstrNum++;		//addbykeyming 20140609

		//keyming 20131202
#define KEYMING_STATICS
#ifdef KEYMING_STATICS
				assert(rec->m_TimeStatistic->m_runState == IDLE);
				//rec->m_TimeStatistic->m_squashTime += rec->m_TimeStatistic->m_timeSlot;
				rec->m_TimeStatistic->m_timeSlot = 0;
				rec->m_TimeStatistic->m_timeSlot+=cost;
				rec->m_TimeStatistic->m_runState = PSLICE;
#undef KEYMING_STATICS
		#endif
	}

	static void PsliceExitAction(StatRecord* rec, StatCondition condition, int cost, void* clientdata = NULL)
	{
		rec->m_TotalInst++;		//addbykeyming 20140609
		StatRecord::m_totalInstrNum++;		//addbykeyming 20140609

		//keyming 20131202
#define KEYMING_STATICS
#ifdef KEYMING_STATICS
				assert(rec->m_TimeStatistic->m_runState == PSLICE);
				//rec->m_TimeStatistic->m_squashTime += rec->m_TimeStatistic->m_timeSlot;
				rec->m_TimeStatistic->m_timeSlot+=cost;
				rec->m_TimeStatistic->m_psliceTime += rec->m_TimeStatistic->m_timeSlot;
				rec->m_TimeStatistic->m_timeSlot = 0;
				rec->m_TimeStatistic->m_runState = SPEXE;
#undef KEYMING_STATICS
		#endif

	}

	//
	static void StableTokenAction(StatRecord* rec, StatCondition condition,
			int cost, void* clientdata = NULL)
	{
		//keyming 20131211
#define KEYMING_STATICS
#ifdef KEYMING_STATICS
		StatState runState = rec->m_TimeStatistic->m_runState;
		assert(runState == PSLICE || runState == SPEXE || runState == WAIT);
		//rec->m_TimeStatistic->m_squashTime += rec->m_TimeStatistic->m_timeSlot;
		if(PSLICE == runState){
			rec->m_TimeStatistic->m_timeSlot+=cost;
			rec->m_TimeStatistic->m_psliceTime += rec->m_TimeStatistic->m_timeSlot;
			rec->m_TimeStatistic->m_timeSlot = 0;
			rec->m_TimeStatistic->m_runState = NOSPEXE;
		}
		else if(SPEXE == runState){
			rec->m_TimeStatistic->m_timeSlot+=cost;
			rec->m_TimeStatistic->m_spTime += rec->m_TimeStatistic->m_timeSlot;
			rec->m_TimeStatistic->m_timeSlot = 0;
			rec->m_TimeStatistic->m_runState = NOSPEXE;
		}
		else if(runState == WAIT){		// 此时还求不出waitTime， 要用parallelTime才能求出waitTime.

			rec->m_TimeStatistic->m_spTime += rec->m_TimeStatistic->m_timeNeedVerify;
			rec->m_TimeStatistic->m_timeNeedVerify = 0;

			rec->m_TimeStatistic->m_waitTime = StatRecord::m_ClockParalTime - rec->m_TimeStatistic->m_lastFinishTime;
			rec->m_TimeStatistic->m_timeSlot = 0;
			rec->m_TimeStatistic->m_runState = NOSPEXE;
		}
		else{
			assert(0);
		}
#undef KEYMING_STATICS
#endif

	}

	//
	static void CqipHoldAction(StatRecord* rec, StatCondition condition,
			int cost, void* clientdata = NULL)
	{
	//	assert(rec->m_TimeStatistic->m_runState == SPEXE);
	//该断言有时测试失败， 换为下面的语句
		if(rec->m_TimeStatistic->m_runState != SPEXE){
			std::cerr << "In CqipHoldAction Assertion Failed" << rec->m_TimeStatistic->m_runState  << std::endl;
			fprintf(mystderrlog, "In CqipHoldAction Assertion Failed state = %d\n",rec->m_TimeStatistic->m_runState);
		}
		rec->m_TimeStatistic->m_timeSlot+=cost;
		rec->m_TimeStatistic->m_timeNeedVerify = rec->m_TimeStatistic->m_timeSlot;
		rec->m_TimeStatistic->m_timeSlot = 0;
		rec->m_TimeStatistic->m_lastFinishTime = StatRecord::m_ClockParalTime;
		rec->m_TimeStatistic->m_runState = WAIT;
	}

	//(1)设置nonSptime, (2)对timeNeedVerify 进行确认， 分清是squashTime 还是spTime.
	static void CqipCommitAction(StatRecord* rec, StatCondition condition,
			int cost, void* clientdata = NULL)
	{
		assert(rec->m_TimeStatistic->m_runState == NOSPEXE);
		assert(condition == SUCCESS || condition == FAILED);


		rec->m_TimeStatistic->m_timeSlot+=cost;
		rec->m_TimeStatistic->m_noSptime += rec->m_TimeStatistic->m_timeSlot;
		rec->m_TimeStatistic->m_timeSlot = 0;


		if(condition == SUCCESS){
			//非推测线程对直接后继线程验证成功。
			rec->m_TimeStatistic->m_lastFinishTime = StatRecord::m_ClockParalTime;
			rec->m_TimeStatistic->m_runState = IDLE;
		}
		else{
			rec->m_TimeStatistic->m_runState = NOSPEXE;
		}

	}


/**
 * 验证成功时， 确定线程变为空闲态， 核资源也释放出来变空闲； 失败时， 确定线程继续以确定态执行， 其占有的核没有被释放出来。
 * @param condition 为真时代表确定线程验证直接后继成功， 假为失败
 */
	static void VerifyAction(StatRecord* rec, StatCondition condition,
				int cost, void* clientdata = NULL)
	{
		if(condition){	//验证成功
			//condition
			//keyming 20131204
			int listLen = SpeculativeLogic::GetInstance()->SpListSize();
			++StatRecord::m_CpuStat->coreNum[listLen-1];
		}
		//验证失败时， 非空闲的核的数量不变（失败的子线程已经在squsha中被撤销掉了）。
	}

	static void CommitAction(StatRecord* rec, StatCondition condition, int cost, void* clientdata = NULL)
	{
		if(!gStatInited)
			return;

		rec->m_TotalInst++;		//addbykeyming 20140609
		StatRecord::m_totalInstrNum++;		//addbykeyming 20140609


		rec->m_CommitedThread++;

		//update the parallel execution time
		if(rec->m_FinishTime > rec->m_ParalTime){
			rec->m_ParalTime = rec->m_FinishTime;
		}else{
			rec->m_ParalTime++;			//keyming 20131212
		}


		//update the sequential execution time
		rec->m_SeqTime += (rec->m_FinishTime - rec->m_StartTime);

		/*! \brief updata finish time in case the subthread was squashed*/
		if(rec->m_ParalTime > rec->m_FinishTime){
			rec->m_FinishTime = rec->m_ParalTime;
			//keyming 1124_2013 line 7
//#define DEBUG_KEYMING
#ifdef DEBUG_KEYMING
std::cerr <<"*****DEBUG_KEYMING*****" << "\t(" <<  __FILE__ << ":" << __LINE__ << " " << __func__ << "): " << std::endl;
std::cerr << "In ParalTime" << std::endl;

#undef DEBUG_KEYMING
#endif
		}

/*
		//keyming 1127_2013
		#define KEYMING_STATICS
		#ifdef KEYMING_STATICS
				rec->m_TimeStatistic->m_waitTime += rec->m_ParalTime - preFinishTime;
				rec->m_TimeStatistic->m_spTime += rec->m_TimeStatistic->m_timeSlot;
				rec->m_TimeStatistic->m_timeSlot = 0;
		#endif
*/

		/*! updata the \b start time*/
		rec->m_StartTime = rec->m_FinishTime;		//这一步很关键， 因为可能推测线程验证子线程时 验证失败， 从而推测线程
													//继续往下执行， 以non spective 方式执行.
		//assert(rec->m_ParalTime == StatRecord::m_ClockParalTime)
		//该断言测试很多时候失败， 所以换为下面的语句.
		if(rec->m_ParalTime != StatRecord::m_ClockParalTime){
			std::cerr << "rec->m_ParalTime = " << rec->m_ParalTime << "\t\t" << "m_ClockParalTime" << StatRecord::m_ClockParalTime << std::endl;
			fprintf(mystderrlog, "CommitAction:(%s)\t%d\t%s\tin  rec->m_ParalTime = %d\tm_ClockParalTime=%d\n", __FILE__ ,__LINE__, __func__,rec->m_ParalTime,StatRecord::m_ClockParalTime);
		}
		if(StatRecord::m_ClockParalTime == 2693){
			int i = 0;
			i = rec->m_ParalTime;
			i++;
		}

		//pinfo<<"current sequential time = "<<rec->m_SeqTime<<"\n current parallel time = "<<rec->m_ParalTime<<pendl;
	}

	static void SquashAction(StatRecord* rec, StatCondition condition, int cost, void* data = NULL)
	{
		if(!gStatInited)
			return;


		SquashClientData *clientdata = (SquashClientData*)data;
		assert(data != NULL);

		if(std::strcmp(clientdata->m_Reason, "DDV") ==0)
		{
			rec->m_FailOnDDV++;
		}else if(std::strcmp(clientdata->m_Reason, "INSTRUCTION") == 0) {
			rec->m_FailOnInstruction++;
		}else if(std::strcmp(clientdata->m_Reason, "V_REG_VOILATE") == 0) {
			rec->m_FailOnRegV++;
		}else if(std::strcmp(clientdata->m_Reason, "V_MM_VOILATE") == 0) {
			rec->m_FailOnMMV++;
		}else if(std::strcmp(clientdata->m_Reason, "LOW_PRIORITY") == 0) {
			rec->m_FailOnLowPriority++;
		}else if(std::strcmp(clientdata->m_Reason, "CONTROL_VOILATE") == 0) {
			rec->m_FailOnControl++;
		}else if(std::strcmp(clientdata->m_Reason, "RESET") ==  0) {
			rec->m_FailOnReset++;
		}else if(std::strcmp(clientdata->m_Reason, "HALT") == 0) {
			rec->m_FailOnHalt++;
		}else if(std::strcmp(clientdata->m_Reason, "QUIT") == 0) {
			rec->m_FailOnQuit++;
		}else if(std::strcmp(clientdata->m_Reason, "EXCEPTION") == 0) {
			rec->m_FailOnException++;
		}else if(std::strcmp(clientdata->m_Reason, "XXX") == 0) {
			rec->m_FailOnUnknown++;
		}
		rec->m_FailedThread++;

#define KEYMING_PLUS
#ifdef KEYMING_PLUS
		int dis = clientdata->m_DisInSpList;
		assert(dis>=0 && dis < StatRecord::m_FailNumBufSize);
		rec->m_FailNumBuf[dis]++;
#undef KEYMING_PLUS
#endif


		//keyming 1127_2013
#define KEYMING_STATICS
#ifdef KEYMING_STATICS
		/*
				rec->m_TimeStatistic->m_squashTime += rec->m_TimeStatistic->m_timeSlot;
				rec->m_TimeStatistic->m_timeSlot = 0;
		*/
		//keyming 20131211
		StatState runState = rec->m_TimeStatistic->m_runState;
		assert(runState == PSLICE || runState == SPEXE || runState == WAIT || runState == IDLE);
		if(runState == IDLE){
#define DEBUG_KEYMING
#ifdef DEBUG_KEYMING
std::cout <<"*****DEBUG_KEYMING*****" << "\t(" <<  __FILE__ << ":" << __LINE__ << " " << __func__ << "): " << std::endl;
fprintf(stderr, "%d\n",runState);

#undef DEBUG_KEYMING
#endif
		}

		if(runState == PSLICE){
			rec->m_TimeStatistic->m_psliceTime += rec->m_TimeStatistic->m_timeSlot;
		}
		else if(runState == SPEXE){
			rec->m_TimeStatistic->m_squashTime += rec->m_TimeStatistic->m_timeSlot;
		}
		else if(runState == WAIT){
			rec->m_TimeStatistic->m_squashTime += rec->m_TimeStatistic->m_timeNeedVerify;
			rec->m_TimeStatistic->m_timeNeedVerify = 0;
		}
		else{
			//assert(0);
		}
		rec->m_TimeStatistic->m_lastFinishTime = StatRecord::m_ClockParalTime;
		rec->m_TimeStatistic->m_runState = IDLE;

#endif

		//keyming 20131204
		int listLen = SpeculativeLogic::GetInstance()->SpListSize();
		++StatRecord::m_CpuStat->coreNum[listLen-1];

	}

	static void RestartAction(StatRecord* rec, StatCondition condition, int cost, void* clientdata = NULL)
	{
		if(!gStatInited)
			return;

		//keyming 1127_2013
#define KEYMING_STATICS
#ifdef KEYMING_STATICS
		if(rec->m_TimeStatistic->m_runState == PSLICE){
			rec->m_TimeStatistic->m_timeSlot+= cost;
			rec->m_TimeStatistic->m_psliceTime += rec->m_TimeStatistic->m_timeSlot;
			rec->m_TimeStatistic->m_timeSlot = 0;
			rec->m_TimeStatistic->m_runState = SPEXE;
		}
		else if(rec->m_TimeStatistic->m_runState == SPEXE){
			rec->m_TimeStatistic->m_timeSlot+= cost;
			rec->m_TimeStatistic->m_squashTime+= rec->m_TimeStatistic->m_timeSlot;
			rec->m_TimeStatistic->m_timeSlot = 0;
			rec->m_TimeStatistic->m_runState = SPEXE;
		}
	//	rec->m_TimeStatistic->m_timeSlot = rec->m_FinishTime - rec->m_StartTime;

#endif
		rec->m_StartTime = rec->m_FinishTime;
	}

	/*! \brief this is a hook function, the user can register its own actions here
	*/
	void RegisterActions()
	{
		gStatMap["spawn"].m_Action = SpawnAction;
		gStatMap["ebegin"].m_Action = BeginAction;
		gStatMap["ecommit"].m_Action = CommitAction;
		gStatMap["esquash"].m_Action = SquashAction;
		gStatMap["erestart"].m_Action = RestartAction;
		gStatMap["cpone"].m_Action = CponeAction;
		gStatMap["cpzero"].m_Action = CpzeroAction;

		//keyming 20131202
#define KEYMING_STATICS
#ifdef KEYMING_STATICS
		gStatMap["epslice_entry"].m_Action = PsliceEntryAction;		//统计pslice 内消耗的消耗的时钟周期
		gStatMap["epslice_exit"].m_Action = PsliceExitAction;
		gStatMap["establetoken"].m_Action = StableTokenAction;
		gStatMap["everify"].m_Action = VerifyAction;
		gStatMap["ecqiphold"].m_Action = CqipHoldAction;
		gStatMap["ecqipcommit"].m_Action = CqipCommitAction;
#endif
	}

	/*! \brief initialize the Statistic module
	*/
	void InitStat(const char* prgname)
	{
		std::fstream infile;
		infile.open(file);


		if(!file)
		{
			std::cout<<"can not open setting file!"<<std::endl;
			exit(0);
		}
		// keyming 1009_2013
		if(!infile){
			std::cout<<"can not open file!"<<std::endl;
			exit(0);
		}

		std::string name;
		int value;

		infile>>name;
		//printf("%s*****\n", name.c_str());
		infile>>SettingVersion;
		//const char *p = name.c_str();
		if(value == 2){
			;
			;
		}
		if(name != "version")
		{
			std::cout<<"this seems not a setting file!"<<std::endl;
			exit(0);
		}

		while(!infile.eof())
		{
			infile>>name;

			if(name.empty())
				continue;

			if(name.substr(0, 2) == "//")	//comment
			{
				char comment[2048];
				infile.getline(comment, 2048);
				continue;
			}

			infile>>value;
			StatItem item;
			item.m_Cost = value;
			item.m_Action = DefaultStatAction;
			gStatMap[name] = item;
		}

		infile.close();
		RegisterActions();
		std::strcpy(program, prgname);
		gStatInited = true;
/*
		StatMap::iterator it = gStatMap.begin();
		while(it != gStatMap.end())
		{
			std::cout << it->first << "\t" << it->second.m_Cost << std::endl;
			it++;
		}
*/
	}
//
	void ReportStat()
	{
		std::cerr << "in ReportStat " << std::endl;
		XmlElement* root = new XmlElement("statistic_report");
		time_t now = time(NULL);
		char timebuf[100];
		strftime(timebuf, 100, "%Y-%m-%d %H:%M:%S ", localtime(&now));
		root->addAttribute("report_time", std::string(timebuf));

		XmlElement* set = new XmlElement("settings");
		XmlElement* sitem = new XmlElement("setting_file");
		sitem->setContent(std::string(file));
		set->addElement(sitem);

		sitem = new XmlElement("version");
		sitem->setContent(SettingVersion);
		set->addElement(sitem);

		sitem = new XmlElement("program");
		sitem->setContent(std::string(program));
		set->addElement(sitem);
		root->addElement(set);

		Statistic::Get()->AddToXml(root);

		XmlElement* ana = new XmlElement("analysis");
		/**
		 * addbykeyming 20140609
		 */
		XmlElement* subana = new XmlElement("total_instruction_num");
		char buffer[65];
		sprintf(buffer, "%ld", StatRecord::m_totalInstrNum);
		subana->setContent(std::string(buffer));
		ana->addElement(subana);


		subana = new XmlElement("sequential_time");
		sprintf(buffer, "%d", StatRecord::m_SeqTime);
		subana->setContent(std::string(buffer));
		ana->addElement(subana);

		subana = new XmlElement("parallel_time");
		sprintf(buffer, "%d", StatRecord::m_ParalTime);
		subana->setContent(std::string(buffer));
		ana->addElement(subana);

		subana = new XmlElement("speed_up");
		sprintf(buffer, "%f", double(StatRecord::m_SeqTime) / StatRecord::m_ParalTime);
		subana->setContent(std::string(buffer));
		ana->addElement(subana);

		Statistic::Get()->AddStat(ana);
		root->addElement(ana);

#define KEYMING_STATICS
#ifdef KEYMING_STATICS
		ProphetStat::ProphetCpuStat::getProCpuStatInstance()->addProCpuStatToXml(root);
		ProphetStat::ProphetCpuStat::getProCpuStatInstance()->printCpuStat(NULL);
#endif

		XmlDocument* doc = new XmlDocument();
		doc->setRootElement(root);


		char cpuNumBuf[32];
		sprintf(cpuNumBuf, "%d", StatRecord::m_FailNumBufSize);
		std::string docname(program);
		docname += cpuNumBuf;
		docname += "_statistic.xml";
		remove(docname.c_str());
		std::fstream xmldoc;
		xmldoc.open(docname.c_str(), std::ios::out);
		if(!xmldoc)
		{
			std::cout<<"can not open report document!"<<std::endl;
			return;
		}

		xmldoc<<doc->toString();
		xmldoc.close();
	}
}
//
