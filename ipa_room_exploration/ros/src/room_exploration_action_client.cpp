#include <ros/ros.h>
#include <ros/package.h>

#include <string>
#include <vector>

#include <opencv/cv.h>
#include <opencv/highgui.h>

#include <cv_bridge/cv_bridge.h>

#include <actionlib/client/simple_action_client.h>
#include <actionlib/client/terminal_state.h>
#include <geometry_msgs/Pose2D.h>

#include <ipa_room_exploration/RoomExplorationAction.h>

int main(int argc, char **argv)
{
	ros::init(argc, argv, "room_exploration_client");

	actionlib::SimpleActionClient<ipa_room_exploration::RoomExplorationAction> ac("room_exploration/room_exploration_server", true);

	ROS_INFO("Waiting for action server to start.");
	// wait for the action server to start
	ac.waitForServer(); //will wait for infinite time

	ROS_INFO("Action server started, sending goal.");

	// read in test map
	cv::Mat map = cv::imread("/home/florianj/git/care-o-bot-indigo/src/autopnp/ipa_room_exploration/maps/map.png", 0);
	//make non-white pixels black
	for (int y = 0; y < map.rows; y++)
	{
		for (int x = 0; x < map.cols; x++)
		{
			//find not reachable regions and make them black
			if (map.at<unsigned char>(y, x) < 250)
			{
				map.at<unsigned char>(y, x) = 0;
			}
			//else make it white
			else
			{
				map.at<unsigned char>(y, x) = 255;
			}
		}
	}
//	cv::imshow("map", map);
//	cv::waitKey();
	sensor_msgs::Image labeling;

	cv_bridge::CvImage cv_image;
//	cv_image.header.stamp = ros::Time::now();
	cv_image.encoding = "mono8";
	cv_image.image = map;
	cv_image.toImageMsg(labeling);

	geometry_msgs::Pose2D map_origin;
	map_origin.x = 0.0;
	map_origin.y = 0.0;
	map_origin.theta = 0.0;

	geometry_msgs::Pose2D starting_position;
	starting_position.x = 1.0;
	starting_position.y = 1.0;
	starting_position.theta = 0.0;

	ipa_room_exploration::RoomExplorationGoal goal;
	goal.input_map = labeling;
	goal.map_origin = map_origin;
	goal.starting_position = starting_position;
	goal.map_resolution = 0.05;
	goal.robot_radius = 0.177; // turtlebot, used for sim
	ac.sendGoal(goal);
	return 0;
}
