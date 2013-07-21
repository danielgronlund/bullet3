//this file is autogenerated using stringify.bat (premake --stringify) in the build folder of this project
static const char* sapFastCL= \
"/*\n"
"Copyright (c) 2012 Advanced Micro Devices, Inc.  \n"
"\n"
"This software is provided 'as-is', without any express or implied warranty.\n"
"In no event will the authors be held liable for any damages arising from the use of this software.\n"
"Permission is granted to anyone to use this software for any purpose, \n"
"including commercial applications, and to alter it and redistribute it freely, \n"
"subject to the following restrictions:\n"
"\n"
"1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.\n"
"2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.\n"
"3. This notice may not be removed or altered from any source distribution.\n"
"*/\n"
"//Originally written by Erwin Coumans\n"
"\n"
"\n"
"typedef struct \n"
"{\n"
"	union\n"
"	{\n"
"		float4	m_min;\n"
"		float   m_minElems[4];\n"
"		int			m_minIndices[4];\n"
"	};\n"
"	union\n"
"	{\n"
"		float4	m_max;\n"
"		float   m_maxElems[4];\n"
"		int			m_maxIndices[4];\n"
"	};\n"
"} btAabbCL;\n"
"\n"
"typedef struct \n"
"{\n"
"	union\n"
"	{\n"
"		unsigned int m_key;\n"
"		unsigned int x;\n"
"	};\n"
"\n"
"	union\n"
"	{\n"
"		unsigned int m_value;\n"
"		unsigned int y;\n"
"		\n"
"	};\n"
"}b3SortData;\n"
"\n"
"\n"
"/// conservative test for overlap between two aabbs\n"
"bool TestAabbAgainstAabb2(const btAabbCL* aabb1, __local const btAabbCL* aabb2);\n"
"bool TestAabbAgainstAabb2(const btAabbCL* aabb1, __local const btAabbCL* aabb2)\n"
"{\n"
"//skip pairs between static (mass=0) objects\n"
"	if ((aabb1->m_maxIndices[3]==0) && (aabb2->m_maxIndices[3] == 0))\n"
"		return false;\n"
"		\n"
"	bool overlap = true;\n"
"	overlap = (aabb1->m_min.x > aabb2->m_max.x || aabb1->m_max.x < aabb2->m_min.x) ? false : overlap;\n"
"	overlap = (aabb1->m_min.z > aabb2->m_max.z || aabb1->m_max.z < aabb2->m_min.z) ? false : overlap;\n"
"	overlap = (aabb1->m_min.y > aabb2->m_max.y || aabb1->m_max.y < aabb2->m_min.y) ? false : overlap;\n"
"	return overlap;\n"
"}\n"
"\n"
"__kernel void   computePairsIncremental3dSapKernel( __global const uint2* objectMinMaxIndexGPUaxis0,\n"
"													__global const uint2* objectMinMaxIndexGPUaxis1,\n"
"													__global const uint2* objectMinMaxIndexGPUaxis2,\n"
"													__global const uint2* objectMinMaxIndexGPUaxis0prev,\n"
"													__global const uint2* objectMinMaxIndexGPUaxis1prev,\n"
"													__global const uint2* objectMinMaxIndexGPUaxis2prev,\n"
"													__global const b3SortData*	   sortedAxisGPU0,\n"
"													__global const b3SortData*	   sortedAxisGPU1,\n"
"													__global const b3SortData*	   sortedAxisGPU2,\n"
"													__global const b3SortData*	   sortedAxisGPU0prev,\n"
"													__global const b3SortData*	   sortedAxisGPU1prev,\n"
"													__global const b3SortData*	   sortedAxisGPU2prev,\n"
"													__global int2*			addedHostPairsGPU,\n"
"													__global int2*			removedHostPairsGPU,\n"
"													volatile __global int*				addedHostPairsCount,\n"
"													volatile __global int*				removedHostPairsCount,\n"
"													int maxCapacity,\n"
"													int numObjects)\n"
"{\n"
"	int i = get_global_id(0);\n"
"	if (i>=numObjects)\n"
"		return;\n"
"\n"
"	__global const uint2* objectMinMaxIndexGPU[3][2];\n"
"	objectMinMaxIndexGPU[0][0]=objectMinMaxIndexGPUaxis0;\n"
"	objectMinMaxIndexGPU[1][0]=objectMinMaxIndexGPUaxis1;\n"
"	objectMinMaxIndexGPU[2][0]=objectMinMaxIndexGPUaxis2;\n"
"	objectMinMaxIndexGPU[0][1]=objectMinMaxIndexGPUaxis0prev;\n"
"	objectMinMaxIndexGPU[1][1]=objectMinMaxIndexGPUaxis1prev;\n"
"	objectMinMaxIndexGPU[2][1]=objectMinMaxIndexGPUaxis2prev;\n"
"\n"
"	__global const b3SortData* sortedAxisGPU[3][2];\n"
"	sortedAxisGPU[0][0] = sortedAxisGPU0;\n"
"	sortedAxisGPU[1][0] = sortedAxisGPU1;\n"
"	sortedAxisGPU[2][0] = sortedAxisGPU2;\n"
"	sortedAxisGPU[0][1] = sortedAxisGPU0prev;\n"
"	sortedAxisGPU[1][1] = sortedAxisGPU1prev;\n"
"	sortedAxisGPU[2][1] = sortedAxisGPU2prev;\n"
"\n"
"	int m_currentBuffer = 0;\n"
"\n"
"	for (int axis=0;axis<3;axis++)\n"
"	{\n"
"		//int i = checkObjects[a];\n"
"\n"
"		unsigned int curMinIndex = objectMinMaxIndexGPU[axis][m_currentBuffer][i].x;\n"
"		unsigned int curMaxIndex = objectMinMaxIndexGPU[axis][m_currentBuffer][i].y;\n"
"		unsigned int prevMinIndex = objectMinMaxIndexGPU[axis][1-m_currentBuffer][i].x;\n"
"		int dmin = curMinIndex - prevMinIndex;\n"
"				\n"
"		unsigned int prevMaxIndex = objectMinMaxIndexGPU[axis][1-m_currentBuffer][i].y;\n"
"\n"
"		int dmax = curMaxIndex - prevMaxIndex;\n"
"	\n"
"		for (int otherbuffer = 0;otherbuffer<2;otherbuffer++)\n"
"		{\n"
"			if (dmin!=0)\n"
"			{\n"
"				int stepMin = dmin<0 ? -1 : 1;\n"
"				for (int j=prevMinIndex;j!=curMinIndex;j+=stepMin)\n"
"				{\n"
"					int otherIndex2 = sortedAxisGPU[axis][otherbuffer][j].y;\n"
"					int otherIndex = otherIndex2/2;\n"
"					if (otherIndex!=i)\n"
"					{\n"
"						bool otherIsMax = ((otherIndex2&1)!=0);\n"
"\n"
"						if (otherIsMax)\n"
"						{\n"
"									\n"
"							bool overlap = true;\n"
"\n"
"							for (int ax=0;ax<3;ax++)\n"
"							{\n"
"								if ((objectMinMaxIndexGPU[ax][m_currentBuffer][i].x > objectMinMaxIndexGPU[ax][m_currentBuffer][otherIndex].y) ||\n"
"									(objectMinMaxIndexGPU[ax][m_currentBuffer][i].y < objectMinMaxIndexGPU[ax][m_currentBuffer][otherIndex].x))\n"
"									overlap=false;\n"
"							}\n"
"\n"
"						//	b3Assert(overlap2==overlap);\n"
"\n"
"							bool prevOverlap = true;\n"
"\n"
"							for (int ax=0;ax<3;ax++)\n"
"							{\n"
"								if ((objectMinMaxIndexGPU[ax][1-m_currentBuffer][i].x > objectMinMaxIndexGPU[ax][1-m_currentBuffer][otherIndex].y) ||\n"
"									(objectMinMaxIndexGPU[ax][1-m_currentBuffer][i].y < objectMinMaxIndexGPU[ax][1-m_currentBuffer][otherIndex].x))\n"
"									prevOverlap=false;\n"
"							}\n"
"									\n"
"\n"
"							//b3Assert(overlap==overlap2);\n"
"								\n"
"\n"
"\n"
"							if (dmin<0)\n"
"							{\n"
"								if (overlap && !prevOverlap)\n"
"								{\n"
"									//add a pair\n"
"									int2 newPair;\n"
"									if (i<=otherIndex)\n"
"									{\n"
"										newPair.x = i;\n"
"										newPair.y = otherIndex;\n"
"									} else\n"
"									{\n"
"										newPair.x = otherIndex;\n"
"										newPair.y = i;\n"
"									}\n"
"									\n"
"									{\n"
"										int curPair = atomic_inc(addedHostPairsCount);\n"
"										if (curPair<maxCapacity)\n"
"										{\n"
"											\n"
"											addedHostPairsGPU[curPair].x = newPair.x;\n"
"											addedHostPairsGPU[curPair].y = newPair.y;\n"
"										}\n"
"									}\n"
"\n"
"								}\n"
"							} \n"
"							else\n"
"							{\n"
"								if (!overlap && prevOverlap)\n"
"								{\n"
"									\n"
"									//remove a pair\n"
"									int2 removedPair;\n"
"									if (i<=otherIndex)\n"
"									{\n"
"										removedPair.x = i;\n"
"										removedPair.y = otherIndex;\n"
"									} else\n"
"									{\n"
"										removedPair.x = otherIndex;\n"
"										removedPair.y = i;\n"
"									}\n"
"									{\n"
"										int curPair = atomic_inc(removedHostPairsCount);\n"
"										if (curPair<maxCapacity)\n"
"										{\n"
"											\n"
"											removedHostPairsGPU[curPair].x = removedPair.x;\n"
"											removedHostPairsGPU[curPair].y = removedPair.y;\n"
"\n"
"										}\n"
"									}\n"
"								}\n"
"							}//otherisMax\n"
"						}//if (dmin<0)\n"
"					}//if (otherIndex!=i)\n"
"				}//for (int j=\n"
"			}\n"
"				\n"
"			if (dmax!=0)\n"
"			{\n"
"				int stepMax = dmax<0 ? -1 : 1;\n"
"				for (int j=prevMaxIndex;j!=curMaxIndex;j+=stepMax)\n"
"				{\n"
"					int otherIndex2 = sortedAxisGPU[axis][otherbuffer][j].y;\n"
"					int otherIndex = otherIndex2/2;\n"
"					if (otherIndex!=i)\n"
"					{\n"
"						bool otherIsMin = ((otherIndex2&1)==0);\n"
"						if (otherIsMin)\n"
"						{\n"
"									\n"
"							bool overlap = true;\n"
"\n"
"							for (int ax=0;ax<3;ax++)\n"
"							{\n"
"								if ((objectMinMaxIndexGPU[ax][m_currentBuffer][i].x > objectMinMaxIndexGPU[ax][m_currentBuffer][otherIndex].y) ||\n"
"									(objectMinMaxIndexGPU[ax][m_currentBuffer][i].y < objectMinMaxIndexGPU[ax][m_currentBuffer][otherIndex].x))\n"
"									overlap=false;\n"
"							}\n"
"							//b3Assert(overlap2==overlap);\n"
"\n"
"							bool prevOverlap = true;\n"
"\n"
"							for (int ax=0;ax<3;ax++)\n"
"							{\n"
"								if ((objectMinMaxIndexGPU[ax][1-m_currentBuffer][i].x > objectMinMaxIndexGPU[ax][1-m_currentBuffer][otherIndex].y) ||\n"
"									(objectMinMaxIndexGPU[ax][1-m_currentBuffer][i].y < objectMinMaxIndexGPU[ax][1-m_currentBuffer][otherIndex].x))\n"
"									prevOverlap=false;\n"
"							}\n"
"									\n"
"\n"
"							if (dmax>0)\n"
"							{\n"
"								if (overlap && !prevOverlap)\n"
"								{\n"
"									//add a pair\n"
"									int2 newPair;\n"
"									if (i<=otherIndex)\n"
"									{\n"
"										newPair.x = i;\n"
"										newPair.y = otherIndex;\n"
"									} else\n"
"									{\n"
"										newPair.x = otherIndex;\n"
"										newPair.y = i;\n"
"									}\n"
"									{\n"
"										int curPair = atomic_inc(addedHostPairsCount);\n"
"										if (curPair<maxCapacity)\n"
"										{\n"
"											\n"
"											addedHostPairsGPU[curPair].x = newPair.x;\n"
"											addedHostPairsGPU[curPair].y = newPair.y;\n"
"										}\n"
"									}\n"
"							\n"
"								}\n"
"							} \n"
"							else\n"
"							{\n"
"								if (!overlap && prevOverlap)\n"
"								{\n"
"									//if (otherIndex2&1==0) -> min?\n"
"									//remove a pair\n"
"									int2 removedPair;\n"
"									if (i<=otherIndex)\n"
"									{\n"
"										removedPair.x = i;\n"
"										removedPair.y = otherIndex;\n"
"									} else\n"
"									{\n"
"										removedPair.x = otherIndex;\n"
"										removedPair.y = i;\n"
"									}\n"
"									{\n"
"										int curPair = atomic_inc(removedHostPairsCount);\n"
"										if (curPair<maxCapacity)\n"
"										{\n"
"											\n"
"											removedHostPairsGPU[curPair].x = removedPair.x;\n"
"											removedHostPairsGPU[curPair].y = removedPair.y;\n"
"										}\n"
"									}\n"
"								\n"
"								}\n"
"							}\n"
"							\n"
"						}//if (dmin<0)\n"
"					}//if (otherIndex!=i)\n"
"				}//for (int j=\n"
"			}\n"
"		}//for (int otherbuffer\n"
"	}//for (int axis=0;\n"
"\n"
"\n"
"}\n"
"\n"
"//computePairsKernelBatchWrite\n"
"__kernel void   computePairsKernel( __global const btAabbCL* aabbs, volatile __global int2* pairsOut,volatile  __global int* pairCount, int numObjects, int axis, int maxPairs)\n"
"{\n"
"	int i = get_global_id(0);\n"
"	int localId = get_local_id(0);\n"
"\n"
"	__local int numActiveWgItems[1];\n"
"	__local int breakRequest[1];\n"
"	__local btAabbCL localAabbs[128];// = aabbs[i];\n"
"	\n"
"	int2 myPairs[64];\n"
"	\n"
"	btAabbCL myAabb;\n"
"	\n"
"	myAabb = (i<numObjects)? aabbs[i]:aabbs[0];\n"
"	float testValue = 	myAabb.m_maxElems[axis];\n"
"	\n"
"	if (localId==0)\n"
"	{\n"
"		numActiveWgItems[0] = 0;\n"
"		breakRequest[0] = 0;\n"
"	}\n"
"	int localCount=0;\n"
"	int block=0;\n"
"	localAabbs[localId] = (i+block)<numObjects? aabbs[i+block] : aabbs[0];\n"
"	localAabbs[localId+64] = (i+block+64)<numObjects? aabbs[i+block+64]: aabbs[0];\n"
"	\n"
"	barrier(CLK_LOCAL_MEM_FENCE);\n"
"	atomic_inc(numActiveWgItems);\n"
"	barrier(CLK_LOCAL_MEM_FENCE);\n"
"	int localBreak = 0;\n"
"	int curNumPairs = 0;\n"
"	\n"
"	int j=i+1;\n"
"	do\n"
"	{\n"
"		barrier(CLK_LOCAL_MEM_FENCE);\n"
"	\n"
"		if (j<numObjects)\n"
"		{\n"
"	  	if(testValue < (localAabbs[localCount+localId+1].m_minElems[axis])) \n"
"			{\n"
"				if (!localBreak)\n"
"				{\n"
"					atomic_inc(breakRequest);\n"
"					localBreak = 1;\n"
"				}\n"
"			}\n"
"		}\n"
"		\n"
"		barrier(CLK_LOCAL_MEM_FENCE);\n"
"		\n"
"		if (j>=numObjects && !localBreak)\n"
"		{\n"
"			atomic_inc(breakRequest);\n"
"			localBreak = 1;\n"
"		}\n"
"		barrier(CLK_LOCAL_MEM_FENCE);\n"
"		\n"
"		if (!localBreak)\n"
"		{\n"
"			if (TestAabbAgainstAabb2(&myAabb,&localAabbs[localCount+localId+1]))\n"
"			{\n"
"				int2 myPair;\n"
"				myPair.x = myAabb.m_minIndices[3];\n"
"				myPair.y = localAabbs[localCount+localId+1].m_minIndices[3];\n"
"				myPairs[curNumPairs] = myPair;\n"
"				curNumPairs++;\n"
"				if (curNumPairs==64)\n"
"				{\n"
"					int curPair = atomic_add(pairCount,curNumPairs);\n"
"					//avoid a buffer overrun\n"
"					if ((curPair+curNumPairs)<maxPairs)\n"
"					{\n"
"						for (int p=0;p<curNumPairs;p++)\n"
"						{\n"
"							pairsOut[curPair+p] = myPairs[p]; //flush to main memory\n"
"						}\n"
"					}\n"
"					curNumPairs = 0;\n"
"				}\n"
"			}\n"
"		}\n"
"		barrier(CLK_LOCAL_MEM_FENCE);\n"
"		\n"
"		localCount++;\n"
"		if (localCount==64)\n"
"		{\n"
"			localCount = 0;\n"
"			block+=64;			\n"
"			localAabbs[localId] = ((i+block)<numObjects) ? aabbs[i+block] : aabbs[0];\n"
"			localAabbs[localId+64] = ((i+64+block)<numObjects) ? aabbs[i+block+64] : aabbs[0];\n"
"		}\n"
"		j++;\n"
"		\n"
"	} while (breakRequest[0]<numActiveWgItems[0]);\n"
"	\n"
"	\n"
"	if (curNumPairs>0)\n"
"	{\n"
"		//avoid a buffer overrun\n"
"		int curPair = atomic_add(pairCount,curNumPairs);\n"
"		if ((curPair+curNumPairs)<maxPairs)\n"
"		{\n"
"			for (int p=0;p<curNumPairs;p++)\n"
"			{\n"
"					pairsOut[curPair+p] = myPairs[p]; //flush to main memory\n"
"			}\n"
"		}\n"
"		curNumPairs = 0;\n"
"	}\n"
"}\n"
;
