#ifndef TRAJECTORY_PLANNER_MODULE_H
#define TRAJECTORY_PLANNER_MODULE_H

#include "planner_module.h"

#include <vigir_planning_msgs/RequestDrakeTrajectory.h>
#include <vigir_planning_msgs/ResultDrakeTrajectory.h>

#include <vector>

class RigidBodyManipulator;
class RigidBodyConstraint;
class IKoptions;

namespace vigir_drake_cpp {
  
class TrajectoryPlannerModule : public PlannerModule
{
public:
    TrajectoryPlannerModule(RigidBodyManipulator *robot_model );
    ~TrajectoryPlannerModule();

    bool plan(vigir_planning_msgs::RequestDrakeTrajectory &request_message, vigir_planning_msgs::ResultDrakeTrajectory &result_message);

protected:
    std::vector<RigidBodyConstraint*> buildIKConstraints(vigir_planning_msgs::RequestDrakeTrajectory &request_message, Eigen::VectorXd &q0);
    IKoptions *buildIKOptions();
    vigir_planning_msgs::ResultDrakeTrajectory buildTrajectoryResultMsg(Eigen::MatrixXd &q_sol, Eigen::MatrixXd &qd_sol, Eigen::MatrixXd &qdd_sol, std::vector<double> t_vec,std::vector<std::string> &joint_names, bool send_world_transform);
};

}

#endif // TRAJECTORY_PLANNER_MODULE_H