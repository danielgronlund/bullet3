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

#ifndef B3_GPU_PARALLEL_LINEAR_BVH_BROADPHASE_H
#define B3_GPU_PARALLEL_LINEAR_BVH_BROADPHASE_H

#include "Bullet3OpenCL/BroadphaseCollision/b3GpuBroadphaseInterface.h"

#include "b3GpuParallelLinearBvh.h"

class b3GpuParallelLinearBvhBroadphase : public b3GpuBroadphaseInterface
{
	b3GpuParallelLinearBvh m_plbvh;
	
	b3OpenCLArray<b3Int4> m_overlappingPairsGpu;
	b3OpenCLArray<b3SapAabb> m_aabbsGpu;
	b3OpenCLArray<int> m_tempNumPairs;
	
	b3AlignedObjectArray<b3SapAabb> m_aabbsCpu;
	
public:
	b3GpuParallelLinearBvhBroadphase(cl_context context, cl_device_id device, cl_command_queue queue) : 
		m_plbvh(context, device, queue),
		
		m_overlappingPairsGpu(context, queue),
		m_aabbsGpu(context, queue),
		m_tempNumPairs(context, queue)
	{
		m_tempNumPairs.resize(1);
	}
	virtual ~b3GpuParallelLinearBvhBroadphase() {}

	virtual void createProxy(const b3Vector3& aabbMin, const b3Vector3& aabbMax, int userPtr, short int collisionFilterGroup, short int collisionFilterMask)
	{
		b3SapAabb aabb;
		aabb.m_minVec = aabbMin;
		aabb.m_maxVec = aabbMax;
		aabb.m_minIndices[3] = userPtr;
		
		m_aabbsCpu.push_back(aabb);
	}
	virtual void createLargeProxy(const b3Vector3& aabbMin, const b3Vector3& aabbMax, int userPtr, short int collisionFilterGroup, short int collisionFilterMask)
	{
		b3Assert(0);	//Not implemented
	}
	
	virtual void calculateOverlappingPairs(int maxPairs)
	{
		//Reconstruct BVH
		m_plbvh.build(m_aabbsGpu);
		
		//
		m_overlappingPairsGpu.resize(maxPairs);
		m_plbvh.calculateOverlappingPairs(m_aabbsGpu, m_tempNumPairs, m_overlappingPairsGpu);
	}
	virtual void calculateOverlappingPairsHost(int maxPairs)
	{
		b3Assert(0);	//CPU version not implemented
	}

	//call writeAabbsToGpu after done making all changes (createProxy etc)
	virtual void writeAabbsToGpu() { m_aabbsGpu.copyFromHost(m_aabbsCpu); }
	
	virtual int	getNumOverlap() { return m_overlappingPairsGpu.size(); }
	virtual cl_mem getOverlappingPairBuffer() { return m_overlappingPairsGpu.getBufferCL(); }

	virtual cl_mem getAabbBufferWS() { return m_aabbsGpu.getBufferCL(); }
	virtual b3OpenCLArray<b3SapAabb>& getAllAabbsGPU() { return m_aabbsGpu; }
	
	virtual b3AlignedObjectArray<b3SapAabb>& getAllAabbsCPU() { return m_aabbsCpu; }
	
	static b3GpuBroadphaseInterface* CreateFunc(cl_context context, cl_device_id device, cl_command_queue queue)
	{
		return new b3GpuParallelLinearBvhBroadphase(context, device, queue);
	}
};

#endif
