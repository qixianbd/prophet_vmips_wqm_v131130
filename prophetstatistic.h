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

#ifndef _PROPHETSTATISTIC_H
#define _PROPHETSTATISTIC_H

#include "prophetxmlelement.h"
#include <list>
#include <map>
#include "types.h"

//#define STAT_PATH SYSCONFDIR"/inst_cost.ini"
//keyming 1009_2013
#define STAT_PATH "/home/qmwx/workspace/prophet_vmips_final_keyming_131130/inst_cost.ini"

/*! \brief enumerate all the instruction condition for statistic
*/
typedef enum StatCondition{
	FAILED = 0,
	SUCCESS = 1
};

namespace ProphetStat
{
/**
 * StatState , 统计时间时用的CPU运行状态列表
 */
	enum StatState{
		IDLE = 0, 			//CPU此时空闲
		PSLICE, 			//执行pslice
		SPEXE, 				//在推测执行
		NOSPEXE, 			//在非推测执行
		WAIT				//(推测线程)在等待验证
	};
	//keyming 1127_2013 line 10
	//统计CPU的运行时间组成, 包括空闲时间， pslice执行时间， 等待时间， （成功的）推测执行时间，非推测执行时间
	//以及撤销时间
	struct StatTime{
		int m_freeTime;		//CPU空闲
		int m_squashTime;	//thread推测执行， 最后被撤销
		int m_waitTime;		// subthread等待验证
		int m_spTime;		// subthread验证成功时  其推测执行的时间
		int m_noSptime;		// 非推测执行时间

		int m_timeSlot;		//当前线程执行的时间片, 这个只对这个线程有效。 该struct内的其他属性是统计数据， 基于CPU在整个过程中的统计数据
							//timeSlot为临时数据, (类似于临时变量)
		int m_timeNeedVerify;		//这段时间需要被验证之后才能确定是将会被撤销的，还是将会验证成功的称为m_spTime??


		long m_totalTime;	//单个核运行的总时间
		int m_psliceTime;	// pslice 段执行的时间
		int m_lastFinishTime;	//核上次空闲之前运行的最后的时间
		StatState m_runState;				//CPU此刻正在运行的状态
		long setTotalTime();		//求单个核运行的总时间
		StatTime();
	};

/**
 * 统计CPU核利用率， 主要从在某一时刻， 有多少个核在同时运行这个方面进行统计的。
 * 这个是单例模式， 使用getStatCpuInstance来构造一个唯一的对象。
 */
	struct StatCpu{
		static StatCpu* hasone;			//静态变量， 只想唯一的StatCpu单例
		int num;				//CPU 核数
		int *coreNum;			//coreNum[0..num-1], core[i-1]运行核个个数为i(0<=i<=num)的总次数
		double *proportion;		//各个次数在总的统计中所占的比例
		double averageCoreNum();			//平均运行时核的个数
		double* calProportion();			//计算比例
		static StatCpu* getStatCpuInstance(int n);			// 获取一个StatCpu类型的单例对象
	private:
		StatCpu(int n);			//private 使得单例实现
	};

	/**
	 * 非静态属性 只统计单个核（由单个对象表示）上的若干参数， 静态属性统计全局的（所有对象，所有核上）参数
	 * 因此非静态属性使用时， 最后要把所有的核统计的结果累加， 而静态属性不需要累加
	 */
	/*! \brief a class to store the statistical variables
	*/
	class StatRecord
	{
	public:
		int m_TotalInst;	//the number of total instruction executed
		int m_TotalClock;	//the number of total clock
		int m_SpawnedThread;
		int m_CommitedThread;	//the number of thread successfuly executed

		//count the occurrence of squash for all reason
		int m_FailedThread;
		int m_FailOnDDV;
		int m_FailOnInstruction;
		int m_FailOnRegV;
		int m_FailOnMMV;
		int m_FailOnLowPriority;
		int m_FailOnControl;
		int m_FailOnReset;
		int m_FailOnHalt;
		int m_FailOnQuit;
		int m_FailOnException;
		int m_FailOnUnknown;

		int m_StartTime;
		int m_FinishTime;

/**
 * addbykeyming 20140609.
 * statistic for total instructions that has been executed.
 */
		//long m_instrNum;
		static long m_totalInstrNum;

		//keyming 1121_2013
		static int m_FailNumBufSize;			//名称没有取好， 应该叫总的CPU个数
		int *m_FailNumBuf;						//代表推测撤销时， 撤销两核的距离
		StatTime *m_TimeStatistic;				//统计CPU运行时间
		static StatCpu *m_CpuStat;				//统计CPU核的利用率


		static int m_SeqTime;
		static int m_ParalTime;

		//keyming 20121211
		static int m_ClockParalTime;

		StatRecord();
		XmlElement* XmlNode();
	};



	/*! \brief class RecAdder wil be inherited by the classes needing statistic to add a StatRecord instance
	*/
	class RecAdder
	{
	public:
		RecAdder();
		virtual ~RecAdder();
		StatRecord* GetRec();

	protected:
		StatRecord *m_Record;
	};

	/*! \brief statistic action for a specific instruction
	*/
	typedef void (*STAT_ACTION)(StatRecord*, StatCondition, int, void*);

	/*! \brief instruction hash list item, its content is loaded from setting file
	*/
	typedef struct StatItem{
		int m_Cost;
		STAT_ACTION m_Action;
	}StatItem;

	typedef std::map<std::string, StatItem> StatMap;

	/*! \brief global variable to store the statistic settings
	*/
	extern StatMap gStatMap;

	/*! \brief indicating whether the statistic module is initialized or not
	*/
	extern bool gStatInited;

	void InitStat(const char*);
	void ReportStat();
}

/*! \brief macros for declareing the class needing statistic
*/
#define PROPHET_STAT_CLASS(name)	class name : public ProphetStat::RecAdder
#define WITH_PUBLIC_ROOT(name)		, public name
#define WITH_PRIVATE_ROOT(name)		, private name
#define WITH_PROTECTED_ROOT(name)	, protected name

/*! \brief this macros can only be used in a member function inherited from PROPHET_STAT_CLASS
* \param instname the name of the instruction
* \param condition the condition state of the execution of the instruction
* \param userdata the data needed to pass to the registered action function for \b instname	
*/
#define DO_STATISTIC(instname, condition, userdata...) \
	if(ProphetStat::gStatInited) \
	{ \
		ProphetStat::StatItem item = ProphetStat::gStatMap[instname]; \
		ProphetStat::STAT_ACTION action = item.m_Action; \
		action(RecAdder::m_Record, condition, item.m_Cost, ##userdata); \
		/*std::cerr<< "in DO_STATSTICS" << std::endl;*/ \
	}NULL

/*! \brief user defined data passed to its associated actions
*/
typedef struct SpawnClientData
{
	ProphetStat::RecAdder* m_SubAdder;
}SpawnClientData;

//the valide value for the squash reason string can be "DDV", "INSTRUCTION", "V_REG_VOILATE"...
typedef struct SquashClientData
{

	const char *m_Reason;
	//keyming 1126_2013 line 4
	//old char *m_Reason;

	// 该线程在推测链表的位置
	int m_DisInSpList;
}SquashClientData;

typedef struct CpzeroClientData
{
	uint16 m_rs;
	uint16 m_funct;
}CpzeroClientData;

typedef struct CponeClientData
{
	uint16 m_rs;
	uint16 m_funct;
	uint16 m_ft;
}CponeClientData;

#endif	//_PROPHETSTATISTIC_H
