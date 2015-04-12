#include "vigir_drake_cpp/planner_modules/position_ik_planner_module.h"

#include "RigidBodyManipulator.h"
#include "RigidBodyConstraint.h"
#include "RigidBodyIK.h"
#include "IKoptions.h"

#include <tf/tf.h>
#include <ros/ros.h>

using namespace Eigen;

namespace vigir_drake_cpp {
  
PositionIKPlannerModule::PositionIKPlannerModule(RigidBodyManipulator *robot_model ) : PlannerModule(robot_model)
{
}

PositionIKPlannerModule::~PositionIKPlannerModule() {

}

bool PositionIKPlannerModule::plan(vigir_planning_msgs::RequestDrakeIK &request_message, vigir_planning_msgs::ResultDrakeIK &result_message)
{
    // convert robot position to drake format
    VectorXd q0 = VectorXd::Zero(this->getRobotModel()->num_positions);

    bool received_world_transform = false;
    q0 = messageQs2DrakeQs(q0, request_message.robot_state, received_world_transform);

    // build IK options and constraints
    IKoptions *ik_options = buildIKOptions(request_message);
    std::vector<RigidBodyConstraint*> ik_constraints = buildIKConstraints(request_message, q0);

    VectorXd q_sol = VectorXd::Zero(this->getRobotModel()->num_positions);
    int info;
    std::vector<std::string> infeasible_constraints;
    inverseKin(this->getRobotModel(),q0,q0,ik_constraints.size(),ik_constraints.data(),q_sol,info,infeasible_constraints,*ik_options);

    bool success = true;
    if(info>10) { // something went wrong
        std::string constraint_string = "";
        for (auto const& constraint : infeasible_constraints) { constraint_string += (constraint+" | "); }

        ROS_WARN("SNOPT info is %d, IK mex fails to solve the problem", info);
        ROS_INFO("Infeasible constraints: %s", constraint_string.c_str());

        success = false;
    }

    if ( success ) {
        // build result message form q values
        result_message.result_state = request_message.robot_state;

        // map message robotModel qs to message order
        MatrixXd result_q_mat = q_sol;
        MatrixXd result_qs = drakeQs2MessageQs(result_q_mat, request_message.robot_state);
        std::vector<double> result_q_vec(result_qs.data(), result_qs.data() + result_qs.rows() * result_qs.cols());
        result_message.result_state.joint_state.position = result_q_vec;

        // extract world position and orientation
        if ( received_world_transform ) {
            geometry_msgs::Transform world_transform;
            world_transform.rotation = tf::createQuaternionMsgFromRollPitchYaw(q_sol(3), q_sol(4), q_sol(5));
            world_transform.translation.x = q_sol(0);
            world_transform.translation.y = q_sol(1);
            world_transform.translation.z = q_sol(2);

            result_message.result_state.multi_dof_joint_state.joint_names.push_back("world_virtual_joint");
            result_message.result_state.multi_dof_joint_state.transforms.push_back(world_transform);
        }

        result_message.is_valid = true;
    }
    else {
        // return starting pose in an invalid message
        result_message.result_state = request_message.robot_state;
        result_message.is_valid = false;
    }

    delete ik_options;
    return success;
}

std::vector<RigidBodyConstraint*> PositionIKPlannerModule::buildIKConstraints(vigir_planning_msgs::RequestDrakeIK &request_message, VectorXd &q0) {
    std::vector<RigidBodyConstraint*> constraints;

    // fixed foot placement
    int l_foot_id = this->getRobotModel()->findLinkId("l_foot");
    int r_foot_id = this->getRobotModel()->findLinkId("r_foot");

    Vector4d foot_pts;
    foot_pts << 0.0,0.0,0.0,1.0;

    //Map<VectorXd> q_map(&q0, q0.rows(), 1);
    //Map<VectorXd> v_map(nullptr, 0, 1);
    //this->getRobotModel()->doKinematicsNew(q0, v_map, false, false);
    this->getRobotModel()->doKinematics(q0);

    Vector7d l_foot_pos, r_foot_pos;
    this->getRobotModel()->forwardKin(l_foot_id, foot_pts, 2, l_foot_pos);
    this->getRobotModel()->forwardKin(r_foot_id, foot_pts, 2, r_foot_pos);

    constraints.push_back( new WorldPositionConstraint(this->getRobotModel(), l_foot_id, foot_pts, l_foot_pos.block<3,1>(0,0), l_foot_pos.block<3,1>(0,0)) );
    constraints.push_back( new WorldPositionConstraint(this->getRobotModel(), r_foot_id, foot_pts, r_foot_pos.block<3,1>(0,0), r_foot_pos.block<3,1>(0,0)) );
    constraints.push_back( new WorldQuatConstraint(this->getRobotModel(), l_foot_id, l_foot_pos.block<4,1>(3,0), 0.0));
    constraints.push_back( new WorldQuatConstraint(this->getRobotModel(), r_foot_id, l_foot_pos.block<4,1>(3,0), 0.0));

    // add quasi static constraint
    MatrixXd l_foot_contact_pts = getRobotModel()->bodies[l_foot_id]->contact_pts;
    MatrixXd r_foot_contact_pts = getRobotModel()->bodies[r_foot_id]->contact_pts;

    QuasiStaticConstraint *quasi_static_constraint = new QuasiStaticConstraint(this->getRobotModel());
    quasi_static_constraint->addContact(1, &l_foot_id, &l_foot_contact_pts);
    quasi_static_constraint->addContact(1, &r_foot_id, &r_foot_contact_pts);
    quasi_static_constraint->setActive(true);
    quasi_static_constraint->setShrinkFactor(0.9);
    constraints.push_back( quasi_static_constraint );

    // handle end effector positions and orientations
    for (int i = 0; i < request_message.target_poses.size(); i++ ) {
        // get endeffector body ids and points
        int eef_body_id = this->getRobotModel()->findLinkId(request_message.target_link_names[i]);
        Vector4d eef_pts;
        eef_pts << 0.0,0.0,0.0,1.0;

        // goal position constraint
        geometry_msgs::Point goal_position = request_message.target_poses[i].pose.position;

        Vector3d goal_position_vec;
        goal_position_vec << goal_position.x,goal_position.y,goal_position.z;

        constraints.push_back( new WorldPositionConstraint(this->getRobotModel(), eef_body_id, eef_pts, goal_position_vec, goal_position_vec) );

        // goal orientation constraint
        geometry_msgs::Quaternion goal_orientation = request_message.target_poses[i].pose.orientation;
        Vector4d goal_orientation_quat;
        goal_orientation_quat << goal_orientation.w,goal_orientation.x,goal_orientation.y,goal_orientation.z;
        if ( goal_orientation_quat == Vector4d::Zero()) {
            goal_orientation_quat << 1.0, 0.0, 0.0, 0.0;
        }

        constraints.push_back( new WorldQuatConstraint(this->getRobotModel(), eef_body_id, goal_orientation_quat, 0) );
    }

    return constraints;
}


IKoptions *PositionIKPlannerModule::buildIKOptions(vigir_planning_msgs::RequestDrakeIK &request_message) {
    RigidBodyManipulator *current_robot = this->getRobotModel();
    IKoptions *ik_options = new IKoptions( current_robot );

    Eigen::MatrixXd Q = MatrixXd::Identity(current_robot->num_positions, current_robot->num_positions);
    ik_options->setQ(Q);
    ik_options->setQa(0.001*Q);
    ik_options->setMajorIterationsLimit(10000);
    ik_options->setIterationsLimit(500000);
    ik_options->setSuperbasicsLimit(1000);
    ik_options->setMajorOptimalityTolerance(2e-4);
    ik_options->setDebug(true);

    return ik_options;
}

}