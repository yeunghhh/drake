#pragma once

#include <memory>

#include "drake/common/drake_copyable.h"
#include "drake/multibody/rigid_body_tree.h"
#include "drake/systems/framework/leaf_system.h"

namespace drake {
namespace examples {
namespace qp_inverse_dynamics {

/**
 * An abstract base class that translates a QpOutput to a vector of torque
 * commands in the actuator order within the RigidBodyTree. Derived classes
 * need to implement DoCalcExtendedOutput() to generate additional custom
 * outputs.
 */
class JointLevelControllerBaseSystem : public systems::LeafSystem<double> {
 public:
  DRAKE_NO_COPY_NO_MOVE_NO_ASSIGN(JointLevelControllerBaseSystem)

  /**
   * @param robot A reference to the RigidBodyTree within the plant that is
   * being controlled. The lifespan of this reference must exceed that of this
   * class's instance. @p robot should only contain a single model instance.
   */
  explicit JointLevelControllerBaseSystem(const RigidBodyTree<double>& robot);

  /**
   * Returns the input port for QpOutput.
   */
  inline const systems::InputPortDescriptor<double>& get_input_port_qp_output()
      const {
    return get_input_port(input_port_index_qp_output_);
  }

  /**
   * Returns the output port for the torques.
   */
  inline const systems::OutputPortDescriptor<double>& get_output_port_torque()
      const {
    return get_output_port(output_port_index_torque_);
  }

  inline const RigidBodyTree<double>& get_robot() const { return robot_; }

 protected:
  /**
   * Derived classes can implement custom outputs in this function. It is called
   * by DoCalcOutput(), and it is safe to assume that output port returned by
   * get_output_port_torque() is already filled.
   */
  virtual void DoCalcExtendedOutput(
      const systems::Context<double>& context,
      systems::SystemOutput<double>* output) const = 0;

 private:
  // Extracts the torques from a QpOutput and output them in the actuator order
  // within the RigidBodyTree passed to the constructor. More specifically, the
  // output is `tau_act = B^T * qp_output.dof_torques`, where `B` is the
  // selection matrix that maps the actuator indices to the generalized
  // coordinate indices. Then calls DoCalcExtendedOutput(). Derived classes need
  // to implement DoCalcExtendedOutput(), and can assume that output port
  // returned by get_output_port_torque() is already filled.
  void DoCalcOutput(const systems::Context<double>& context,
                    systems::SystemOutput<double>* output) const final;

  const RigidBodyTree<double>& robot_;

  int input_port_index_qp_output_{0};
  int output_port_index_torque_{0};
};

/**
 * Translates a QpOutput to a vector of torque commands in the actuator order
 * within the RigidBodyTree.
 */
class TrivialJointLevelControllerSystem
    : public JointLevelControllerBaseSystem {
 public:
  DRAKE_NO_COPY_NO_MOVE_NO_ASSIGN(TrivialJointLevelControllerSystem);

  explicit TrivialJointLevelControllerSystem(const RigidBodyTree<double>& robot)
      : JointLevelControllerBaseSystem(robot) {}

 private:
  void DoCalcExtendedOutput(const systems::Context<double>& context,
                            systems::SystemOutput<double>* output) const final {
  }
};

}  // namespace qp_inverse_dynamics
}  // namespace examples
}  // namespace drake
