#ifndef B3_CONFIG_H
#define B3_CONFIG_H

struct	b3Config
{
	int	m_maxConvexBodies;
	int	m_maxConvexShapes;
	int	m_maxBroadphasePairs;
	int m_maxContactCapacity;

	int m_maxVerticesPerFace;
	int m_maxFacesPerShape;
	int	m_maxConvexVertices;
	int m_maxConvexIndices;
	int m_maxConvexUniqueEdges;
	
	int	m_maxCompoundChildShapes;
	
	int m_maxTriConvexPairCapacity;

	b3Config()
#ifdef __APPLE__
		:m_maxConvexBodies(32*1024),
#else
		:m_maxConvexBodies(32*1024),
#endif
		m_maxConvexShapes(81920),
		m_maxVerticesPerFace(64),
		m_maxFacesPerShape(64),
		m_maxConvexVertices(8192000),
		m_maxConvexIndices(8192000),
		m_maxConvexUniqueEdges(819200),
		m_maxCompoundChildShapes(81920),
		m_maxTriConvexPairCapacity(512*1024)
		//m_maxTriConvexPairCapacity(256*1024)
	{
		m_maxBroadphasePairs = 16*m_maxConvexBodies;
		m_maxContactCapacity = m_maxBroadphasePairs;
	}
};


#endif//B3_CONFIG_H

