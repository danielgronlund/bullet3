/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#include "btOptimizedBvh.h"
#include "btStridingMeshInterface.h"
#include "LinearMath/btAabbUtil2.h"


btOptimizedBvh::btOptimizedBvh() : m_contiguousNodes(0), m_useQuantization(false)
{ 

}


void btOptimizedBvh::build(btStridingMeshInterface* triangles, bool useQuantizedAabbCompression)
{
	m_useQuantization = useQuantizedAabbCompression;


	// NodeArray	triangleNodes;

	struct	NodeTriangleCallback : public btInternalTriangleIndexCallback
	{
		NodeArray&	m_triangleNodes;

		NodeTriangleCallback(NodeArray&	triangleNodes)
			:m_triangleNodes(triangleNodes)
		{
		}

		virtual void internalProcessTriangleIndex(btVector3* triangle,int partId,int  triangleIndex)
		{
			btOptimizedBvhNode node;
			btVector3	aabbMin,aabbMax;
			aabbMin.setValue(btScalar(1e30),btScalar(1e30),btScalar(1e30));
			aabbMax.setValue(btScalar(-1e30),btScalar(-1e30),btScalar(-1e30)); 
			aabbMin.setMin(triangle[0]);
			aabbMax.setMax(triangle[0]);
			aabbMin.setMin(triangle[1]);
			aabbMax.setMax(triangle[1]);
			aabbMin.setMin(triangle[2]);
			aabbMax.setMax(triangle[2]);

			//with quantization?
			node.m_aabbMinOrg = aabbMin;
			node.m_aabbMaxOrg = aabbMax;

			node.m_escapeIndex = -1;
	
			//for child nodes
			node.m_subPart = partId;
			node.m_triangleIndex = triangleIndex;
			m_triangleNodes.push_back(node);
		}
	};
	struct	QuantizedNodeTriangleCallback : public btInternalTriangleIndexCallback
	{
		QuantizedNodeArray&	m_triangleNodes;
		const btOptimizedBvh* m_optimizedTree; // for quantization

		QuantizedNodeTriangleCallback(QuantizedNodeArray&	triangleNodes,const btOptimizedBvh* tree)
			:m_triangleNodes(triangleNodes),m_optimizedTree(tree)
		{
		}

		virtual void internalProcessTriangleIndex(btVector3* triangle,int partId,int  triangleIndex)
		{
			btAssert(partId==0);
			//negative indices are reserved for escapeIndex
			btAssert(triangleIndex>=0);

			btQuantizedBvhNode node;
			btVector3	aabbMin,aabbMax;
			aabbMin.setValue(btScalar(1e30),btScalar(1e30),btScalar(1e30));
			aabbMax.setValue(btScalar(-1e30),btScalar(-1e30),btScalar(-1e30)); 
			aabbMin.setMin(triangle[0]);
			aabbMax.setMax(triangle[0]);
			aabbMin.setMin(triangle[1]);
			aabbMax.setMax(triangle[1]);
			aabbMin.setMin(triangle[2]);
			aabbMax.setMax(triangle[2]);

			m_optimizedTree->quantizeWithClamp(&node.m_quantizedAabbMin[0],aabbMin);
			m_optimizedTree->quantizeWithClamp(&node.m_quantizedAabbMax[0],aabbMax);

			node.m_escapeIndexOrTriangleIndex = triangleIndex;

			m_triangleNodes.push_back(node);
		}
	};
	


	int numLeafNodes = 0;

	
	if (m_useQuantization)
	{

		initQuantizationValues(triangles);

		QuantizedNodeTriangleCallback	callback(m_quantizedLeafNodes,this);

	
		triangles->InternalProcessAllTriangles(&callback,m_bvhAabbMin,m_bvhAabbMax);

		//now we have an array of leafnodes in m_leafNodes
		numLeafNodes = m_quantizedLeafNodes.size();


		m_quantizedContiguousNodes.resize(2*numLeafNodes);


	} else
	{
		NodeTriangleCallback	callback(m_leafNodes);

		btVector3 aabbMin(btScalar(-1e30),btScalar(-1e30),btScalar(-1e30));
		btVector3 aabbMax(btScalar(1e30),btScalar(1e30),btScalar(1e30));

		triangles->InternalProcessAllTriangles(&callback,aabbMin,aabbMax);

		//now we have an array of leafnodes in m_leafNodes
		numLeafNodes = m_leafNodes.size();

		m_contiguousNodes = new btOptimizedBvhNode[2*numLeafNodes];
	}

	m_curNodeIndex = 0;

	buildTree(0,numLeafNodes);

}


void	btOptimizedBvh::refit(btStridingMeshInterface* meshInterface)
{
	if (m_useQuantization)
	{
		int nodeSubPart=0;

		initQuantizationValues(meshInterface);

		//get access info to trianglemesh data
		const unsigned char *vertexbase;
		int numverts;
		PHY_ScalarType type;
		int stride;
		const unsigned char *indexbase;
		int indexstride;
		int numfaces;
		PHY_ScalarType indicestype;
		meshInterface->getLockedReadOnlyVertexIndexBase(&vertexbase,numverts,	type,stride,&indexbase,indexstride,numfaces,indicestype,nodeSubPart);


		int numNodes = m_curNodeIndex;
		int i;
		for (i=numNodes-1;i>=0;i--)
		{
			btVector3	triangleVerts[3];

			btQuantizedBvhNode& curNode = m_quantizedContiguousNodes[i];
			if (curNode.isLeafNode())
			{
				//recalc aabb from triangle data
				int nodeTriangleIndex = curNode.getTriangleIndex();
				//triangles->getLockedReadOnlyVertexIndexBase(vertexBase,numVerts,

				int* gfxbase = (int*)(indexbase+nodeTriangleIndex*indexstride);
				
				const btVector3& meshScaling = meshInterface->getScaling();
				for (int j=2;j>=0;j--)
				{
					
					int graphicsindex = gfxbase[j];
					btScalar* graphicsbase = (btScalar*)(vertexbase+graphicsindex*stride);

					triangleVerts[j] = btVector3(
						graphicsbase[0]*meshScaling.getX(),
						graphicsbase[1]*meshScaling.getY(),
						graphicsbase[2]*meshScaling.getZ());
				}


				btVector3	aabbMin,aabbMax;
				aabbMin.setValue(btScalar(1e30),btScalar(1e30),btScalar(1e30));
				aabbMax.setValue(btScalar(-1e30),btScalar(-1e30),btScalar(-1e30)); 
				aabbMin.setMin(triangleVerts[0]);
				aabbMax.setMax(triangleVerts[0]);
				aabbMin.setMin(triangleVerts[1]);
				aabbMax.setMax(triangleVerts[1]);
				aabbMin.setMin(triangleVerts[2]);
				aabbMax.setMax(triangleVerts[2]);

				quantizeWithClamp(&curNode.m_quantizedAabbMin[0],aabbMin);
				quantizeWithClamp(&curNode.m_quantizedAabbMax[0],aabbMax);
				int k;
				k=0;

			} else
			{
				//combine aabb from both children

				btQuantizedBvhNode* leftChildNode = &m_quantizedContiguousNodes[i+1];
				
				btQuantizedBvhNode* rightChildNode = leftChildNode->isLeafNode() ? &m_quantizedContiguousNodes[i+2] :
					&m_quantizedContiguousNodes[i+1+leftChildNode->getEscapeIndex()];
				int k;
				k=0;

				{
					for (int i=0;i<3;i++)
					{
						curNode.m_quantizedAabbMin[i] = leftChildNode->m_quantizedAabbMin[i];
						if (curNode.m_quantizedAabbMin[i]>rightChildNode->m_quantizedAabbMin[i])
							curNode.m_quantizedAabbMin[i]=rightChildNode->m_quantizedAabbMin[i];

						curNode.m_quantizedAabbMax[i] = leftChildNode->m_quantizedAabbMax[i];
						if (curNode.m_quantizedAabbMax[i] < rightChildNode->m_quantizedAabbMax[i])
							curNode.m_quantizedAabbMax[i] = rightChildNode->m_quantizedAabbMax[i];
					}
				}
			}

		}

		meshInterface->unLockReadOnlyVertexBase(nodeSubPart);

	} else
	{

	}

}


void	btOptimizedBvh::initQuantizationValues(btStridingMeshInterface* triangles)
{

	struct	AabbCalculationCallback : public btInternalTriangleIndexCallback
	{
		btVector3	m_aabbMin;
		btVector3	m_aabbMax;

		AabbCalculationCallback()
		{
			m_aabbMin.setValue(1e30,1e30,1e30);
			m_aabbMax.setValue(-1e30,-1e30,-1e30);
		}

		virtual void internalProcessTriangleIndex(btVector3* triangle,int partId,int  triangleIndex)
		{
			m_aabbMin.setMin(triangle[0]);
			m_aabbMax.setMax(triangle[0]);
			m_aabbMin.setMin(triangle[1]);
			m_aabbMax.setMax(triangle[1]);
			m_aabbMin.setMin(triangle[2]);
			m_aabbMax.setMax(triangle[2]);
		}
	};

		//first calculate the total aabb for all triangles
	AabbCalculationCallback	aabbCallback;
	btVector3 aabbMin(btScalar(-1e30),btScalar(-1e30),btScalar(-1e30));
	btVector3 aabbMax(btScalar(1e30),btScalar(1e30),btScalar(1e30));
	triangles->InternalProcessAllTriangles(&aabbCallback,aabbMin,aabbMax);

	//initialize quantization values
	m_bvhAabbMin = aabbCallback.m_aabbMin;
	m_bvhAabbMax = aabbCallback.m_aabbMax;
	btVector3 aabbSize = m_bvhAabbMax - m_bvhAabbMin;
	m_bvhQuantization = btVector3(btScalar(65535.0),btScalar(65535.0),btScalar(65535.0)) / aabbSize;


}
btOptimizedBvh::~btOptimizedBvh()
{
	if (m_contiguousNodes)
		delete []m_contiguousNodes;
}

#ifdef DEBUG_TREE_BUILDING
int gStackDepth = 0;
int gMaxStackDepth = 0;
#endif //DEBUG_TREE_BUILDING

void	btOptimizedBvh::buildTree	(int startIndex,int endIndex)
{
#ifdef DEBUG_TREE_BUILDING
	gStackDepth++;
	if (gStackDepth > gMaxStackDepth)
		gMaxStackDepth = gStackDepth;
#endif //DEBUG_TREE_BUILDING


	int splitAxis, splitIndex, i;
	int numIndices =endIndex-startIndex;
	int curIndex = m_curNodeIndex;

	assert(numIndices>0);

	if (numIndices==1)
	{
#ifdef DEBUG_TREE_BUILDING
		gStackDepth--;
#endif //DEBUG_TREE_BUILDING
		
		assignInternalNodeFromLeafNode(m_curNodeIndex,startIndex);

		m_curNodeIndex++;
		return;	
	}
	//calculate Best Splitting Axis and where to split it. Sort the incoming 'leafNodes' array within range 'startIndex/endIndex'.
	
	splitAxis = calcSplittingAxis(startIndex,endIndex);

	splitIndex = sortAndCalcSplittingIndex(startIndex,endIndex,splitAxis);

	int internalNodeIndex = m_curNodeIndex;
	
	setInternalNodeAabbMax(m_curNodeIndex,btVector3(btScalar(-1e30),btScalar(-1e30),btScalar(-1e30)));
	setInternalNodeAabbMin(m_curNodeIndex,btVector3(btScalar(1e30),btScalar(1e30),btScalar(1e30)));
	
	for (i=startIndex;i<endIndex;i++)
	{
		mergeInternalNodeAabb(m_curNodeIndex,getAabbMin(i),getAabbMax(i));
	}

	m_curNodeIndex++;
	

	//internalNode->m_escapeIndex;
	
	//build left child tree
	buildTree(startIndex,splitIndex);

	//build right child tree
	buildTree(splitIndex,endIndex);

#ifdef DEBUG_TREE_BUILDING
	gStackDepth--;
#endif //DEBUG_TREE_BUILDING
	
	setInternalNodeEscapeIndex(internalNodeIndex,m_curNodeIndex - curIndex);

}

int	btOptimizedBvh::sortAndCalcSplittingIndex(int startIndex,int endIndex,int splitAxis)
{
	int i;
	int splitIndex =startIndex;
	int numIndices = endIndex - startIndex;
	btScalar splitValue;

	btVector3 means(btScalar(0.),btScalar(0.),btScalar(0.));
	for (i=startIndex;i<endIndex;i++)
	{
		btVector3 center = btScalar(0.5)*(getAabbMax(i)+getAabbMin(i));
		means+=center;
	}
	means *= (btScalar(1.)/(btScalar)numIndices);
	
	splitValue = means[splitAxis];
	
	//sort leafNodes so all values larger then splitValue comes first, and smaller values start from 'splitIndex'.
	for (i=startIndex;i<endIndex;i++)
	{
		btVector3 center = btScalar(0.5)*(getAabbMax(i)+getAabbMin(i));
		if (center[splitAxis] > splitValue)
		{
			//swap
			swapLeafNodes(i,splitIndex);
			splitIndex++;
		}
	}

	//if the splitIndex causes unbalanced trees, fix this by using the center in between startIndex and endIndex
	//otherwise the tree-building might fail due to stack-overflows in certain cases.
	//unbalanced1 is unsafe: it can cause stack overflows
	//bool unbalanced1 = ((splitIndex==startIndex) || (splitIndex == (endIndex-1)));

	//unbalanced2 should work too: always use center (perfect balanced trees)	
	//bool unbalanced2 = true;

	//this should be safe too:
	int rangeBalancedIndices = numIndices/3;
	bool unbalanced = ((splitIndex<=(startIndex+rangeBalancedIndices)) || (splitIndex >=(endIndex-1-rangeBalancedIndices)));
	
	if (unbalanced)
	{
		splitIndex = startIndex+ (numIndices>>1);
	}

	bool unbal = (splitIndex==startIndex) || (splitIndex == (endIndex));
	btAssert(!unbal);

	return splitIndex;
}


int	btOptimizedBvh::calcSplittingAxis(int startIndex,int endIndex)
{
	int i;

	btVector3 means(btScalar(0.),btScalar(0.),btScalar(0.));
	btVector3 variance(btScalar(0.),btScalar(0.),btScalar(0.));
	int numIndices = endIndex-startIndex;

	for (i=startIndex;i<endIndex;i++)
	{
		btVector3 center = btScalar(0.5)*(getAabbMax(i)+getAabbMin(i));
		means+=center;
	}
	means *= (btScalar(1.)/(btScalar)numIndices);
		
	for (i=startIndex;i<endIndex;i++)
	{
		btVector3 center = btScalar(0.5)*(getAabbMax(i)+getAabbMin(i));
		btVector3 diff2 = center-means;
		diff2 = diff2 * diff2;
		variance += diff2;
	}
	variance *= (btScalar(1.)/	((btScalar)numIndices-1)	);
	
	return variance.maxAxis();
}



void	btOptimizedBvh::reportAabbOverlappingNodex(btNodeOverlapCallback* nodeCallback,const btVector3& aabbMin,const btVector3& aabbMax) const
{
	//either choose recursive traversal (walkTree) or stackless (walkStacklessTree)


	if (m_useQuantization)
	{
//USE_RECURSION shows you can still do a recursive traversal on the stackless 'skip index' tree data without the explicit left/right child pointer
//#define USE_RECURSION 1
#ifdef USE_RECURSION
		bool useRecursion = true;
		if (useRecursion)
		{
			unsigned short int quantizedQueryAabbMin[3];
			unsigned short int quantizedQueryAabbMax[3];
			quantizeWithClamp(quantizedQueryAabbMin,aabbMin);
			quantizeWithClamp(quantizedQueryAabbMax,aabbMax);
			const btQuantizedBvhNode* rootNode = &m_quantizedContiguousNodes[0];
			walkRecursiveQuantizedTreeAgainstQueryAabb(rootNode,nodeCallback,quantizedQueryAabbMin,quantizedQueryAabbMax);
		} else
#endif //USE_RECURSION
		{
			walkStacklessQuantizedTree(nodeCallback,aabbMin,aabbMax);
		}
	} else
	{
		walkStacklessTree(nodeCallback,aabbMin,aabbMax);
	}
}


int maxIterations = 0;

void	btOptimizedBvh::walkStacklessTree(btNodeOverlapCallback* nodeCallback,const btVector3& aabbMin,const btVector3& aabbMax) const
{
	btAssert(!m_useQuantization);

	btOptimizedBvhNode* rootNode = &m_contiguousNodes[0];
	int escapeIndex, curIndex = 0;
	int walkIterations = 0;
	bool aabbOverlap, isLeafNode;

	while (curIndex < m_curNodeIndex)
	{
		//catch bugs in tree data
		assert (walkIterations < m_curNodeIndex);

		walkIterations++;
		aabbOverlap = TestAabbAgainstAabb2(aabbMin,aabbMax,rootNode->m_aabbMinOrg,rootNode->m_aabbMaxOrg);
		isLeafNode = rootNode->m_escapeIndex == -1;
		
		if (isLeafNode && aabbOverlap)
		{
			nodeCallback->processNode(rootNode->m_subPart,rootNode->m_triangleIndex);
		} 
		
		if (aabbOverlap || isLeafNode)
		{
			rootNode++;
			curIndex++;
		} else
		{
			escapeIndex = rootNode->m_escapeIndex;
			rootNode += escapeIndex;
			curIndex += escapeIndex;
		}
	}
	if (maxIterations < walkIterations)
		maxIterations = walkIterations;

}



void btOptimizedBvh::walkRecursiveQuantizedTreeAgainstQueryAabb(const btQuantizedBvhNode* currentNode,btNodeOverlapCallback* nodeCallback,unsigned short int* quantizedQueryAabbMin,unsigned short int* quantizedQueryAabbMax) const
{
	btAssert(m_useQuantization);
	
	int escapeIndex;
	bool aabbOverlap, isLeafNode;

	aabbOverlap = testQuantizedAabbAgainstQuantizedAabb(quantizedQueryAabbMin,quantizedQueryAabbMax,currentNode->m_quantizedAabbMin,currentNode->m_quantizedAabbMax);
	isLeafNode = currentNode->isLeafNode();
		
	if (aabbOverlap)
	{
		if (isLeafNode)
		{
			nodeCallback->processNode(0,currentNode->getTriangleIndex());
		} else
		{
			//process left and right children
			const btQuantizedBvhNode* leftChildNode = currentNode+1;
			walkRecursiveQuantizedTreeAgainstQueryAabb(leftChildNode,nodeCallback,quantizedQueryAabbMin,quantizedQueryAabbMax);

			const btQuantizedBvhNode* rightChildNode = leftChildNode->isLeafNode() ? 
								leftChildNode+1:
								leftChildNode+leftChildNode->getEscapeIndex();
			walkRecursiveQuantizedTreeAgainstQueryAabb(rightChildNode,nodeCallback,quantizedQueryAabbMin,quantizedQueryAabbMax);
		}
	}		
}


void	btOptimizedBvh::walkStacklessQuantizedTree(btNodeOverlapCallback* nodeCallback,const btVector3& aabbMin,const btVector3& aabbMax) const
{
	btAssert(m_useQuantization);

	unsigned short int quantizedQueryAabbMin[3];
	unsigned short int quantizedQueryAabbMax[3];
	quantizeWithClamp(quantizedQueryAabbMin,aabbMin);
	quantizeWithClamp(quantizedQueryAabbMax,aabbMax);
	
	const btQuantizedBvhNode* rootNode = &m_quantizedContiguousNodes[0];
	int escapeIndex, curIndex = 0;
	int walkIterations = 0;
	bool aabbOverlap, isLeafNode;

	while (curIndex < m_curNodeIndex)
	{
		//catch bugs in tree data
		assert (walkIterations < m_curNodeIndex);

		walkIterations++;
		aabbOverlap = testQuantizedAabbAgainstQuantizedAabb(quantizedQueryAabbMin,quantizedQueryAabbMax,rootNode->m_quantizedAabbMin,rootNode->m_quantizedAabbMax);
		isLeafNode = rootNode->isLeafNode();
		
		if (isLeafNode && aabbOverlap)
		{
			nodeCallback->processNode(0,rootNode->getTriangleIndex());
		} 
		
		if (aabbOverlap || isLeafNode)
		{
			rootNode++;
			curIndex++;
		} else
		{
			escapeIndex = rootNode->getEscapeIndex();
			rootNode += escapeIndex;
			curIndex += escapeIndex;
		}
	}
	if (maxIterations < walkIterations)
		maxIterations = walkIterations;

}

/*
///this was the original recursive traversal, before we optimized towards stackless traversal
void	btOptimizedBvh::walkTree(btOptimizedBvhNode* rootNode,btNodeOverlapCallback* nodeCallback,const btVector3& aabbMin,const btVector3& aabbMax) const
{
	bool isLeafNode, aabbOverlap = TestAabbAgainstAabb2(aabbMin,aabbMax,rootNode->m_aabbMin,rootNode->m_aabbMax);
	if (aabbOverlap)
	{
		isLeafNode = (!rootNode->m_leftChild && !rootNode->m_rightChild);
		if (isLeafNode)
		{
			nodeCallback->processNode(rootNode);
		} else
		{
			walkTree(rootNode->m_leftChild,nodeCallback,aabbMin,aabbMax);
			walkTree(rootNode->m_rightChild,nodeCallback,aabbMin,aabbMax);
		}
	}

}
*/



void	btOptimizedBvh::reportSphereOverlappingNodex(btNodeOverlapCallback* nodeCallback,const btVector3& aabbMin,const btVector3& aabbMax) const
{

}


void btOptimizedBvh::quantizeWithClamp(unsigned short* out, const btVector3& point) const
{

	btAssert(m_useQuantization);

	btVector3 clampedPoint(point);
	clampedPoint.setMax(m_bvhAabbMin);
	clampedPoint.setMin(m_bvhAabbMax);

	btVector3 v = (clampedPoint - m_bvhAabbMin) * m_bvhQuantization;
	out[0] = (unsigned short)(v.getX()+0.5f);
	out[1] = (unsigned short)(v.getY()+0.5f);
	out[2] = (unsigned short)(v.getZ()+0.5f);		
}

btVector3	btOptimizedBvh::unQuantize(const unsigned short* vecIn) const
{
	btVector3	vecOut;
	vecOut.setValue(
		(btScalar)(vecIn[0]) / (m_bvhQuantization.getX()),
		(btScalar)(vecIn[1]) / (m_bvhQuantization.getY()),
		(btScalar)(vecIn[2]) / (m_bvhQuantization.getZ()));
	vecOut += m_bvhAabbMin;
	return vecOut;
}


void	btOptimizedBvh::swapLeafNodes(int i,int splitIndex)
{
	if (m_useQuantization)
	{
			btQuantizedBvhNode tmp = m_quantizedLeafNodes[i];
			m_quantizedLeafNodes[i] = m_quantizedLeafNodes[splitIndex];
			m_quantizedLeafNodes[splitIndex] = tmp;
	} else
	{
			btOptimizedBvhNode tmp = m_leafNodes[i];
			m_leafNodes[i] = m_leafNodes[splitIndex];
			m_leafNodes[splitIndex] = tmp;
	}
}

void	btOptimizedBvh::assignInternalNodeFromLeafNode(int internalNode,int leafNodeIndex)
{
	if (m_useQuantization)
	{
		m_quantizedContiguousNodes[internalNode] = m_quantizedLeafNodes[leafNodeIndex];
	} else
	{
		m_contiguousNodes[internalNode] = m_leafNodes[leafNodeIndex];
	}
}
