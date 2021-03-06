#include "drake/examples/kuka_iiwa_arm/iiwa_lcm.h"

#include "drake/common/drake_assert.h"
#include "drake/lcmt_iiwa_command.hpp"
#include "drake/lcmt_iiwa_status.hpp"

namespace drake {
namespace examples {
namespace kuka_iiwa_arm {

using systems::BasicVector;
using systems::Context;
using systems::DiscreteState;
using systems::State;
using systems::SystemOutput;

static const int kNumJoints = 7;

// This value is chosen to match the value in getSendPeriodMilliSec()
// when initializing the FRI configuration on the iiwa's control
// cabinet.
const double kIiwaLcmStatusPeriod = 0.005;

IiwaCommandReceiver::IiwaCommandReceiver() {
  this->DeclareAbstractInputPort();
  this->DeclareOutputPort(systems::kVectorValued, kNumJoints * 2);
  this->DeclareDiscreteUpdatePeriodSec(kIiwaLcmStatusPeriod);
  this->DeclareDiscreteState(kNumJoints * 2);
}

void IiwaCommandReceiver::set_initial_position(
    Context<double>* context,
    const Eigen::Ref<const VectorX<double>> x) const {
  auto state_value =
      context->get_mutable_discrete_state(0)->get_mutable_value();
  DRAKE_ASSERT(x.size() == kNumJoints);
  state_value.head(kNumJoints) = x;
  state_value.tail(kNumJoints) = VectorX<double>::Zero(kNumJoints);
}

void IiwaCommandReceiver::DoCalcDiscreteVariableUpdates(
    const Context<double>& context,
    DiscreteState<double>* discrete_state) const {
  const systems::AbstractValue* input = this->EvalAbstractInput(context, 0);
  DRAKE_ASSERT(input != nullptr);
  const auto& command = input->GetValue<lcmt_iiwa_command>();
  // TODO(sam.creasey) Support torque control.
  DRAKE_ASSERT(command.num_torques == 0);


  // If we're using a default constructed message (haven't received
  // a command yet), keep using the initial state.
  if (command.num_joints != 0) {
    DRAKE_DEMAND(command.num_joints == kNumJoints);
    VectorX<double> new_positions(kNumJoints);
    for (int i = 0; i < command.num_joints; ++i) {
      new_positions(i) = command.joint_position[i];
    }

    BasicVector<double>* state = discrete_state->get_mutable_discrete_state(0);
    auto state_value = state->get_mutable_value();
    state_value.tail(kNumJoints) =
        (new_positions - state_value.head(kNumJoints)) / kIiwaLcmStatusPeriod;
    state_value.head(kNumJoints) = new_positions;
  }
}

void IiwaCommandReceiver::DoCalcOutput(const Context<double>& context,
                                       SystemOutput<double>* output) const {
  Eigen::VectorBlock<VectorX<double>> output_vec =
      this->GetMutableOutputVector(output, 0);
  output_vec = context.get_discrete_state(0)->get_value();
}

IiwaCommandSender::IiwaCommandSender()
    : position_input_port_(
          this->DeclareInputPort(
              systems::kVectorValued, kNumJoints).get_index()),
      torque_input_port_(
          this->DeclareInputPort(
              systems::kVectorValued, kNumJoints).get_index()) {
  this->DeclareAbstractOutputPort();
}

std::unique_ptr<systems::AbstractValue>
IiwaCommandSender::AllocateOutputAbstract(
    const systems::OutputPortDescriptor<double>& descriptor) const {
  lcmt_iiwa_command msg{};
  return std::make_unique<systems::Value<lcmt_iiwa_command>>(msg);
}

void IiwaCommandSender::DoCalcOutput(
    const Context<double>& context, SystemOutput<double>* output) const {
  systems::AbstractValue* mutable_data = output->GetMutableData(0);
  lcmt_iiwa_command& command =
      mutable_data->GetMutableValue<lcmt_iiwa_command>();

  command.utime = context.get_time() * 1e6;
  const systems::BasicVector<double>* positions =
      this->EvalVectorInput(context, 0);

  command.num_joints = kNumJoints;
  command.joint_position.resize(kNumJoints);
  for (int i = 0; i < kNumJoints; ++i) {
    command.joint_position[i] = positions->GetAtIndex(i);
  }

  const systems::BasicVector<double>* torques =
      this->EvalVectorInput(context, 1);
  if (torques == nullptr) {
    command.num_torques = 0;
    command.joint_torque.clear();
  } else {
    command.num_torques = kNumJoints;
    command.joint_torque.resize(kNumJoints);
    for (int i = 0; i < kNumJoints; ++i) {
      command.joint_torque[i] = torques->GetAtIndex(i);
    }
  }
}


IiwaStatusReceiver::IiwaStatusReceiver()
    : measured_position_output_port_(
          this->DeclareOutputPort(
              systems::kVectorValued, kNumJoints * 2).get_index()),
      commanded_position_output_port_(
          this->DeclareOutputPort(
              systems::kVectorValued, kNumJoints).get_index()) {
  this->DeclareAbstractInputPort();
  this->DeclareDiscreteState(kNumJoints * 3);
  this->DeclareDiscreteUpdatePeriodSec(kIiwaLcmStatusPeriod);
}

void IiwaStatusReceiver::DoCalcDiscreteVariableUpdates(
    const Context<double>& context,
    DiscreteState<double>* discrete_state) const {

  const systems::AbstractValue* input = this->EvalAbstractInput(context, 0);
  DRAKE_ASSERT(input != nullptr);
  const auto& status = input->GetValue<lcmt_iiwa_status>();

  // If we're using a default constructed message (haven't received
  // status yet), keep using the initial state.
  if (status.num_joints != 0) {
    DRAKE_DEMAND(status.num_joints == kNumJoints);

    VectorX<double> measured_position(kNumJoints);
    VectorX<double> commanded_position(kNumJoints);
    for (int i = 0; i < status.num_joints; ++i) {
      measured_position(i) = status.joint_position_measured[i];
      commanded_position(i) = status.joint_position_commanded[i];
    }

    BasicVector<double>* state = discrete_state->get_mutable_discrete_state(0);
    auto state_value = state->get_mutable_value();
    state_value.segment(kNumJoints, kNumJoints) =
        (measured_position - state_value.head(kNumJoints)) /
        kIiwaLcmStatusPeriod;
    state_value.head(kNumJoints) = measured_position;
    state_value.tail(kNumJoints) = commanded_position;
  }
}

void IiwaStatusReceiver::DoCalcOutput(const Context<double>& context,
                                       SystemOutput<double>* output) const {
  const auto state_value = context.get_discrete_state(0)->get_value();

  Eigen::VectorBlock<VectorX<double>> measured_position_output =
      this->GetMutableOutputVector(output, measured_position_output_port_);
  measured_position_output = state_value.head(kNumJoints * 2);
  Eigen::VectorBlock<VectorX<double>> commanded_position_output =
      this->GetMutableOutputVector(output, commanded_position_output_port_);
  commanded_position_output = state_value.tail(kNumJoints);
  DRAKE_ASSERT(
      measured_position_output.size() + commanded_position_output.size() ==
      state_value.size());
}

IiwaStatusSender::IiwaStatusSender() {
  this->DeclareInputPort(systems::kVectorValued, kNumJoints * 2);
  this->DeclareInputPort(systems::kVectorValued, kNumJoints * 2);
  this->DeclareAbstractOutputPort();
}

std::unique_ptr<systems::AbstractValue>
IiwaStatusSender::AllocateOutputAbstract(
    const systems::OutputPortDescriptor<double>& descriptor) const {
  lcmt_iiwa_status msg{};
  msg.num_joints = kNumJoints;
  msg.joint_position_measured.resize(msg.num_joints, 0);
  msg.joint_position_commanded.resize(msg.num_joints, 0);
  msg.joint_position_ipo.resize(msg.num_joints, 0);
  msg.joint_torque_measured.resize(msg.num_joints, 0);
  msg.joint_torque_commanded.resize(msg.num_joints, 0);
  msg.joint_torque_external.resize(msg.num_joints, 0);
  return std::make_unique<systems::Value<lcmt_iiwa_status>>(msg);
}

void IiwaStatusSender::DoCalcOutput(
    const Context<double>& context, SystemOutput<double>* output) const {
  systems::AbstractValue* mutable_data = output->GetMutableData(0);
  lcmt_iiwa_status& status =
      mutable_data->GetMutableValue<lcmt_iiwa_status>();

  status.utime = context.get_time() * 1e6;
  const systems::BasicVector<double>* command =
      this->EvalVectorInput(context, 0);
  const systems::BasicVector<double>* state =
      this->EvalVectorInput(context, 1);
  for (int i = 0; i < kNumJoints; ++i) {
    status.joint_position_measured[i] = state->GetAtIndex(i);
    status.joint_position_commanded[i] = command->GetAtIndex(i);
  }
}


}  // namespace kuka_iiwa_arm
}  // namespace examples
}  // namespace drake
