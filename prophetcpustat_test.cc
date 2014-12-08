/*
 * prophetcpustat_test.cc
 *
 *  Created on: 2013年12月10日
 *      Author: qmwx
 */

#include "prophetcpustat.h"
using namespace ProphetStat;
int main()
{
	ProphetCpuStat *prostat = ProphetCpuStat::getProCpuStatInstance();
	prostat->getTotalSquashLen();
	FILE *fp = NULL;
	prostat->printSquashLen(fp);
}


