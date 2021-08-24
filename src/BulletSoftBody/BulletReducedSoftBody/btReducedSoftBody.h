#ifndef BT_REDUCED_SOFT_BODY_H
#define BT_REDUCED_SOFT_BODY_H

#include "../btSoftBody.h"
#include "LinearMath/btAlignedObjectArray.h"
#include "LinearMath/btVector3.h"
#include "LinearMath/btMatrix3x3.h"
#include "LinearMath/btTransform.h"

// Reduced deformable body is a simplified deformable object embedded in a rigid frame.
class btReducedSoftBody : public btSoftBody
{
 public:
  //
  //  Typedefs
  //
  typedef btAlignedObjectArray<btVector3> TVStack;
  // typedef btAlignedObjectArray<btMatrix3x3> tBlockDiagMatrix;
  typedef btAlignedObjectArray<btScalar> tDenseArray;
  typedef btAlignedObjectArray<btAlignedObjectArray<btScalar> > tDenseMatrix;

 private:
  // scaling factors
  btScalar m_rhoScale;         // mass density scale
  btScalar m_ksScale;          // stiffness scale

  // projection matrix
  tDenseMatrix m_projPA;        // Eqn. 4.11 from Rahul Sheth's thesis
  tDenseMatrix m_projCq;
  tDenseArray m_STP;
  tDenseArray m_MrInvSTP;

  TVStack m_localMomentArm; // Sq + x0

  btVector3 m_internalDeltaLinearVelocity;
  btVector3 m_internalDeltaAngularVelocity;
  tDenseArray m_internalDeltaReducedVelocity;

 protected:
  // rigid frame
  btScalar m_mass;          // total mass of the rigid frame
  btScalar m_inverseMass;   // inverse of the total mass of the rigid frame
  btVector3 m_linearVelocity;
  btVector3 m_angularVelocity;
  btScalar m_linearDamping;    // linear damping coefficient
  btScalar m_angularDamping;    // angular damping coefficient
  btVector3 m_linearFactor;
  btVector3 m_angularFactor;
  btVector3 m_invInertiaLocal;
  btTransform m_rigidTransformWorld;
  btMatrix3x3 m_invInertiaTensorWorld;
  btMatrix3x3 m_interpolateInvInertiaTensorWorld;
  btVector3 m_initialOrigin;  // initial center of mass (original of the m_rigidTransformWorld)

  // damping
  btScalar m_dampingAlpha;
  btScalar m_dampingBeta;

 public:

  bool m_rigidOnly;

  //
  //  Fields
  //

  // reduced space
  int m_startMode;
  int m_nReduced;
  int m_nFull;
  tDenseMatrix m_modes;														// modes of the reduced deformable model. Each inner array is a mode, outer array size = n_modes
  tDenseArray m_reducedDofs;				   // Reduced degree of freedom
  tDenseArray m_reducedDofsBuffer;     // Reduced degree of freedom at t^n
  tDenseArray m_reducedVelocity;		   // Reduced velocity array
  tDenseArray m_reducedVelocityBuffer; // Reduced velocity array at t^n
  tDenseArray m_reducedForceExternal;          // reduced external force
  tDenseArray m_reducedForceElastic;           // reduced internal elastic force
  tDenseArray m_reducedForceDamping;           // reduced internal damping force
  tDenseArray m_eigenvalues;		// eigenvalues of the reduce deformable model
  tDenseArray m_Kr;	// reduced stiffness matrix
  tDenseArray m_Mr;	// reduced mass matrix //TODO: do we need this?
  
  // full space
  TVStack m_x0;					     				 // Rest position
  tDenseArray m_nodalMass;           // Mass on each node
  btAlignedObjectArray<int> m_fixedNodes; // index of the fixed nodes

  // contacts
  btAlignedObjectArray<int> m_contactNodesList;

  //
  // Api
  //
  btReducedSoftBody(btSoftBodyWorldInfo* worldInfo, int node_count, const btVector3* x, const btScalar* m);

  ~btReducedSoftBody() {}

  //
  // initializing helpers
  //
  void internalInitialization();

  void setReducedModes(int start_mode, int num_modes, int full_size);

  void setMassProps(const tDenseArray& mass_array);

  void setInertiaProps(const btVector3& inertia);

  void setRigidVelocity(const btVector3& v);

  void setRigidAngularVelocity(const btVector3& omega);

  void setStiffnessScale(const btScalar ks);

  void setMassScale(const btScalar rho);

  void setFixedNodes(const int n_node);

  void setDamping(const btScalar alpha, const btScalar beta);

  //
  // various internal updates
  //
  virtual void translate(const btVector3& trs);

  void updateRestNodalPositions();

  void updateInertiaTensor();

  void updateLocalMomentArm();

  void predictIntegratedTransform(btScalar dt, btTransform& predictedTransform);

  // update the external force projection matrix 
  void updateExternalForceProjectMatrix(bool initialized);

  void endOfTimeStepZeroing();

  void applyInternalVelocityChanges();

  //
  // position and velocity update related
  //

  // compute reduced degree of freedoms
  void updateReducedDofs(btScalar solverdt);

  // compute reduced velocity update
  void updateReducedVelocity(btScalar solverdt, bool explicit_force = false);

  // map to full degree of freedoms
  void mapToFullPosition(const btTransform& ref_trans);

  // compute full space velocity from the reduced velocity
  void mapToFullVelocity(const btTransform& ref_trans);

  // get a single node's full space velocity from the reduced velocity
  const btVector3 computeNodeFullVelocity(const btTransform& ref_trans, int n_node) const;

  // get a single node's all delta velocity
  const btVector3 internalComputeNodeDeltaVelocity(const btTransform& ref_trans, int n_node) const;

  //
  // rigid motion related
  //
  void applyDamping(btScalar timeStep);

  void applyCentralImpulse(const btVector3& impulse);

  void applyTorqueImpulse(const btVector3& torque);

  void proceedToTransform(btScalar dt, bool end_of_time_step);

  //
  // force related
  //

  // apply impulse to the rigid frame
  void internalApplyRigidImpulse(const btVector3& impulse, const btVector3& rel_pos);
	void applyRigidImpulse(const btVector3& impulse, const btVector3& rel_pos);

  // apply impulse to nodes in the full space
  void internalApplyFullSpaceImpulse(const btVector3& impulse, const btVector3& rel_pos, int n_node, btScalar dt);
  void applyFullSpaceImpulse(const btVector3& impulse, const btVector3& rel_pos, int n_node, btScalar dt);

  // apply nodal external force in the full space
  void applyFullSpaceNodalForce(const btVector3& f_ext, int n_node);

  // apply fixed contraints to the nodes
  void applyFixedContraints(btScalar dt);

  // apply gravity to the rigid frame
  void applyRigidGravity(const btVector3& gravity, btScalar dt);

  // apply reduced elastic force
  void applyReducedElasticForce(const tDenseArray& reduce_dofs);

  // apply reduced damping force
  void applyReducedDampingForce(const tDenseArray& reduce_vel);

  // calculate the impulse factor
  virtual btMatrix3x3 getImpulseFactor(int n_node);

  // get relative position from a node to the CoM of the rigid frame
  btVector3 getRelativePos(int n_node);

  // apply velocity constraint
  void applyVelocityConstraint(const btVector3& target_vel, int n_node, btScalar dt);

  // apply position constraint
  void applyPositionConstraint(const btVector3& target_pos, int n_node, btScalar dt);

  //
  // accessors
  //
  btScalar getTotalMass() const;
  btTransform& getRigidTransform();
  const btVector3& getLinearVelocity() const;
	const btVector3& getAngularVelocity() const;

  #if defined(BT_CLAMP_VELOCITY_TO) && BT_CLAMP_VELOCITY_TO > 0
  void clampVelocity(btVector3& v) const {
      v.setX(
          fmax(-BT_CLAMP_VELOCITY_TO,
                fmin(BT_CLAMP_VELOCITY_TO, v.getX()))
      );
      v.setY(
          fmax(-BT_CLAMP_VELOCITY_TO,
                fmin(BT_CLAMP_VELOCITY_TO, v.getY()))
      );
      v.setZ(
          fmax(-BT_CLAMP_VELOCITY_TO,
                fmin(BT_CLAMP_VELOCITY_TO, v.getZ()))
      );
  }
  #endif
};

#endif // BT_REDUCED_SOFT_BODY_H