#include "btReducedDeformableContactConstraint.h"
#include <iostream>

// ================= static constraints ===================
btReducedDeformableStaticConstraint::btReducedDeformableStaticConstraint(
  btReducedSoftBody* rsb, 
  btSoftBody::Node* node,
	const btVector3& ri,
  const btContactSolverInfo& infoGlobal,
	btScalar dt)
  : m_rsb(rsb), m_ri(ri), m_dt(dt), btDeformableStaticConstraint(node, infoGlobal)
{
	// get impulse
  m_impulseFactor = rsb->getImpulseFactor(m_node->index);
}

btScalar btReducedDeformableStaticConstraint::solveConstraint(const btContactSolverInfo& infoGlobal)
{
	// target velocity of fixed constraint is 0
  btVector3 impulse = -(m_impulseFactor.inverse() * m_node->m_v);
  
  // apply full space impulse
	std::cout << "node: " << m_node->index << " impulse: " << impulse[0] << '\t' << impulse[1] << '\t' << impulse[2] << '\n';
	// std::cout << "impulse norm: " << impulse.norm() << "\n";

	m_rsb->applyFullSpaceImpulse(impulse, m_ri, m_node->index, m_dt);

	// get residual //TODO: only calculate the velocity of the given node
	m_rsb->mapToFullVelocity(m_rsb->getInterpolationWorldTransform());

	// calculate residual
	btScalar residualSquare = btDot(m_node->m_v, m_node->m_v);

	return residualSquare;
}
  
// this calls reduced deformable body's applyFullSpaceImpulse
void btReducedDeformableStaticConstraint::applyImpulse(const btVector3& impulse)
{
	m_rsb->applyFullSpaceImpulse(impulse, m_ri, m_node->index, m_dt);
}


// ================= base contact constraints ===================
btReducedDeformableRigidContactConstraint::btReducedDeformableRigidContactConstraint(
  btReducedSoftBody* rsb, 
  const btSoftBody::DeformableRigidContact& c, 
  const btContactSolverInfo& infoGlobal,
	btScalar dt)
  : m_rsb(rsb), m_dt(dt), btDeformableRigidContactConstraint(c, infoGlobal)
{
	m_appliedNormalImpulse = 0;
  m_appliedTangentImpulse = 0;
  m_impulseFactorNormal = 0;
  m_impulseFactorTangent = 0;
	m_rhs = 0;
	m_cfm = 0;
	m_erp = btScalar(0.2);
	m_friction = btScalar(0.5);

  m_contactNormalA = c.m_cti.m_normal;
  m_contactNormalB = -c.m_cti.m_normal;
  m_impulseFactor = c.m_c0;
	m_normalImpulseFactor = (m_impulseFactor * m_contactNormalA).dot(m_contactNormalA);
}

btScalar btReducedDeformableRigidContactConstraint::solveConstraint(const btContactSolverInfo& infoGlobal)
{
	btVector3 deltaVa = getVa() - m_bufferVelocityA;
	btVector3 deltaVb = getDeltaVb();
	std::cout << "deltaVa: " << deltaVa[0] << '\t' << deltaVa[1] << '\t' << deltaVa[2] << '\n';
	std::cout << "deltaVb: " << deltaVb[0] << '\t' << deltaVb[1] << '\t' << deltaVb[2] << '\n';

	// get delta relative velocity and magnitude (i.e., how much impulse has been applied?)
	btVector3 deltaV_rel = deltaVa - deltaVb;
	btScalar deltaV_rel_normal = -btDot(deltaV_rel, m_contactNormalA);
	
	// get the normal impulse to be applied
	btScalar deltaImpulse = m_rhs - deltaV_rel_normal / m_normalImpulseFactor;
	{
		// cumulative impulse that has been applied
		btScalar sum = m_appliedNormalImpulse + deltaImpulse;
		// if the cumulative impulse is pushing the object into the rigid body, set it zero
		if (sum < 0)
		{
			std::cout <<"set zeroed!!!\n";
			deltaImpulse = -m_appliedNormalImpulse;
			m_appliedNormalImpulse = 0;
		}
		else
		{
			m_appliedNormalImpulse = sum;
		}	
	}
	std::cout << "m_appliedNormalImpulse: " << m_appliedNormalImpulse << '\n';
	std::cout << "deltaImpulse: " << deltaImpulse << '\n';

	// residual is the nodal normal velocity change in current iteration
	btScalar residualSquare = deltaImpulse * m_normalImpulseFactor;	// get residual
	residualSquare *= residualSquare;

	
	// use Coulomb friction (based on delta velocity, |dv_t| = |dv_n * friction|)
	btScalar deltaImpulse_tangent = m_friction * deltaImpulse;
	if (m_appliedNormalImpulse > 0)
	{
		btScalar sum = m_appliedTangentImpulse + deltaImpulse_tangent;
		if (sum > m_appliedNormalImpulse * m_friction)
		{
			sum = m_appliedNormalImpulse * m_friction;
			m_appliedTangentImpulse = sum;
		}
		else if (sum < - m_appliedNormalImpulse * m_friction)
		{
			sum = -m_appliedNormalImpulse * m_friction;
			m_appliedTangentImpulse = sum;
		}
		deltaImpulse_tangent = sum - m_appliedTangentImpulse;	
		std::cout << "deltaImpulse_tangent: " << deltaImpulse_tangent << '\n';
	}
	

	// // friction correction
	// btScalar delta_v_rel_normal = v_rel_normal;
	// btScalar delta_v_rel_tangent = m_contact->m_c3 * v_rel_normal;
	// // btScalar delta_v_rel_tangent = 0.3 * v_rel_normal;

	// btVector3 impulse_tangent(0, 0, 0);
	// if (v_rel_tangent.norm() < delta_v_rel_tangent)
	// {
	// 	// the object should be static
	// 	impulse_tangent = m_contact->m_c0 * (-v_rel_tangent);
	// }
	// else
	// {
	// 	// apply friction
	// 	impulse_tangent = m_contact->m_c0 * (-v_rel_tangent.safeNormalize() * delta_v_rel_tangent);
	// 	std::cout << "friction called\n";
	// }

	// get the total impulse vector
	btVector3 impulse_normal = deltaImpulse * m_contactNormalA;
	btVector3 impulse_tangent = -deltaImpulse_tangent * m_contactTangent;
	btVector3 impulse = impulse_normal + impulse_tangent;

	// btVector3 impulse_dir = impulse_tangent;
	// impulse_dir.safeNormalize();
	// std::cout << "impulse direct: " << impulse_dir[0] << '\t' << impulse_dir[1] << '\t' << impulse_dir[2] << '\n';

	applyImpulse(impulse);
	// applyImpulse(impulse); // TODO: apply impulse?
	// apply impulse to the rigid/multibodies involved and change their velocities
	// if (cti.m_colObj->getInternalType() == btCollisionObject::CO_RIGID_BODY)
	// {
	// 	btRigidBody* rigidCol = 0;
	// 	rigidCol = (btRigidBody*)btRigidBody::upcast(cti.m_colObj);
	// 	if (rigidCol)
	// 	{
	// 		rigidCol->applyImpulse(impulse, m_contact->m_c1);
	// 	}
	// }
	// else if (cti.m_colObj->getInternalType() == btCollisionObject::CO_FEATHERSTONE_LINK)
	// {
	// 	btMultiBodyLinkCollider* multibodyLinkCol = 0;
	// 	multibodyLinkCol = (btMultiBodyLinkCollider*)btMultiBodyLinkCollider::upcast(cti.m_colObj);
	// 	if (multibodyLinkCol)
	// 	{
	// 		const btScalar* deltaV_normal = &m_contact->jacobianData_normal.m_deltaVelocitiesUnitImpulse[0];
	// 		// apply normal component of the impulse
	// 		multibodyLinkCol->m_multiBody->applyDeltaVeeMultiDof2(deltaV_normal, impulse.dot(cti.m_normal));
	// 		if (impulse_tangent.norm() > SIMD_EPSILON)
	// 		{
	// 			// apply tangential component of the impulse
	// 			const btScalar* deltaV_t1 = &m_contact->jacobianData_t1.m_deltaVelocitiesUnitImpulse[0];
	// 			multibodyLinkCol->m_multiBody->applyDeltaVeeMultiDof2(deltaV_t1, impulse.dot(m_contact->t1));
	// 			const btScalar* deltaV_t2 = &m_contact->jacobianData_t2.m_deltaVelocitiesUnitImpulse[0];
	// 			multibodyLinkCol->m_multiBody->applyDeltaVeeMultiDof2(deltaV_t2, impulse.dot(m_contact->t2));
	// 		}
	// 	}
	// }
	return residualSquare;
}

// ================= node vs rigid constraints ===================
btReducedDeformableNodeRigidContactConstraint::btReducedDeformableNodeRigidContactConstraint(
  btReducedSoftBody* rsb, 
  const btSoftBody::DeformableNodeRigidContact& contact, 
  const btContactSolverInfo& infoGlobal,
	btScalar dt)
  : m_node(contact.m_node), btReducedDeformableRigidContactConstraint(rsb, contact, infoGlobal, dt)
{
	m_relPosA = contact.m_c1;
	m_relPosB = m_node->m_x - m_rsb->getRigidTransform().getOrigin();
	warmStarting();
}

void btReducedDeformableNodeRigidContactConstraint::warmStarting()
{
	btVector3 va = getVa();
	btVector3 vb = getVb();
	m_bufferVelocityA = va;
	m_bufferVelocityB = vb;

	// we define the (+) direction of errors to be the outward surface normal of the rigid object
	btVector3 v_rel = va - vb;
	// get tangent direction
	m_contactTangent = -(v_rel / v_rel.safeNorm() - m_contactNormalA);

	btScalar velocity_error = -btDot(v_rel, m_contactNormalA);	// magnitude of relative velocity
	btScalar position_error = 0;
	if (m_penetration > 0)
	{
		// velocity_error += m_penetration / m_dt; // TODO: why?
	}
	else
	{
		// add penetration correction vel
		position_error = m_penetration * m_erp / m_dt;	//TODO: add erp parameter
	}
	// get the initial estimate of impulse magnitude to be applied
	// m_rhs = -velocity_error * m_normalImpulseFactorInv;
	m_rhs = -(velocity_error + position_error) / m_normalImpulseFactor;
}

btVector3 btReducedDeformableNodeRigidContactConstraint::getVb() const
{
	return m_node->m_v;
}

btVector3 btReducedDeformableNodeRigidContactConstraint::getDeltaVb() const
{
	return m_rsb->internalComputeNodeDeltaVelocity(m_rsb->getInterpolationWorldTransform(), m_node->index);
}

btVector3 btReducedDeformableNodeRigidContactConstraint::getSplitVb() const
{
	return m_node->m_splitv;
}

btVector3 btReducedDeformableNodeRigidContactConstraint::getDv(const btSoftBody::Node* node) const
{
	return m_total_normal_dv + m_total_tangent_dv;
}

void btReducedDeformableNodeRigidContactConstraint::applyImpulse(const btVector3& impulse)
{
	std::cout << "impulse applied: " << impulse[0] << '\t' << impulse[1] << '\t' << impulse[2] << '\n';
  m_rsb->internalApplyFullSpaceImpulse(impulse, m_relPosB, m_node->index, m_dt);
	// m_rsb->applyFullSpaceImpulse(impulse, m_relPosB, m_node->index, m_dt);
	// m_rsb->mapToFullVelocity(m_rsb->getInterpolationWorldTransform());
	std::cout << "node: " << m_node->index << " vel: " << m_node->m_v[0] << '\t' << m_node->m_v[1] << '\t' << m_node->m_v[2] << '\n';
	btVector3 v_after = getDeltaVb() + m_node->m_v;
	std::cout << "vel after: " << v_after[0] << '\t' << v_after[1] << '\t' << v_after[2] << '\n';
}

// ================= face vs rigid constraints ===================
btReducedDeformableFaceRigidContactConstraint::btReducedDeformableFaceRigidContactConstraint(
  btReducedSoftBody* rsb, 
  const btSoftBody::DeformableFaceRigidContact& contact, 
  const btContactSolverInfo& infoGlobal,
	btScalar dt, 
  bool useStrainLimiting)
  : m_face(contact.m_face), m_useStrainLimiting(useStrainLimiting), btReducedDeformableRigidContactConstraint(rsb, contact, infoGlobal, dt)
{}

btVector3 btReducedDeformableFaceRigidContactConstraint::getVb() const
{
	const btSoftBody::DeformableFaceRigidContact* contact = getContact();
	btVector3 vb = m_face->m_n[0]->m_v * contact->m_bary[0] + m_face->m_n[1]->m_v * contact->m_bary[1] + m_face->m_n[2]->m_v * contact->m_bary[2];
	return vb;
}

btVector3 btReducedDeformableFaceRigidContactConstraint::getSplitVb() const
{
	const btSoftBody::DeformableFaceRigidContact* contact = getContact();
	btVector3 vb = (m_face->m_n[0]->m_splitv) * contact->m_bary[0] + (m_face->m_n[1]->m_splitv) * contact->m_bary[1] + (m_face->m_n[2]->m_splitv) * contact->m_bary[2];
	return vb;
}

btVector3 btReducedDeformableFaceRigidContactConstraint::getDv(const btSoftBody::Node* node) const
{
	btVector3 face_dv = m_total_normal_dv + m_total_tangent_dv;
	const btSoftBody::DeformableFaceRigidContact* contact = getContact();
	if (m_face->m_n[0] == node)
	{
		return face_dv * contact->m_weights[0];
	}
	if (m_face->m_n[1] == node)
	{
		return face_dv * contact->m_weights[1];
	}
	btAssert(node == m_face->m_n[2]);
	return face_dv * contact->m_weights[2];
}

void btReducedDeformableFaceRigidContactConstraint::applyImpulse(const btVector3& impulse)
{
  //
}