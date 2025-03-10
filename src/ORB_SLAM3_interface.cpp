#include "orb_slam3_ros_wrapper/ORB_SLAM3_interface.h"
#include "sophus/geometry.hpp"

#include <vector>

ORB_SLAM3_interface::ORB_SLAM3_interface(ORB_SLAM3::System* pSLAM, ros::NodeHandle* node_handle)
  : mpSLAM(pSLAM), node_handle(node_handle)
{
  frame_pub = node_handle->advertise<orb_slam3_ros_wrapper::frame>("/frames", 1);

  std::string node_name = ros::this_node::getName();
  node_handle->param<std::string>(node_name + "/map_frame_id", map_frame_id, "map");
  node_handle->param<std::string>(node_name + "/pose_frame_id", pose_frame_id, "pose");

  prev_sample_time = ros::Time::now();
}

void ORB_SLAM3_interface::rgbd_callback(const sensor_msgs::ImageConstPtr& msgRGB,
                                        const sensor_msgs::ImageConstPtr& msgD)
{
  // Copy the ros image message to cv::Mat.
  cv_bridge::CvImageConstPtr cv_ptrRGB;
  try
  {
    cv_ptrRGB = cv_bridge::toCvShare(msgRGB);
  }
  catch (cv_bridge::Exception& e)
  {
    ROS_ERROR("cv_bridge exception: %s", e.what());
    return;
  }

  cv_bridge::CvImageConstPtr cv_ptrD;
  try
  {
    cv_ptrD = cv_bridge::toCvShare(msgD);
  }
  catch (cv_bridge::Exception& e)
  {
    ROS_ERROR("cv_bridge exception: %s", e.what());
    return;
  }

  // Main algorithm runs here
  Sophus::SE3f Tcw = mpSLAM->TrackRGBD(cv_ptrRGB->image, cv_ptrD->image, cv_ptrRGB->header.stamp.toSec());

  publish_frame(Tcw, *msgRGB, *msgD, ORB_SLAM3::System::STEREO);
  //  publish_tracking_mappoints(mpSLAM->GetTrackedMapPoints(), cv_ptrRGB->header.stamp);
}

geometry_msgs::PoseStamped ORB_SLAM3_interface::SE3toPoseMsg(Sophus::SE3f tf)
{
  Eigen::Isometry3d camera_tf;
  camera_tf.matrix() = tf.matrix().cast<double>();

  geometry_msgs::PoseStamped pose_msg;
  pose_msg.header.stamp = ros::Time::now();
  pose_msg.header.frame_id = map_frame_id;

  pose_msg.pose.position.x = camera_tf.translation().x();
  pose_msg.pose.position.y = camera_tf.translation().y();
  pose_msg.pose.position.z = camera_tf.translation().z();

  Eigen::Quaterniond q(camera_tf.linear());
  pose_msg.pose.orientation.x = q.x();
  pose_msg.pose.orientation.y = q.y();
  pose_msg.pose.orientation.z = q.z();
  pose_msg.pose.orientation.w = q.w();

  return pose_msg;
}

void ORB_SLAM3_interface::publish_frame(Sophus::SE3f Tcw, sensor_msgs::Image msgRGB, sensor_msgs::Image msgD,
                                            ORB_SLAM3::System::eSensor sensor_type)
{
  orb_slam3_ros_wrapper::frame frame_msg;
  frame_msg.rgb = msgRGB;
  frame_msg.depth = msgD;
  frame_msg.pose = SE3toPoseMsg(Tcw).pose;

  frame_pub.publish(frame_msg);
}
