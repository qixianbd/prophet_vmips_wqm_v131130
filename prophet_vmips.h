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

#ifndef __PROPHET_H
#define __PROPHET_H

#include "vmips.h"
#include "prophetlog.h"

class Prophet : public vmips
{
	PLOG_CLASS(Prophet);
public:
	Prophet(uint32 argc, char **argv);
	~Prophet() throw();

	void setup_machine(void);
	void step(void);
	int run(void);
	uint32 getCpuNum(){return m_CPUNum;}

private:
	uint32 m_CPUNum;	//prophet cpus
};

#endif
