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

#include "prophet_vmips.h"
#include "speculativecpu.h"
#include "speculativelogic.h"
#include "prophetlog.h"
#include "prophetconsole.h"
#include "prophetstatistic.h"
#include "options.h"
#include <signal.h>
#include <sys/time.h>
#include <stdio.h>
#include <assert.h>
#include <exception>

extern int restart_counts = 0;
extern FILE *mystderrlog = NULL;

Prophet::Prophet(uint32 argc, char **argv) : vmips(argc, argv), m_CPUNum(0)
{}

Prophet::~Prophet() throw()
{}

void Prophet::setup_machine(void)	//the debug can not use in prophet
{
	SpeculativeCPU *mycpu = NULL;

	//build the base component
	vmips::setup_machine();

	//load core number
	m_CPUNum = opt->option("corenum")->num;

	if(m_CPUNum < 1)
		perror<<"no core in virtual machine"<<pendl;

	//build sp-cpu for prophet-vmips
	for(uint32 i = 0; i < m_CPUNum; i++)
	{
		mycpu = new SpeculativeCPU(*physmem, *intc);
		mycpu->SetRegDumpMark(opt->option("dumpcpu")->flag);
		SpeculativeLogic::GetInstance()->SwapToFreeList(mycpu);
		//keyming 1025_2013 practise
		//#define DEBUG_KEYMING
		#ifdef DEBUG_KEYMING
		std::cout <<"*****DEBUG_KEYMING*****" << "\t" <<  __FILE__ << ": " << __LINE__ << ": " << std::endl;
		std::cout << "m_SpList.size() : " << SpeculativeLogic::GetInstance()->SpListSize()<< std::endl;
		std::cout << "m_freeSpList.size() : " << SpeculativeLogic::GetInstance()->FreeListSize()<< std::endl;
		#undef DEBUG_KEYMING
		#endif
	}
}


void Prophet::step(void)		//we should do more thing here
{
	//process instruction
	SpeculativeLogic::GetInstance()->Step();
}

static void OnQuit(int s)
{
	// std::cerr << " This is in On Quit " << std::endl;
	machine->halt();
	SpeculativeLogic::GetInstance()->CheckStateOnQuit();
}

int Prophet::run(void)
{
	/* Check host processor endianness. */
	if (host_endian_selftest () != 0) {
		perror<<"Could not determine host processor endianness."<<pendl;
		return 1;
	}

	//must initialize statistic before setup the machine
	ProphetStat::InitStat(opt_image);

	/* Set up the rest of the machine components. */
	setup_machine();

	if (!setup_rom ()) 
	  return 1;
		;

	if (!setup_ram ())
	  return 1;

	if (!setup_haltdevice ())
	  return 1;

	if (!setup_clock ())
	  return 1;

	if (!setup_clockdevice ())
	  return 1;

	if (!setup_decrtc ())
	  return 1;

	if (!setup_deccsr ())
	  return 1;

	if (!setup_decstat ())
	  return 1;

	if (!setup_decserial ())
	  return 1;

	if (!setup_spimconsole ())
	  return 1;
	void (*myold_handler)(int);
	myold_handler = signal(SIGINT, &OnQuit);
	if(myold_handler == SIG_ERR){
		std::cerr << "signal bind error " << std::endl;
	}

	/* Reset the CPU. */
	pinfo<<"\n*************RESET*************\n\n"<<pendl;

	SpeculativeLogic::GetInstance()->Reset();

	Plog::InitPlog(opt->option("filelog")->flag, opt->option("consolelog")->flag);

	if (!setup_exe ())
	  return 1;

	timeval start;
	if (opt_instcounts)
		gettimeofday(&start, NULL);

	//we do not support debug now
//	if (opt_debug) {
//		dbgr->serverloop();
//	} else {
		while (!halted)
			step();
//	}

	timeval end;
	if (opt_instcounts)
		gettimeofday(&end, NULL);

	//report statistic result
	ProphetStat::ReportStat();
	//keyming 1028_2013 practise
	#define DEBUG_KEYMING
	#ifdef DEBUG_KEYMING
	std::cout <<"*****DEBUG_KEYMING*****" << "\t" <<  __FILE__ << ": " << __LINE__ << ": " << std::endl;
	std::cerr << "begin statistics " << std::endl;

	#undef DEBUG_KEYMING
	#endif

	/* Halt! */
	pinfo<<"\n*************HALT*************\n\n"<<pendl;

	//we should do more after this
	return 0;
}
static void prophet_unexpected()
{
	perror<<"unexpected exception received!"<<pendl;
}

static void prophet_terminate()
{
	perror<<"uncaught exception!"<<pendl;
}
//physical address   prophet_console.txt
int main(int argc, char **argv)
{
	try {
		std::set_unexpected(prophet_unexpected);
		std::set_terminate(prophet_terminate);

		mystderrlog = fopen("mystderrlog.txt", "w");
		assert(mystderrlog != NULL);
		machine = new Prophet(argc, argv);

		/*console initialization*/
		ProphetConsole::Init(argc, argv);

		int ret = machine->run();

		//keyming 20131204
		fprintf(stderr, "corenum:%d\n", ((Prophet*)machine)->getCpuNum());
		fprintf(stderr, "restart_counts : %d\n",restart_counts);

		delete machine;
		machine = NULL;

		fclose(mystderrlog);

		return ret;
	}
	catch(...){
		perror<<"exception occur!"<<pendl;
	}
}
//
