/*
 * prophetcpustat.h
 *
 *  Created on: 2013年12月9日
 *      Author: qmwx
 */

#ifndef PROPHETCPUSTAT_H_
#define PROPHETCPUSTAT_H_

#include <vector>
#include "prophetxmldoc.h"
#include "prophetxmlelement.h"

namespace ProphetStat{

	enum SquashOrSpawn{
		TOSQUASH, TOSPAWN
	};

	class SquashOnSpList{
	public:
		static SquashOnSpList* getInstance(int num);
		void addToList(SquashOrSpawn state, int posSpList);
		void addToXml(XmlElement* root);
		void printList(FILE* fp= stderr);
		void calProp();
		virtual ~SquashOnSpList();
	private:
		SquashOnSpList(int num);
		SquashOnSpList(const SquashOnSpList&);
		SquashOnSpList& operator= (const SquashOnSpList &);


		static SquashOnSpList *instance;
		int coreNum;
		std::vector<int> squashStatOnSpList;
		std::vector<int> spawnStatOnSpList;
		std::vector<double> propOnSpList;
	};
/*! \brief Prophet cpu stat class.
 *
 */
	class ProphetCpuStat{
	public:
		static int coreNum;
		/**
		 * 单例模式， 调用该静态方法返回唯一的ProphetCpuStat对象。在返回该对象前， 将coreNum的值设为
		 * 用户在运行时附带的参数的值.
		 * @return 返回指向ProphetCpuStat对象的指针.
		 */
		static ProphetCpuStat * getProCpuStatInstance();		//单例模式

/**
 *将核号为cpuid上的撤销长度为len的统计值加一
 * @param cpuId		核号[0..coreNum-1]
 * @param len		撤销长度(此次撤销总共撤销的线程数 ) [0..coreNum-1]
 */
		void addSquashLen(int cpuId, int len);

		/**
		 * 将ProphetCpuStat对象统计的信息加入到root元素下，(供统一打印)
		 * @param root (代挂载的XmlElement元素指针)
		 */
		void addProCpuStatToXml(XmlElement* root);

		/**
		 * 向文件fp 打印出ProphetCpuStat对象统计的信息.fp 指向的为普通文本文件.调用此方法时，
		 * fp 为NULL, 或stderr, 或stdout, 其他的fp不接受。当fp 为NULL使， 由方法的实现决定
		 * 创建一个新文件， 并将fp指针指向该文件.
		 * @param fp
		 */
		void printCpuStat(FILE* fp= stderr);
/*! \brief get she squash list of the squOnList.
 *
 * @return	return the squash list
 */
		SquashOnSpList* getSquList();
		virtual ~ProphetCpuStat();

	protected:
		void getTotalSquashLen();
		void printSquashLen(FILE* fp= stderr);
		void addSquashLenToXml(XmlElement* root);
	private:
		ProphetCpuStat();
		ProphetCpuStat(const ProphetCpuStat&);
		ProphetCpuStat& operator=(const ProphetCpuStat&);

		static ProphetCpuStat *instance;
		SquashOnSpList* squOnList;

	//	int* squashLen;

		/**
		 * squashLenList存放的是每个核上的撤销长度的统计值。 整个squashLenList表现的行为类似于一个
		 * 二维数组.
		 */
		std::vector<int*> *squashLenList;
		/**
		 * 所有核之和的撤销长度的统计信息
		 */
		std::vector<int> *totalSquashLenList;
	};

}






#endif /* PROPHETCPUSTAT_H_ */
