/*
 Written by Xuchen Han <xuchenhan2015@u.northwestern.edu>
 
 Bullet Continuous Collision Detection and Physics Library
 Copyright (c) 2019 Google Inc. http://bulletphysics.org
 This software is provided 'as-is', without any express or implied warranty.
 In no event will the authors be held liable for any damages arising from the use of this software.
 Permission is granted to anyone to use this software for any purpose,
 including commercial applications, and to alter it and redistribute it freely,
 subject to the following restrictions:
 1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
 2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
 3. This notice may not be removed or altered from any source distribution.
 */

#include "btDeformableContactProjection.h"
#include "btDeformableMultiBodyDynamicsWorld.h"
#include <algorithm>
#include <cmath>
btScalar btDeformableContactProjection::update()
{
    btScalar residualSquare = 0;

    // node constraints
    for (int index = 0; index < m_nodeRigidConstraints.size(); ++index)
    {
        btAlignedObjectArray<btDeformableNodeRigidContactConstraint>& constraints = *m_nodeRigidConstraints.getAtIndex(index);
        for (int i = 0; i < constraints.size(); ++i)
        {
            btScalar localResidualSquare = constraints[i].solveConstraint();
            residualSquare = btMax(residualSquare, localResidualSquare);
        }
    }
    
    // face constraints
    for (int index = 0; index < m_allFaceConstraints.size(); ++index)
    {
        btDeformableFaceRigidContactConstraint* constraint = m_allFaceConstraints[index];
        btScalar localResidualSquare = constraint->solveConstraint();
        residualSquare = btMax(residualSquare, localResidualSquare);
    }
    
    // todo xuchenhan@: deformable/deformable constraints
    return residualSquare;
}


void btDeformableContactProjection::setConstraints()
{
    BT_PROFILE("setConstraints");
    // set Dirichlet constraint
    for (int i = 0; i < m_softBodies.size(); ++i)
    {
        btSoftBody* psb = m_softBodies[i];
        for (int j = 0; j < psb->m_nodes.size(); ++j)
        {
            if (psb->m_nodes[j].m_im == 0)
            {
                btDeformableStaticConstraint static_constraint(&psb->m_nodes[j]);
                m_staticConstraints.insert(psb->m_nodes[j].index, static_constraint);
            }
        }
    }
    
    // set Deformable Node vs. Rigid constraint
    for (int i = 0; i < m_softBodies.size(); ++i)
    {
        btSoftBody* psb = m_softBodies[i];
        for (int j = 0; j < psb->m_nodeRigidContacts.size(); ++j)
        {
            const btSoftBody::DeformableNodeRigidContact& contact = psb->m_nodeRigidContacts[j];
            // skip fixed points
            if (contact.m_node->m_im == 0)
            {
                continue;
            }
            btDeformableNodeRigidContactConstraint constraint(contact);
            btVector3 va = constraint.getVa();
            btVector3 vb = constraint.getVb();
            const btVector3 vr = vb - va;
            const btSoftBody::sCti& cti = contact.m_cti;
            const btScalar dn = btDot(vr, cti.m_normal);
            if (dn < SIMD_EPSILON)
            {
                if (m_nodeRigidConstraints.find(contact.m_node->index) == NULL)
                {
                    btAlignedObjectArray<btDeformableNodeRigidContactConstraint> constraintsList;
                    constraintsList.push_back(constraint);
                    m_nodeRigidConstraints.insert(contact.m_node->index, constraintsList);
                }
                else
                {
                    btAlignedObjectArray<btDeformableNodeRigidContactConstraint>& constraintsList = *m_nodeRigidConstraints[contact.m_node->index];
                    constraintsList.push_back(constraint);
                }
            }
        }
        
         // set Deformable Face vs. Rigid constraint
        for (int j = 0; j < psb->m_faceRigidContacts.size(); ++j)
        {
            const btSoftBody::DeformableFaceRigidContact& contact = psb->m_faceRigidContacts[j];
            // skip fixed faces
            if (contact.m_c2 == 0)
            {
                continue;
            }
            btDeformableFaceRigidContactConstraint* constraint = new btDeformableFaceRigidContactConstraint(contact);
            btVector3 va = constraint->getVa();
            btVector3 vb = constraint->getVb();
            const btVector3 vr = vb - va;
            const btSoftBody::sCti& cti = contact.m_cti;
            const btScalar dn = btDot(vr, cti.m_normal);
            if (dn < SIMD_EPSILON)
            {
                m_allFaceConstraints.push_back(constraint);
                // add face constraints to each of the nodes
                for (int k = 0; k < 3; ++k)
                {
                    btSoftBody::Node* node = contact.m_face->m_n[k];
                    // static node does not need to own face/rigid constraint
                    if (node->m_im != 0)
                    {
                        if (m_faceRigidConstraints.find(node->index) == NULL)
                        {
                            btAlignedObjectArray<btDeformableFaceRigidContactConstraint*> constraintsList;
                            constraintsList.push_back(constraint);
                            m_faceRigidConstraints.insert(node->index, constraintsList);
                        }
                        else
                        {
                            btAlignedObjectArray<btDeformableFaceRigidContactConstraint*>& constraintsList = *m_faceRigidConstraints[node->index];
                            constraintsList.push_back(constraint);
                        }
                    }
                }
            }
            else
            {
                delete constraint;
            }
        }
    }
    // todo xuchenhan@: set Deformable Face vs. Deformable Node
}

void btDeformableContactProjection::enforceConstraint(TVStack& x)
{
    // x is set to zero when passed in
    
    // loop through node constraints to add in contributions to dv
    for (int index = 0; index < m_nodeRigidConstraints.size(); ++index)
    {
        btAlignedObjectArray<btDeformableNodeRigidContactConstraint>& constraintsList = *m_nodeRigidConstraints.getAtIndex(index);
        // i is node index
        int i = m_nodeRigidConstraints.getKeyAtIndex(index).getUid1();
        int numConstraints = 1;
//        int numConstraints = constraintsList.size();
//        if (m_faceRigidConstraints.find(i) != NULL)
//        {
//            numConstraints += m_faceRigidConstraints[i]->size();
//        }
        for (int j = 0; j < constraintsList.size(); ++j)
        {
            const btDeformableNodeRigidContactConstraint& constraint = constraintsList[j];
            x[i] += constraint.getDv(constraint.getContact()->m_node)/btScalar(numConstraints);
        }
    }
    
    // loop through face constraints to add in contributions to dv
    // note that for each face constraint is owned by three nodes. Be careful here to only add the dv to the node that owns the constraint
    for (int index = 0; index < m_faceRigidConstraints.size(); ++index)
    {
        btAlignedObjectArray<btDeformableFaceRigidContactConstraint*>& constraintsList = *m_faceRigidConstraints.getAtIndex(index);
        // i is node index
        int i = m_faceRigidConstraints.getKeyAtIndex(index).getUid1();
        int numConstraints = 1;
//        int numConstraints = constraintsList.size();
//        if (m_nodeRigidConstraints.find(i) != NULL)
//        {
//            numConstraints += m_nodeRigidConstraints[i]->size();
//        }
        for (int j = 0; j < constraintsList.size(); ++j)
        {
            const btDeformableFaceRigidContactConstraint* constraint = constraintsList[j];
            const btSoftBody::Face* face = constraint->m_face;
            btSoftBody::Node* node;
            // find the node that owns the constraint
            for (int k = 0; k < 3; ++k)
            {
                if (face->m_n[k]->index == i)
                {
                    node = face->m_n[k];
                    break;
                }
            }
            x[i] += constraint->getDv(node) / btScalar(numConstraints);
        }
    }
    // todo xuchenhan@: add deformable deformable constraints' contribution to dv
    
    // Finally, loop through static constraints to set dv of static nodes to zero
    for (int index = 0; index < m_staticConstraints.size(); ++index)
    {
        const btDeformableStaticConstraint& constraint = *m_staticConstraints.getAtIndex(index);
        int i = m_staticConstraints.getKeyAtIndex(index).getUid1();
        x[i] = constraint.getDv(constraint.m_node);
    }
//
//    for (int i = 0; i < x.size(); ++i)
//    {
//        x[i].setZero();
//        if (m_staticConstraints.find(i) != NULL)
//        {
//            // if a node is fixed, dv = 0
//            continue;
//        }
//        if (m_nodeRigidConstraints.find(i) != NULL)
//        {
//            btAlignedObjectArray<btDeformableNodeRigidContactConstraint>& constraintsList = *m_nodeRigidConstraints[i];
//            for (int j = 0; j < constraintsList.size(); ++j)
//            {
//                const btDeformableNodeRigidContactConstraint& constraint = constraintsList[j];
////                x[i] += constraint.getDv(m_nodes->at(i));
//                x[i] += constraint.getDv(constraint.getContact()->m_node);
//            }
//        }
        // todo xuchenhan@
//        if (m_faceRigidConstraints.find(i) != NULL)
//        {
//
//        }
//    }
}

void btDeformableContactProjection::project(TVStack& x)
{
    const int dim = 3;
    for (int index = 0; index < m_projectionsDict.size(); ++index)
    {
        btAlignedObjectArray<btVector3>& projectionDirs = *m_projectionsDict.getAtIndex(index);
        size_t i = m_projectionsDict.getKeyAtIndex(index).getUid1();
        if (projectionDirs.size() >= dim)
        {
            // static node
            x[i].setZero();
            continue;
        }
        else if (projectionDirs.size() == 2)
        {
            btVector3 dir0 = projectionDirs[0];
            btVector3 dir1 = projectionDirs[1];
            btVector3 free_dir = btCross(dir0, dir1);
            if (free_dir.norm() < SIMD_EPSILON)
            {
                x[i] -= x[i].dot(dir0) * dir0;
                x[i] -= x[i].dot(dir1) * dir1;
            }
            else
            {
                free_dir.normalize();
                x[i] = x[i].dot(free_dir) * free_dir;
            }
        }
        else
        {
            btAssert(projectionDirs.size() == 1);
            btVector3 dir0 = projectionDirs[0];
            x[i] -= x[i].dot(dir0) * dir0;
        }
    }
}

void btDeformableContactProjection::setProjection()
{
    for (int i = 0; i < m_softBodies.size(); ++i)
    {
        btSoftBody* psb = m_softBodies[i];
        for (int j = 0; j < psb->m_nodes.size(); ++j)
        {
            int index = psb->m_nodes[j].index;
            bool hasConstraint = false;
            bool existStaticConstraint = false;
            btVector3 averagedNormal(0,0,0);
            btAlignedObjectArray<btVector3> normals;
            if (m_staticConstraints.find(index) != NULL)
            {
                existStaticConstraint = true;
                hasConstraint = true;
            }
            
            // accumulate normals
            if (!existStaticConstraint && m_nodeRigidConstraints.find(index) != NULL)
            {
                hasConstraint = true;
                btAlignedObjectArray<btDeformableNodeRigidContactConstraint>& constraintsList = *m_nodeRigidConstraints[index];
                for (int k = 0; k < constraintsList.size(); ++k)
                {
                    if (constraintsList[k].m_static)
                    {
                        existStaticConstraint = true;
                        break;
                    }
                    const btVector3& local_normal = constraintsList[k].m_normal;
                    normals.push_back(local_normal);
                    averagedNormal += local_normal;
                }
            }
        
            if (!existStaticConstraint && m_faceRigidConstraints.find(index) != NULL)
            {
                hasConstraint = true;
                btAlignedObjectArray<btDeformableFaceRigidContactConstraint*>& constraintsList = *m_faceRigidConstraints[index];
                for (int k = 0; k < constraintsList.size(); ++k)
                {
                    if (constraintsList[k]->m_static)
                    {
                        existStaticConstraint = true;
                        break;
                    }
                    const btVector3& local_normal = constraintsList[k]->m_normal;
                    normals.push_back(local_normal);
                    averagedNormal += local_normal;
                }
            }
            
            
            // build projections
            if (!hasConstraint)
            {
                continue;
            }
            btAlignedObjectArray<btVector3> projections;
            if (existStaticConstraint)
            {
                projections.push_back(btVector3(1,0,0));
                projections.push_back(btVector3(0,1,0));
                projections.push_back(btVector3(0,0,1));
            }
            else
            {
                bool averageExists = (averagedNormal.length2() > SIMD_EPSILON);
                averagedNormal = averageExists ? averagedNormal.normalized() : btVector3(0,0,0);
                if (averageExists)
                {
                    projections.push_back(averagedNormal);
                }
                for (int k = 0; k < normals.size(); ++k)
                {
                    const btVector3& local_normal = normals[k];
                    // add another projection direction if it deviates from the average by more than about 15 degrees
                    if (!averageExists || btAngle(averagedNormal, local_normal) > 0.25)
                    {
                        projections.push_back(local_normal);
                    }
                }
            }
            m_projectionsDict.insert(index, projections);
        }
    }
}


void btDeformableContactProjection::applyDynamicFriction(TVStack& f)
{
    // loop over constraints
    for (int i = 0; i < f.size(); ++i)
    {
        if (m_projectionsDict.find(i) != NULL)
        {
            // doesn't need to add friction force for fully constrained vertices
            btAlignedObjectArray<btVector3>& projectionDirs = *m_projectionsDict[i];
            if (projectionDirs.size() >= 3)
            {
                continue;
            }
        }
        
        if (m_nodeRigidConstraints.find(i) != NULL)
        {
            btAlignedObjectArray<btDeformableNodeRigidContactConstraint>& constraintsList = *m_nodeRigidConstraints[i];
            for (int j = 0; j < constraintsList.size(); ++j)
            {
                const btDeformableNodeRigidContactConstraint& constraint = constraintsList[j];
                btSoftBody::Node* node = constraint.getContact()->m_node;
                
                // it's ok to add the friction force generated by the entire impulse here because the normal component of the residual will be projected out anyway.
                f[i] += constraint.getDv(node)* (1./node->m_im);
            }
        }
        // todo xuchenhan@
        if (m_faceRigidConstraints.find(i) != NULL)
        {
            btAlignedObjectArray<btDeformableFaceRigidContactConstraint*>& constraintsList = *m_faceRigidConstraints[i];
            for (int j = 0; j < constraintsList.size(); ++j)
            {
                const btDeformableFaceRigidContactConstraint* constraint = constraintsList[j];
                btSoftBody::Face* face = constraint->getContact()->m_face;
                
                // it's ok to add the friction force generated by the entire impulse here because the normal component of the residual will be projected out anyway.
                // todo xuchenhan@: figure out the index for m_faceRigidConstraints
//                f[i] += constraint.getDv(face->m_n[0])* (1./face->m_n[0]->m_im);
//                f[i] += constraint.getDv(face->m_n[1])* (1./face->m_n[1]->m_im);
//                f[i] += constraint.getDv(face->m_n[2])* (1./face->m_n[2]->m_im);
            }
        }
    }
//    for (int index = 0; index < m_constraints.size(); ++index)
//    {
//        const DeformableContactConstraint& constraint = *m_constraints.getAtIndex(index);
//        const btSoftBody::Node* node = constraint.m_node;
//        if (node == NULL)
//            continue;
//        size_t i = m_constraints.getKeyAtIndex(index).getUid1();
//        bool has_static_constraint = false;
//        
//        // apply dynamic friction force (scaled by dt) if the node does not have static friction constraint
//        for (int j = 0; j < constraint.m_static.size(); ++j)
//        {
//            if (constraint.m_static[j])
//            {
//                has_static_constraint = true;
//                break;
//            }
//        }
//        for (int j = 0; j < constraint.m_total_tangent_dv.size(); ++j)
//        {
//            btVector3 friction_force =  constraint.m_total_tangent_dv[j] * (1./node->m_im);
//            if (!has_static_constraint)
//            {
//                f[i] += friction_force;
//            }
//        }
//    }
}

void btDeformableContactProjection::reinitialize(bool nodeUpdated)
{
    btCGProjection::reinitialize(nodeUpdated);
    m_staticConstraints.clear();
    m_nodeRigidConstraints.clear();
    m_faceRigidConstraints.clear();
    m_projectionsDict.clear();
    for (int i = 0; i < m_allFaceConstraints.size(); ++i)
    {
        delete m_allFaceConstraints[i];
    }
    m_allFaceConstraints.clear();
}



