/*
 * prophetcpustat.cc
 *
 *  Created on: 2013年12月9日
 *      Author: qmwx
 */

#include "prophetcpustat.h"
#include "prophetxmldoc.h"
#include "prophetxmlelement.h"
#include "options.h"
#include "vmips.h"
#include "prophet_vmips.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fstream>
#include <iostream>


extern vmips *machine;
namespace ProphetStat{

	int ProphetCpuStat::coreNum = 0;
	ProphetCpuStat* ProphetCpuStat::instance = NULL;
	SquashOnSpList* SquashOnSpList::instance = NULL;


	SquashOnSpList::SquashOnSpList(int num): coreNum(num), squashStatOnSpList(std::vector<int>(num)), spawnStatOnSpList(std::vector<int>(num)),
			propOnSpList(std::vector<double>(num))
	{
	}

	SquashOnSpList::~SquashOnSpList()
	{
	}

	SquashOnSpList* SquashOnSpList::getInstance(int num)
	{
		assert(num > 0);
		if(NULL != instance){
			return instance;
		}
		instance = new SquashOnSpList(num);
		return instance;
	}

	void SquashOnSpList::addToList(SquashOrSpawn state, int pos)
	{
		assert(state == TOSQUASH || state == TOSPAWN);
		assert(pos < coreNum && pos >= 0);
		if(state == TOSPAWN){
			spawnStatOnSpList[pos]++;
		}
		else{
			squashStatOnSpList[pos]++;
		}
	}

	void SquashOnSpList::addToXml(XmlElement* root)
	{
		calProp();
		XmlElement* squashStat = new XmlElement("SquashOnSpListStat");
		for(int i = 0; i < coreNum; i++){
			char buf[32];
			XmlElement* stat = new XmlElement("OnSpList");
			stat->addAttribute("OnSpList ", i);
			sprintf(buf, "%d/%d(%.5lf)", squashStatOnSpList[i], spawnStatOnSpList[i], propOnSpList[i]);
			stat->setContent(buf);
			squashStat->addElement(stat);
		}
		root->addElement(squashStat);
	}

	void SquashOnSpList::calProp()
	{
		static int count = 0;
		if(count != 0){
			return ;
		}
		count++;
		for(int i = 0; i < coreNum; i++){
			propOnSpList[i] =  squashStatOnSpList[i]/(double)spawnStatOnSpList[i];
		}
	}

	void SquashOnSpList::printList(FILE* fp)
	{
		assert(fp != NULL);
		calProp();
		for(int i = 0; i < coreNum; i++){
			fprintf(fp, "%6d/%6d\t", squashStatOnSpList[i], spawnStatOnSpList[i]);
		}
		fprintf(fp, "\n");
		for(int i = 0; i < coreNum; i++){
			fprintf(fp, "%13lf\t", propOnSpList[i]);
		}
		fprintf(fp, "\n");

	}


	ProphetCpuStat::ProphetCpuStat():squashLenList(new std::vector<int*>(coreNum)), totalSquashLenList(new std::vector<int>(coreNum)),
			squOnList(NULL)
	{
		for(int i = 0; i < coreNum; i++){
			(*squashLenList)[i] = new int[coreNum];
			memset((*squashLenList)[i], 0, coreNum*sizeof(int));
		}
		squOnList = SquashOnSpList::getInstance(coreNum);
	}

	ProphetCpuStat::~ProphetCpuStat()
	{
		for(int i = 0; i < coreNum; i++){
			delete[] ((*squashLenList)[i]);
		}
		delete squashLenList;
		delete totalSquashLenList;
	}

	ProphetCpuStat* ProphetCpuStat::getProCpuStatInstance()
	{
		if(instance != NULL){
			return instance;
		}
#ifdef PROPHETCPUSTAT_TEST
		coreNum = 4;
#else
		coreNum = ::machine->opt->option("corenum")->num;
#endif
		instance = new ProphetCpuStat();
		return instance;
	}

	SquashOnSpList* ProphetCpuStat::getSquList()
	{
		return squOnList;
	}
	void ProphetCpuStat::addSquashLen(int cpuId, int len)
	{
		assert(cpuId>=0 && cpuId < coreNum);
		assert(len>=0 && len <coreNum);
		(*squashLenList)[cpuId][len]++;
	}
	void ProphetCpuStat::getTotalSquashLen()
	{
		static int count = 0;
		if(count != 0){
			return ;
		}
		count++;
		for(int len = 0; len < coreNum; len++)
		{
			for(int cpuid = 0; cpuid < coreNum; cpuid++){
				(*totalSquashLenList)[len] += (*squashLenList)[cpuid][len];
			}
		}
	}
	void ProphetCpuStat::printCpuStat(FILE* fp)
	{
		assert(fp == NULL || fp == stdout || fp == stderr);
		std::string filename;
		char buf[32];
		if(fp != stdout && fp != stderr){
#ifdef PROPHETCPUSTAT_TEST
			filename += "mytest";
#else
			filename += ::machine->opt->option("romfile")->str;
#endif
			sprintf(buf, "__%d__%s", coreNum, "cpustat.txt");
			filename+= buf;
			fp = fopen(filename.c_str(), "w");
			if(NULL == fp){
				std::cerr << "Can not open file " << filename << std::endl;
				exit(1);
			}
		}
		printSquashLen(fp);
		this->squOnList->printList(fp);

		fclose(fp);
	}

	void ProphetCpuStat::printSquashLen(FILE* fp)
	{
		assert(fp != NULL);
		getTotalSquashLen();

		//fprintf(fp, "%d(%lf)\t%d(%lf)\t%d(%lf)\t%d(%lf)\n");
		for(int cpuid = 0; cpuid < coreNum; cpuid++){
			for(int len = 0; len < coreNum; len++){
				fprintf(fp, "%14d\t", (*squashLenList)[cpuid][len]);
			}
			fprintf(fp, "\n");
		}

		int totalSquashLen = 0;
		for(int i = 0; i < coreNum; i++){
			totalSquashLen += (*totalSquashLenList)[i];
		}
		for(int len = 0; len < coreNum; len++){
			fprintf(fp, "%6d(%6.5lf)\t", (*totalSquashLenList)[len], (*totalSquashLenList)[len]/(double)totalSquashLen);
		}
		fprintf(fp,"\n");

	}

	void ProphetCpuStat::addSquashLenToXml(XmlElement* root)
	{
		getTotalSquashLen();
		int totalSquashLen = 0;
		for(int i = 0; i < coreNum; i++){
			totalSquashLen += (*totalSquashLenList)[i];
		}
		XmlElement* prostat = new XmlElement("SquashLenStat");
		for(int i = 0; i < coreNum; i++){
			char buf[32];
			XmlElement* stat = new XmlElement("SquashLen");
			stat->addAttribute("SquashLen ", i);
			sprintf(buf, "%d(%lf)", (*totalSquashLenList)[i], (*totalSquashLenList)[i]/(double)totalSquashLen);
			stat->setContent(buf);
			prostat->addElement(stat);
		}
		root->addElement(prostat);
	}

	void ProphetCpuStat::addProCpuStatToXml(XmlElement* root)
	{
		XmlElement *rootProStat = new XmlElement("ProStatData");
		addSquashLenToXml(rootProStat);
		squOnList->addToXml(rootProStat);



		root->addElement(rootProStat);
	}

}



