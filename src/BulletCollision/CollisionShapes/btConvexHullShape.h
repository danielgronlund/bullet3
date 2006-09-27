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

#ifndef CONVEX_HULL_SHAPE_H
#define CONVEX_HULL_SHAPE_H

#include "btPolyhedralConvexShape.h"
#include "BulletCollision/BroadphaseCollision/btBroadphaseProxy.h" // for the types

#include <vector>

///ConvexHullShape implements an implicit (getSupportingVertex) Convex Hull of a Point Cloud (vertices)
///No connectivity is needed. LocalGetSupportingVertex iterates linearly though all vertices.
///on modern hardware, due to cache coherency this isn't that bad. Complex algorithms tend to trash the cash.
///(memory is much slower then the cpu)
class btConvexHullShape : public btPolyhedralConvexShape
{
	std::vector<btPoint3>	m_points;

public:
	btConvexHullShape(btPoint3* points,int numPoints, int stride=sizeof(btPoint3));

	void AddPoint(const btPoint3& point)
	{
		m_points.push_back(point);
	}
	virtual btVector3	LocalGetSupportingVertex(const btVector3& vec)const;
	virtual btVector3	LocalGetSupportingVertexWithoutMargin(const btVector3& vec)const;
	virtual void	BatchedUnitVectorGetSupportingVertexWithoutMargin(const btVector3* vectors,btVector3* supportVerticesOut,int numVectors) const;
	

	virtual int	GetShapeType()const { return CONVEX_HULL_SHAPE_PROXYTYPE; }

	//debugging
	virtual char*	GetName()const {return "Convex";}

	
	virtual int	GetNumVertices() const;
	virtual int GetNumEdges() const;
	virtual void GetEdge(int i,btPoint3& pa,btPoint3& pb) const;
	virtual void GetVertex(int i,btPoint3& vtx) const;
	virtual int	GetNumPlanes() const;
	virtual void GetPlane(btVector3& planeNormal,btPoint3& planeSupport,int i ) const;
	virtual	bool IsInside(const btPoint3& pt,btScalar tolerance) const;



};


#endif //CONVEX_HULL_SHAPE_H

