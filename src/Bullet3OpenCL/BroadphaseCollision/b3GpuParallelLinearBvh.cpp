/*
This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/
//Initial Author Jackson Lee, 2014

#include "Bullet3OpenCL/Initialize/b3OpenCLUtils.h"
#include "Bullet3OpenCL/ParallelPrimitives/b3LauncherCL.h"

#include "b3GpuParallelLinearBvh.h"

b3GpuParallelLinearBvh::b3GpuParallelLinearBvh(cl_context context, cl_device_id device, cl_command_queue queue) :
	m_queue(queue),
	m_fill(context, device, queue),
	m_radixSorter(context, device, queue),
	
	m_rootNodeIndex(context, queue),
	
	m_numNodesPerLevelGpu(context, queue),
	m_firstIndexOffsetPerLevelGpu(context, queue),
	
	m_internalNodeAabbs(context, queue),
	m_internalNodeLeafIndexRanges(context, queue),
	m_internalNodeChildNodes(context, queue),
	m_internalNodeParentNodes(context, queue),
	
	m_leafNodeParentNodes(context, queue),
	m_mortonCodesAndAabbIndicies(context, queue),
	m_mergedAabb(context, queue),
	m_leafNodeAabbs(context, queue)
{
	m_rootNodeIndex.resize(1);

	//
	const char CL_PROGRAM_PATH[] = "src/Bullet3OpenCL/BroadphaseCollision/kernels/parallelLinearBvh.cl";
	
	const char* kernelSource = parallelLinearBvhCL;	//parallelLinearBvhCL.h
	cl_int error;
	char* additionalMacros = 0;
	m_parallelLinearBvhProgram = b3OpenCLUtils::compileCLProgramFromString(context, device, kernelSource, &error, additionalMacros, CL_PROGRAM_PATH);
	b3Assert(m_parallelLinearBvhProgram);
	
	m_findAllNodesMergedAabbKernel = b3OpenCLUtils::compileCLKernelFromString( context, device, kernelSource, "findAllNodesMergedAabb", &error, m_parallelLinearBvhProgram, additionalMacros );
	b3Assert(m_findAllNodesMergedAabbKernel);
	m_assignMortonCodesAndAabbIndiciesKernel = b3OpenCLUtils::compileCLKernelFromString( context, device, kernelSource, "assignMortonCodesAndAabbIndicies", &error, m_parallelLinearBvhProgram, additionalMacros );
	b3Assert(m_assignMortonCodesAndAabbIndiciesKernel);
	
	m_constructBinaryTreeKernel = b3OpenCLUtils::compileCLKernelFromString( context, device, kernelSource, "constructBinaryTree", &error, m_parallelLinearBvhProgram, additionalMacros );
	b3Assert(m_constructBinaryTreeKernel);
	m_determineInternalNodeAabbsKernel = b3OpenCLUtils::compileCLKernelFromString( context, device, kernelSource, "determineInternalNodeAabbs", &error, m_parallelLinearBvhProgram, additionalMacros );
	b3Assert(m_determineInternalNodeAabbsKernel);
	
	m_plbvhCalculateOverlappingPairsKernel = b3OpenCLUtils::compileCLKernelFromString( context, device, kernelSource, "plbvhCalculateOverlappingPairs", &error, m_parallelLinearBvhProgram, additionalMacros );
	b3Assert(m_plbvhCalculateOverlappingPairsKernel);
	m_plbvhRayTraverseKernel = b3OpenCLUtils::compileCLKernelFromString( context, device, kernelSource, "plbvhRayTraverse", &error, m_parallelLinearBvhProgram, additionalMacros );
	b3Assert(m_plbvhRayTraverseKernel);
}

b3GpuParallelLinearBvh::~b3GpuParallelLinearBvh() 
{
	clReleaseKernel(m_findAllNodesMergedAabbKernel);
	clReleaseKernel(m_assignMortonCodesAndAabbIndiciesKernel);
	clReleaseKernel(m_constructBinaryTreeKernel);
	clReleaseKernel(m_determineInternalNodeAabbsKernel);
	
	clReleaseKernel(m_plbvhCalculateOverlappingPairsKernel);
	clReleaseKernel(m_plbvhRayTraverseKernel);
	
	clReleaseProgram(m_parallelLinearBvhProgram);
}

void b3GpuParallelLinearBvh::build(const b3OpenCLArray<b3SapAabb>& worldSpaceAabbs)
{
	B3_PROFILE("b3ParallelLinearBvh::build()");
	
	m_leafNodeAabbs.copyFromOpenCLArray(worldSpaceAabbs);
	
	//
	int numLeaves = m_leafNodeAabbs.size();	//Number of leaves in the BVH == Number of rigid body AABBs
	int numInternalNodes = numLeaves - 1;
	
	if(numLeaves < 2)
	{
		int rootNodeIndex = numLeaves - 1;
		m_rootNodeIndex.copyFromHostPointer(&rootNodeIndex, 1);
		return;
	}
	
	//
	{
		m_internalNodeAabbs.resize(numInternalNodes);
		m_internalNodeLeafIndexRanges.resize(numInternalNodes);
		m_internalNodeChildNodes.resize(numInternalNodes);
		m_internalNodeParentNodes.resize(numInternalNodes);

		m_leafNodeParentNodes.resize(numLeaves);
		m_mortonCodesAndAabbIndicies.resize(numLeaves);
		m_mergedAabb.resize(numLeaves);
	}
	
	//Find the AABB of all input AABBs; this is used to define the size of 
	//each cell in the virtual grid(2^10 cells in each dimension).
	{
		B3_PROFILE("Find AABB of merged nodes");
	
		m_mergedAabb.copyFromOpenCLArray(worldSpaceAabbs);	//Need to make a copy since the kernel modifies the array
			
		for(int numAabbsNeedingMerge = numLeaves; numAabbsNeedingMerge >= 2; 
				numAabbsNeedingMerge = numAabbsNeedingMerge / 2 + numAabbsNeedingMerge % 2)
		{
			b3BufferInfoCL bufferInfo[] = 
			{
				b3BufferInfoCL( m_mergedAabb.getBufferCL() )		//Resulting AABB is stored in m_mergedAabb[0]
			};
			
			b3LauncherCL launcher(m_queue, m_findAllNodesMergedAabbKernel, "m_findAllNodesMergedAabbKernel");
			launcher.setBuffers( bufferInfo, sizeof(bufferInfo)/sizeof(b3BufferInfoCL) );
			launcher.setConst(numAabbsNeedingMerge);
			
			launcher.launch1D(numAabbsNeedingMerge);
		}
		
		clFinish(m_queue);
	}
	
	
	//Insert the center of the AABBs into a virtual grid,
	//then convert the discrete grid coordinates into a morton code
	//For each element in m_mortonCodesAndAabbIndicies, set
	//	m_key == morton code (value to sort by)
	//	m_value = AABB index
	{
		B3_PROFILE("Assign morton codes");
	
		b3BufferInfoCL bufferInfo[] = 
		{
			b3BufferInfoCL( m_leafNodeAabbs.getBufferCL() ),
			b3BufferInfoCL( m_mergedAabb.getBufferCL() ),
			b3BufferInfoCL( m_mortonCodesAndAabbIndicies.getBufferCL() )
		};
		
		b3LauncherCL launcher(m_queue, m_assignMortonCodesAndAabbIndiciesKernel, "m_assignMortonCodesAndAabbIndiciesKernel");
		launcher.setBuffers( bufferInfo, sizeof(bufferInfo)/sizeof(b3BufferInfoCL) );
		launcher.setConst(numLeaves);
		
		launcher.launch1D(numLeaves);
		clFinish(m_queue);
	}
	
	//
	{
		B3_PROFILE("Sort leaves by morton codes");
	
		m_radixSorter.execute(m_mortonCodesAndAabbIndicies);
		clFinish(m_queue);
	}
	
	//Optional; only element at m_internalNodeParentNodes[0], the root node, needs to be set here
	//as the parent indices of other nodes are overwritten during m_constructBinaryTreeKernel
	{
		B3_PROFILE("Reset parent node indices");
		
		m_fill.execute( m_internalNodeParentNodes, B3_PLBVH_ROOT_NODE_MARKER, m_internalNodeParentNodes.size() );
		m_fill.execute( m_leafNodeParentNodes, B3_PLBVH_ROOT_NODE_MARKER, m_leafNodeParentNodes.size() );
		clFinish(m_queue);
	}
	
	constructSimpleBinaryTree();
}

void b3GpuParallelLinearBvh::calculateOverlappingPairs(b3OpenCLArray<int>& out_numPairs, b3OpenCLArray<b3Int4>& out_overlappingPairs)
{
	b3Assert( out_numPairs.size() == 1 );
	
	int maxPairs = out_overlappingPairs.size();
		
	int reset = 0;
	out_numPairs.copyFromHostPointer(&reset, 1);
	
	if( m_leafNodeAabbs.size() < 2 ) return;
	
	{
		B3_PROFILE("PLBVH calculateOverlappingPairs");
	
		int numQueryAabbs = m_leafNodeAabbs.size();
		
		b3BufferInfoCL bufferInfo[] = 
		{
			b3BufferInfoCL( m_leafNodeAabbs.getBufferCL() ),
			
			b3BufferInfoCL( m_rootNodeIndex.getBufferCL() ),
			b3BufferInfoCL( m_internalNodeChildNodes.getBufferCL() ),
			b3BufferInfoCL( m_internalNodeAabbs.getBufferCL() ),
			b3BufferInfoCL( m_internalNodeLeafIndexRanges.getBufferCL() ),
			b3BufferInfoCL( m_mortonCodesAndAabbIndicies.getBufferCL() ),
			
			b3BufferInfoCL( out_numPairs.getBufferCL() ),
			b3BufferInfoCL( out_overlappingPairs.getBufferCL() )
		};
		
		b3LauncherCL launcher(m_queue, m_plbvhCalculateOverlappingPairsKernel, "m_plbvhCalculateOverlappingPairsKernel");
		launcher.setBuffers( bufferInfo, sizeof(bufferInfo)/sizeof(b3BufferInfoCL) );
		launcher.setConst(maxPairs);
		launcher.setConst(numQueryAabbs);
		
		launcher.launch1D(numQueryAabbs);
		clFinish(m_queue);
	}
	
	
	//
	int numPairs = -1;
	out_numPairs.copyToHostPointer(&numPairs, 1);
	if(numPairs > maxPairs)
	{
		b3Error("Error running out of pairs: numPairs = %d, maxPairs = %d.\n", numPairs, maxPairs);
		numPairs = maxPairs;
		out_numPairs.copyFromHostPointer(&maxPairs, 1);
	}
	
	out_overlappingPairs.resize(numPairs);
}


void b3GpuParallelLinearBvh::testRaysAgainstBvhAabbs(const b3OpenCLArray<b3RayInfo>& rays, 
							b3OpenCLArray<int>& out_numRayRigidPairs, b3OpenCLArray<b3Int2>& out_rayRigidPairs)
{
	B3_PROFILE("PLBVH testRaysAgainstBvhAabbs()");
	
	int numRays = rays.size();
	int maxRayRigidPairs = out_rayRigidPairs.size();
	
	int reset = 0;
	out_numRayRigidPairs.copyFromHostPointer(&reset, 1);
	
	if( m_leafNodeAabbs.size() < 1 ) return;
	
	b3BufferInfoCL bufferInfo[] = 
	{
		b3BufferInfoCL( m_leafNodeAabbs.getBufferCL() ),
		
		b3BufferInfoCL( m_rootNodeIndex.getBufferCL() ),
		b3BufferInfoCL( m_internalNodeChildNodes.getBufferCL() ),
		b3BufferInfoCL( m_internalNodeAabbs.getBufferCL() ),
		b3BufferInfoCL( m_internalNodeLeafIndexRanges.getBufferCL() ),
		b3BufferInfoCL( m_mortonCodesAndAabbIndicies.getBufferCL() ),
		
		b3BufferInfoCL( rays.getBufferCL() ),
		
		b3BufferInfoCL( out_numRayRigidPairs.getBufferCL() ),
		b3BufferInfoCL( out_rayRigidPairs.getBufferCL() )
	};
	
	b3LauncherCL launcher(m_queue, m_plbvhRayTraverseKernel, "m_plbvhRayTraverseKernel");
	launcher.setBuffers( bufferInfo, sizeof(bufferInfo)/sizeof(b3BufferInfoCL) );
	launcher.setConst(maxRayRigidPairs);
	launcher.setConst(numRays);
	
	launcher.launch1D(numRays);
	clFinish(m_queue);
	
	
	//
	int numRayRigidPairs = -1;
	out_numRayRigidPairs.copyToHostPointer(&numRayRigidPairs, 1);
	
	if(numRayRigidPairs > maxRayRigidPairs)
		b3Error("Error running out of rayRigid pairs: numRayRigidPairs = %d, maxRayRigidPairs = %d.\n", numRayRigidPairs, maxRayRigidPairs);
	
}

void b3GpuParallelLinearBvh::constructSimpleBinaryTree()
{
	int numLeaves = m_leafNodeAabbs.size();	//Number of leaves in the BVH == Number of rigid body AABBs
	int numInternalNodes = numLeaves - 1;
	
	//Determine number of levels in the binary tree( numLevels = ceil( log2(numLeaves) ) )
	//The number of levels is equivalent to the number of bits needed to uniquely identify each node(including both internal and leaf nodes)
	int numLevels = 0;
	{
		//Find the most significant bit(msb)
		int mostSignificantBit = 0;
		{
			int temp = numLeaves;
			while(temp >>= 1) mostSignificantBit++;		//Start counting from 0 (0 and 1 have msb 0, 2 has msb 1)
		}
		numLevels = mostSignificantBit + 1;
		
		//If the number of nodes is not a power of 2(as in, can be expressed as 2^N where N is an integer), then there is 1 additional level
		if( ~(1 << mostSignificantBit) & numLeaves ) numLevels++;
	}
	
	//Determine number of internal nodes per level, use prefix sum to get offsets of each level, and send to GPU
	{
		B3_PROFILE("Determine number of nodes per level");
		
		m_numNodesPerLevelCpu.resize(numLevels);
		
		//The last level contains the leaf nodes; number of leaves is already known
		if(numLevels - 1 >= 0) m_numNodesPerLevelCpu[numLevels - 1] = numLeaves;
		
		//Calculate number of nodes in each level; 
		//start from the second to last level(level right next to leaf nodes) and move towards the root(level 0)
		int remainder = 0;
		for(int levelIndex = numLevels - 2; levelIndex >= 0; --levelIndex)
		{
			int numNodesPreviousLevel = m_numNodesPerLevelCpu[levelIndex + 1];		//For first iteration this == numLeaves
			int numNodesCurrentLevel = numNodesPreviousLevel / 2;
			
			remainder += numNodesPreviousLevel % 2;
			if(remainder == 2) 
			{
				numNodesCurrentLevel++;
				remainder = 0;
			}
			
			m_numNodesPerLevelCpu[levelIndex] = numNodesCurrentLevel;
		}
		
		//Prefix sum to calculate the first index offset of each level
		{
			m_firstIndexOffsetPerLevelCpu = m_numNodesPerLevelCpu;
			
			//Perform inclusive scan
			for(int i = 1; i < m_firstIndexOffsetPerLevelCpu.size(); ++i) 
				m_firstIndexOffsetPerLevelCpu[i] += m_firstIndexOffsetPerLevelCpu[i - 1];
			
			//Convert inclusive scan to exclusive scan to get the offsets
			//This is equivalent to shifting each element in m_firstIndexOffsetPerLevelCpu[] by 1 to the right, 
			//and setting the first element to 0
			for(int i = 0; i < m_firstIndexOffsetPerLevelCpu.size(); ++i) 
				m_firstIndexOffsetPerLevelCpu[i] -= m_numNodesPerLevelCpu[i];
		}
		
		//Copy to GPU
		m_numNodesPerLevelGpu.copyFromHost(m_numNodesPerLevelCpu, false);
		m_firstIndexOffsetPerLevelGpu.copyFromHost(m_firstIndexOffsetPerLevelCpu, false);
		clFinish(m_queue);
	}
	
	//Construct binary tree; find the children of each internal node, and assign parent nodes
	{
		B3_PROFILE("Construct binary tree");

		const int ROOT_NODE_INDEX = 0x80000000;		//Default root index is 0, most significant bit is set to indicate internal node
		m_rootNodeIndex.copyFromHostPointer(&ROOT_NODE_INDEX, 1);
	
		b3BufferInfoCL bufferInfo[] = 
		{
			b3BufferInfoCL( m_firstIndexOffsetPerLevelGpu.getBufferCL() ),
			b3BufferInfoCL( m_numNodesPerLevelGpu.getBufferCL() ),
			b3BufferInfoCL( m_internalNodeChildNodes.getBufferCL() ),
			b3BufferInfoCL( m_internalNodeParentNodes.getBufferCL() ),
			b3BufferInfoCL( m_leafNodeParentNodes.getBufferCL() )
		};
		
		b3LauncherCL launcher(m_queue, m_constructBinaryTreeKernel, "m_constructBinaryTreeKernel");
		launcher.setBuffers( bufferInfo, sizeof(bufferInfo)/sizeof(b3BufferInfoCL) );
		launcher.setConst(numLevels);
		launcher.setConst(numInternalNodes);
		
		launcher.launch1D(numInternalNodes);
		clFinish(m_queue);
	}
	
	//For each internal node, check children to get its AABB; start from the 
	//last level, which contains the leaves, and move towards the root
	{
		B3_PROFILE("Set AABBs");
	
		//Due to the arrangement of internal nodes, each internal node corresponds 
		//to a contiguous range of leaf node indices. This characteristic can be used
		//to optimize calculateOverlappingPairs(); checking if
		//(m_internalNodeLeafIndexRanges[].y < leafNodeIndex) can be used to ensure that
		//each pair is processed only once.
		{
			B3_PROFILE("Reset internal node index ranges");
			
			b3Int2 invalidIndexRange;
			invalidIndexRange.x = -1; 	//x == min
			invalidIndexRange.y = -2;	//y == max
			
			m_fill.execute( m_internalNodeLeafIndexRanges, invalidIndexRange, m_internalNodeLeafIndexRanges.size() );
			clFinish(m_queue);
		}
		
		int lastInternalLevelIndex = numLevels - 2;		//Last level is leaf node level
		for(int level = lastInternalLevelIndex; level >= 0; --level)
		{
			b3BufferInfoCL bufferInfo[] = 
			{
				b3BufferInfoCL( m_firstIndexOffsetPerLevelGpu.getBufferCL() ),
				b3BufferInfoCL( m_numNodesPerLevelGpu.getBufferCL() ),
				b3BufferInfoCL( m_internalNodeChildNodes.getBufferCL() ),
				b3BufferInfoCL( m_mortonCodesAndAabbIndicies.getBufferCL() ),
				b3BufferInfoCL( m_leafNodeAabbs.getBufferCL() ),
				b3BufferInfoCL( m_internalNodeLeafIndexRanges.getBufferCL() ),
				b3BufferInfoCL( m_internalNodeAabbs.getBufferCL() )
			};
			
			b3LauncherCL launcher(m_queue, m_determineInternalNodeAabbsKernel, "m_determineInternalNodeAabbsKernel");
			launcher.setBuffers( bufferInfo, sizeof(bufferInfo)/sizeof(b3BufferInfoCL) );
			launcher.setConst(numLevels);
			launcher.setConst(numInternalNodes);
			launcher.setConst(level);
			
			launcher.launch1D(numLeaves);
		}
		clFinish(m_queue);
	}
}


void b3GpuParallelLinearBvh::constructRadixBinaryTree()
{
}

	