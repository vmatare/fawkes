/***************************************************************************
 *  gazsim_robotino_thread.cpp - Thread simulate the Robotino in Gazebo by sending needed informations to the Robotino-plugin in Gazebo and recieving sensordata from Gazebo
 *
 *  Created: Fr 3. Mai 21:27:06 CEST 2013
 *  Copyright  2013  Frederik Zwilling
 ****************************************************************************/

/*  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  Read the full text in the LICENSE.GPL file in the doc directory.
 */

#include "gazsim_robotino_thread.h"

#include <tf/types.h>
#include <stdio.h>
#include <list>
#include <core/threading/mutex_locker.h>

#include <gazebo/transport/Node.hh>
#include <gazebo/msgs/msgs.hh>
#include <gazebo/transport/transport.hh>
#include <aspect/logging.h>

#include <tf/transform_publisher.h>
#include <utils/time/clock.h>

using namespace fawkes;
using namespace gazebo;

/** @class RobotinoSimThread "gazsim_robotino_thread.h"
 * Thread simulate the Robotino in Gazebo 
 * by sending needed informations to the Robotino-plugin in Gazebo
 * and recieving sensordata from Gazebo
 * @author Frederik Zwilling
 */

/** Constructor. */
RobotinoSimThread::RobotinoSimThread()
  : Thread("RobotinoSimThread", Thread::OPMODE_WAITFORWAKEUP),
    BlockedTimingAspect(BlockedTimingAspect::WAKEUP_HOOK_WORLDSTATE), //sonsor and act here
    TransformAspect(TransformAspect::ONLY_PUBLISHER, "Robotino Odometry")
{
}

void
RobotinoSimThread::init()
{
  //get a connection to gazebo (copied from gazeboscene)
  logger->log_debug(name(), "Creating Gazebo publishers");

  //read config values
  slippery_wheels_enabled_ = config->get_bool("gazsim/robotino/motor/slippery-wheels-enabled");
  slippery_wheels_threshold_ = config->get_float("gazsim/robotino/motor/slippery-wheels-threshold");
  moving_speed_factor_ = config->get_float("gazsim/robotino/motor/moving-speed-factor");
  rotation_speed_factor_ = config->get_float("gazsim/robotino/motor/rotation-speed-factor");
  gripper_laser_threshold_ = config->get_float("/gazsim/robotino/gripper-laser-threshold");
  gripper_laser_value_far_ = config->get_float("/gazsim/robotino/gripper-laser-value-far");
  gripper_laser_value_near_ = config->get_float("/gazsim/robotino/gripper-laser-value-near");
  have_gripper_sensors_ = config->exists("/hardware/robotino/sensors/right_ir_id")
    && config->exists("/hardware/robotino/sensors/left_ir_id");
  if(have_gripper_sensors_)
  {
    gripper_laser_right_pos_ = config->get_int("/hardware/robotino/sensors/right_ir_id");
    gripper_laser_left_pos_ = config->get_int("/hardware/robotino/sensors/left_ir_id");
  }
  
  //Open interfaces
  motor_if_ = blackboard->open_for_writing<MotorInterface>("Robotino");
  switch_if_ = blackboard->open_for_writing<fawkes::SwitchInterface>("Robotino Motor");
  sens_if_ = blackboard->open_for_writing<RobotinoSensorInterface>("Robotino");

  //Create suscribers
  pos_sub_ = gazebonode->Subscribe(config->get_string("/gazsim/topics/gps"), &RobotinoSimThread::on_pos_msg, this);
  gyro_sub_ = gazebonode->Subscribe(config->get_string("/gazsim/topics/gyro"), &RobotinoSimThread::on_gyro_msg, this);
  if(have_gripper_sensors_)
  {
    infrared_puck_sensor_sub_ = gazebonode->Subscribe(config->get_string("/gazsim/topics/infrared-puck-sensor"), &RobotinoSimThread::on_infrared_puck_sensor_msg, this);
    gripper_laser_left_sensor_sub_ = gazebonode->Subscribe(config->get_string("/gazsim/topics/gripper-laser-left"), &RobotinoSimThread::on_gripper_laser_left_sensor_msg, this);
    gripper_laser_right_sensor_sub_ = gazebonode->Subscribe(config->get_string("/gazsim/topics/gripper-laser-right"), &RobotinoSimThread::on_gripper_laser_right_sensor_msg, this);
  }

  //Create publishers
  motor_move_pub_ = gazebonode->Advertise<msgs::Vector3d>(config->get_string("/gazsim/topics/motor-move"));
  string_pub_ = gazebonode->Advertise<msgs::Header>(config->get_string("/gazsim/topics/message"));

  //enable motor by default
  switch_if_->set_enabled(true);
  switch_if_->write();

  //init motor variables
  x_ = 0.0;
  y_ = 0.0;
  ori_ = 0.0;
  vx_ = 0.0;
  vy_ = 0.0;
  vomega_ = 0.0;
  des_vx_ = 0.0;
  des_vy_ = 0.0;
  des_vomega_ = 0.0;
  x_offset_ = 0.0;
  y_offset_ = 0.0;
  ori_offset_ = 0.0;
  path_length_ = 0.0;

  new_data_ = false;

  if(string_pub_->HasConnections())
  {
    msgs::Header helloMessage;
    helloMessage.set_str_id("gazsim-robotino plugin connected");
    string_pub_->Publish(helloMessage);

  }
}

void
RobotinoSimThread::finalize()
{
  //close interfaces
  blackboard->close(sens_if_);
  blackboard->close(motor_if_);
  blackboard->close(switch_if_);
}

void
RobotinoSimThread::loop()
{
  //work off all messages passed to the motor_interfaces
  process_motor_messages();

  //update interfaces
  if(new_data_)
  {
    motor_if_->set_odometry_position_x(x_);
    motor_if_->set_odometry_position_y(y_);
    motor_if_->set_odometry_orientation(ori_);
    motor_if_->set_odometry_path_length(path_length_);
    motor_if_->write();

    sens_if_->set_gyro_available(gyro_available_);
    sens_if_->set_gyro_angle(gyro_angle_);
    if(have_gripper_sensors_)
    {
      sens_if_->set_distance(8, infrared_puck_sensor_dist_);
      sens_if_->set_analog_in(gripper_laser_left_pos_, analog_in_left_);
      sens_if_->set_analog_in(gripper_laser_right_pos_, analog_in_right_);
    }    
    sens_if_->write();

    new_data_ = false;
  }
}

void RobotinoSimThread::send_transroot(double vx, double vy, double omega)
{
  msgs::Vector3d motorMove;
  motorMove.set_x(vx_);
  motorMove.set_y(vy_);
  motorMove.set_z(vomega_);
  motor_move_pub_->Publish(motorMove);
}

void RobotinoSimThread::process_motor_messages()
{
  //check messages of the switch interface
  while (!switch_if_->msgq_empty()) {
    if (SwitchInterface::DisableSwitchMessage *msg =
	switch_if_->msgq_first_safe(msg)) {
      switch_if_->set_enabled(false);
      //pause movement
      send_transroot(0, 0, 0);
    } else if (SwitchInterface::EnableSwitchMessage *msg =
	       switch_if_->msgq_first_safe(msg)) {
      switch_if_->set_enabled(true);
      //unpause movement
      send_transroot(vx_, vy_, vomega_);
    }
    switch_if_->msgq_pop();
    switch_if_->write();
  }

  //do not do anything if the motor is disabled
  if(!switch_if_->is_enabled())
  {
    return;
  }

  //check messages of the motor interface
  while(motor_move_pub_->HasConnections() && !motor_if_->msgq_empty())
  {
    if (MotorInterface::TransRotMessage *msg =
	motor_if_->msgq_first_safe(msg))
    {
      //send command only if changed
      if(vel_changed(msg->vx(), vx_, 0.01) || vel_changed(msg->vy(), vy_, 0.01) || vel_changed(msg->omega(), vomega_, 0.01))
      {
	vx_ = msg->vx();
	vy_ = msg->vy();
	vomega_ = msg->omega();
	des_vx_ = vx_;
	des_vy_ = vy_;
	des_vomega_ = vomega_;
	
	//send message to gazebo (apply movement_factor to compensate friction)
	send_transroot(vx_ * moving_speed_factor_, vy_ * moving_speed_factor_, vomega_ * rotation_speed_factor_);

	//update interface
	motor_if_->set_vx(vx_);
	motor_if_->set_vy(vy_);
	motor_if_->set_omega(vomega_);
	motor_if_->set_des_vx(des_vx_);
	motor_if_->set_des_vy(des_vy_);
	motor_if_->set_des_omega(des_vomega_);
	//update interface
	motor_if_->write();
      }    
    }
    else if (motor_if_->msgq_first_is<MotorInterface::ResetOdometryMessage>())
      {
        x_offset_ += x_;
	y_offset_ += y_;
        ori_offset_ += ori_;
	x_ = 0.0;
	y_ = 0.0;
	ori_ = 0.0;
	last_vel_set_time_ = clock->now();
      }
    motor_if_->msgq_pop();
  }
}

bool RobotinoSimThread::vel_changed(float before, float after, float relativeThreashold)
{
  return(before == 0.0 || after == 0.0 || fabs((before-after)/before) > relativeThreashold);
}


//Handler Methods:
void RobotinoSimThread::on_pos_msg(ConstPosePtr &msg)
{
  //logger_->log_debug(name_, "Got Position MSG from gazebo with ori: %f", msg->z());

  MutexLocker lock(loop_mutex);

  //read out values + substract offset
  float new_x = msg->position().x() - x_offset_;
  float new_y = msg->position().y() - y_offset_;
  //calculate ori from quaternion
  float new_ori = tf::get_yaw(tf::Quaternion(msg->orientation().x(), msg->orientation().y()
					  , msg->orientation().z(), msg->orientation().w()));
  new_ori -= ori_offset_;
  
  //estimate path-length
  float length_driven = sqrt((new_x-x_) * (new_x-x_) + (new_y-y_) * (new_y-y_));
  
  if(slippery_wheels_enabled_)
  {
    //simulate slipping wheels when driving against an obstacle
    fawkes::Time new_time = clock->now();
    double duration = new_time.in_sec() - last_pos_time_.in_sec();
    //calculate duration since the velocity was last set to filter slipping while accelerating
    double velocity_set_duration = new_time.in_sec() - last_vel_set_time_.in_sec();

    last_pos_time_ = new_time;
    

    double total_speed = sqrt(vx_ * vx_ + vy_ * vy_);
    if(length_driven < total_speed * duration * slippery_wheels_threshold_ && velocity_set_duration > duration)
    {
      double speed_abs_x = vx_ * cos(ori_) - vy_ * sin(ori_);
      double speed_abs_y = vx_ * sin(ori_) + vy_ * cos(ori_);
      double slipped_x = speed_abs_x * duration * slippery_wheels_threshold_;
      double slipped_y = speed_abs_y * duration * slippery_wheels_threshold_;
      //logger_->log_debug(name_, "Wheels are slipping (%f, %f)", slipped_x, slipped_y);
      new_x = x_ + slipped_x;
      new_y = y_ + slipped_y;
      //update the offset (otherwise the slippery error would be corrected in the next iteration)
      x_offset_ -= slipped_x;
      y_offset_ -= slipped_y;	      

      length_driven = sqrt((new_x-x_) * (new_x-x_) + (new_y-y_) * (new_y-y_));
    }
  }

  //update stored values
  x_ = new_x;
  y_ = new_y;
  ori_ = new_ori;
  path_length_ += length_driven;
  new_data_ = true;

  //publish transform (otherwise the transform can not convert /base_link to /odom)
  fawkes::Time now(clock);
  tf::Transform t(tf::Quaternion(tf::Vector3(0,0,1),ori_),
		  tf::Vector3(x_, y_, 0.0));

  tf_publisher->send_transform(t, now, "/odom", "/base_link");
}
void RobotinoSimThread::on_gyro_msg(ConstVector3dPtr &msg)
{
  MutexLocker lock(loop_mutex);

  gyro_angle_ = msg->z();
  gyro_available_ = true;
  new_data_ = true;
}
void RobotinoSimThread::on_infrared_puck_sensor_msg(ConstFloatPtr &msg)
{
  MutexLocker lock(loop_mutex);

  //make sure that the config values for fetch_puck are set right
  infrared_puck_sensor_dist_ = msg->value();
  new_data_ = true;
}
void RobotinoSimThread::on_gripper_laser_right_sensor_msg(ConstFloatPtr &msg)
{
  MutexLocker lock(loop_mutex);

  if(msg->value() < gripper_laser_threshold_)
  {
    analog_in_right_ = gripper_laser_value_near_;
  }
  else
  {
    analog_in_right_ = gripper_laser_value_far_;
  }
  new_data_ = true;
}
void RobotinoSimThread::on_gripper_laser_left_sensor_msg(ConstFloatPtr &msg)
{
  MutexLocker lock(loop_mutex);

  if(msg->value() < gripper_laser_threshold_)
  {
    analog_in_left_ = gripper_laser_value_near_;
  }
  else
  {
    analog_in_left_ = gripper_laser_value_far_;
  }
  new_data_ = true;
}
