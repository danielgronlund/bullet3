//this file is autogenerated using stringify.bat (premake --stringify) in the build folder of this project
static const char* batchingKernelsNewCL= \
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
"#pragma OPENCL EXTENSION cl_amd_printf : enable\n"
"#pragma OPENCL EXTENSION cl_khr_local_int32_base_atomics : enable\n"
"#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable\n"
"#pragma OPENCL EXTENSION cl_khr_local_int32_extended_atomics : enable\n"
"#pragma OPENCL EXTENSION cl_khr_global_int32_extended_atomics : enable\n"
"\n"
"#ifdef cl_ext_atomic_counters_32\n"
"#pragma OPENCL EXTENSION cl_ext_atomic_counters_32 : enable\n"
"#else\n"
"#define counter32_t volatile __global int*\n"
"#endif\n"
"\n"
"#define SIMD_WIDTH 64\n"
"\n"
"typedef unsigned int u32;\n"
"typedef unsigned short u16;\n"
"typedef unsigned char u8;\n"
"\n"
"#define GET_GROUP_IDX get_group_id(0)\n"
"#define GET_LOCAL_IDX get_local_id(0)\n"
"#define GET_GLOBAL_IDX get_global_id(0)\n"
"#define GET_GROUP_SIZE get_local_size(0)\n"
"#define GET_NUM_GROUPS get_num_groups(0)\n"
"#define GROUP_LDS_BARRIER barrier(CLK_LOCAL_MEM_FENCE)\n"
"#define GROUP_MEM_FENCE mem_fence(CLK_LOCAL_MEM_FENCE)\n"
"#define AtomInc(x) atom_inc(&(x))\n"
"#define AtomInc1(x, out) out = atom_inc(&(x))\n"
"#define AppendInc(x, out) out = atomic_inc(x)\n"
"#define AtomAdd(x, value) atom_add(&(x), value)\n"
"#define AtomCmpxhg(x, cmp, value) atom_cmpxchg( &(x), cmp, value )\n"
"#define AtomXhg(x, value) atom_xchg ( &(x), value )\n"
"\n"
"\n"
"#define SELECT_UINT4( b, a, condition ) select( b,a,condition )\n"
"\n"
"#define make_float4 (float4)\n"
"#define make_float2 (float2)\n"
"#define make_uint4 (uint4)\n"
"#define make_int4 (int4)\n"
"#define make_uint2 (uint2)\n"
"#define make_int2 (int2)\n"
"\n"
"\n"
"#define max2 max\n"
"#define min2 min\n"
"\n"
"\n"
"#define WG_SIZE 64\n"
"\n"
"\n"
"\n"
"typedef struct \n"
"{\n"
"	float4 m_worldPos[4];\n"
"	float4 m_worldNormal;\n"
"	u32 m_coeffs;\n"
"	int m_batchIdx;\n"
"\n"
"	int m_bodyAPtrAndSignBit;//sign bit set for fixed objects\n"
"	int m_bodyBPtrAndSignBit;\n"
"}Contact4;\n"
"\n"
"typedef struct \n"
"{\n"
"	int m_n;\n"
"	int m_start;\n"
"	int m_staticIdx;\n"
"	int m_paddings[1];\n"
"} ConstBuffer;\n"
"\n"
"typedef struct \n"
"{\n"
"	int m_a;\n"
"	int m_b;\n"
"	u32 m_idx;\n"
"}Elem;\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"//	batching on the GPU\n"
"__kernel void CreateBatchesBruteForce( __global Contact4* gConstraints, 	__global const u32* gN, __global const u32* gStart, int m_staticIdx )\n"
"{\n"
"	int wgIdx = GET_GROUP_IDX;\n"
"	int lIdx = GET_LOCAL_IDX;\n"
"	\n"
"	const int m_n = gN[wgIdx];\n"
"	const int m_start = gStart[wgIdx];\n"
"		\n"
"	if( lIdx == 0 )\n"
"	{\n"
"		for (int i=0;i<m_n;i++)\n"
"		{\n"
"			int srcIdx = i+m_start;\n"
"			int batchIndex = i;\n"
"			gConstraints[ srcIdx ].m_batchIdx = batchIndex;	\n"
"		}\n"
"	}\n"
"}\n"
"\n"
"\n"
"#define CHECK_SIZE (WG_SIZE)\n"
"\n"
"\n"
"\n"
"\n"
"u32 readBuf(__local u32* buff, int idx)\n"
"{\n"
"	idx = idx % (32*CHECK_SIZE);\n"
"	int bitIdx = idx%32;\n"
"	int bufIdx = idx/32;\n"
"	return buff[bufIdx] & (1<<bitIdx);\n"
"}\n"
"\n"
"void writeBuf(__local u32* buff, int idx)\n"
"{\n"
"	idx = idx % (32*CHECK_SIZE);\n"
"	int bitIdx = idx%32;\n"
"	int bufIdx = idx/32;\n"
"	buff[bufIdx] |= (1<<bitIdx);\n"
"	//atom_or( &buff[bufIdx], (1<<bitIdx) );\n"
"}\n"
"\n"
"u32 tryWrite(__local u32* buff, int idx)\n"
"{\n"
"	idx = idx % (32*CHECK_SIZE);\n"
"	int bitIdx = idx%32;\n"
"	int bufIdx = idx/32;\n"
"	u32 ans = (u32)atom_or( &buff[bufIdx], (1<<bitIdx) );\n"
"	return ((ans >> bitIdx)&1) == 0;\n"
"}\n"
"\n"
"\n"
"//	batching on the GPU\n"
"__kernel void CreateBatchesNew( __global Contact4* gConstraints, __global const u32* gN, __global const u32* gStart, int staticIdx )\n"
"{\n"
"	int wgIdx = GET_GROUP_IDX;\n"
"	int lIdx = GET_LOCAL_IDX;\n"
"	const int numConstraints = gN[wgIdx];\n"
"	const int m_start = gStart[wgIdx];\n"
"		\n"
"	\n"
"	__local u32 ldsFixedBuffer[CHECK_SIZE];\n"
"		\n"
"	\n"
"	\n"
"	\n"
"	\n"
"	if( lIdx == 0 )\n"
"	{\n"
"	\n"
"		\n"
"		__global Contact4* cs = &gConstraints[m_start];	\n"
"	\n"
"		\n"
"		int numValidConstraints = 0;\n"
"		int batchIdx = 0;\n"
"\n"
"		while( numValidConstraints < numConstraints)\n"
"		{\n"
"			int nCurrentBatch = 0;\n"
"			//	clear flag\n"
"	\n"
"			for(int i=0; i<CHECK_SIZE; i++) \n"
"				ldsFixedBuffer[i] = 0;		\n"
"\n"
"			for(int i=numValidConstraints; i<numConstraints; i++)\n"
"			{\n"
"\n"
"				int bodyAS = cs[i].m_bodyAPtrAndSignBit;\n"
"				int bodyBS = cs[i].m_bodyBPtrAndSignBit;\n"
"				int bodyA = abs(bodyAS);\n"
"				int bodyB = abs(bodyBS);\n"
"				bool aIsStatic = (bodyAS<0) || bodyAS==staticIdx;\n"
"				bool bIsStatic = (bodyBS<0) || bodyBS==staticIdx;\n"
"				int aUnavailable = aIsStatic ? 0 : readBuf( ldsFixedBuffer, bodyA);\n"
"				int bUnavailable = bIsStatic ? 0 : readBuf( ldsFixedBuffer, bodyB);\n"
"				\n"
"				if( aUnavailable==0 && bUnavailable==0 ) // ok\n"
"				{\n"
"					if (!aIsStatic)\n"
"					{\n"
"						writeBuf( ldsFixedBuffer, bodyA );\n"
"					}\n"
"					if (!bIsStatic)\n"
"					{\n"
"						writeBuf( ldsFixedBuffer, bodyB );\n"
"					}\n"
"\n"
"					cs[i].m_batchIdx = batchIdx;\n"
"\n"
"					if (i!=numValidConstraints)\n"
"					{\n"
"						//btSwap(cs[i],cs[numValidConstraints]);\n"
"						\n"
"						Contact4 tmp = cs[i];\n"
"						cs[i] = cs[numValidConstraints];\n"
"						cs[numValidConstraints] = tmp;\n"
"						\n"
"					}\n"
"\n"
"					numValidConstraints++;\n"
"					\n"
"					nCurrentBatch++;\n"
"					if( nCurrentBatch == SIMD_WIDTH)\n"
"					{\n"
"						nCurrentBatch = 0;\n"
"						for(int i=0; i<CHECK_SIZE; i++) \n"
"							ldsFixedBuffer[i] = 0;\n"
"						\n"
"					}\n"
"				}\n"
"			}//for\n"
"			batchIdx ++;\n"
"		}//while\n"
"	}//if( lIdx == 0 )\n"
"	\n"
"	//return batchIdx;\n"
"}\n"
"\n"
;
