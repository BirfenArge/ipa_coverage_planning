/*!
 *****************************************************************
 * \file
 *
 * \note
 * Copyright (c) 2017 \n
 * Fraunhofer Institute for Manufacturing Engineering
 * and Automation (IPA) \n\n
 *
 *****************************************************************
 *
 * \note
 * Project name: Care-O-bot
 * \note
 * ROS stack name: autopnp
 * \note
 * ROS package name: ipa_room_exploration
 *
 * \author
 * Author: Richard Bormann
 * \author
 * Supervised by: Richard Bormann
 *
 * \date Date of creation: 02.2017
 *
 * \brief
 *
 *
 *****************************************************************
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. \n
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution. \n
 * - Neither the name of the Fraunhofer Institute for Manufacturing
 * Engineering and Automation (IPA) nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission. \n
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License LGPL as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License LGPL for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License LGPL along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 ****************************************************************/


#include <ipa_room_exploration/room_rotator.h>

void RoomRotator::rotateRoom(const cv::Mat& room_map, cv::Mat& rotated_room_map, const cv::Mat& R, const cv::Rect& bounding_rect)
{
	// rotate the image
	cv::warpAffine(room_map, rotated_room_map, R, bounding_rect.size(), cv::INTER_NEAREST);

	// apply a binary filter to create a binary image, also use a closing operator to smooth the output (the rotation might produce
	// black pixels reaching into the white area that were not there before, causing new, wrong cells to open)
	cv::threshold(rotated_room_map, rotated_room_map, 200, 255, CV_THRESH_BINARY);
	cv::dilate(rotated_room_map, rotated_room_map, cv::Mat(), cv::Point(-1,-1), 1);
	cv::erode(rotated_room_map, rotated_room_map, cv::Mat(), cv::Point(-1,-1), 1);
}

// compute the affine rotation matrix for rotating a room into parallel alignment with x-axis (longer side of the room is aligned with x-axis)
// R is the transform
// bounding_rect is the ROI of the warped image
void RoomRotator::computeRoomRotationMatrix(const cv::Mat& room_map, cv::Mat& R, cv::Rect& bounding_rect, const double map_resolution, const cv::Point* center)
{
	// rotation angle of the map s.t. the most occurring gradient is in 90 degree to the x-axis
	double rotation_angle = computeRoomMainDirection(room_map, map_resolution);
	std::cout << "RoomRotator::computeRoomRotationMatrix: main rotation angle: " << rotation_angle << std::endl;

	// get rotation matrix R for rotating the image around the center of the room contour
	//	Remark: rotation angle in degrees for opencv

	cv::Point center_of_rotation;
	if (center == 0)
	{
		std::vector < std::vector<cv::Point> > contour;
		cv::Mat contour_map = room_map.clone();
		cv::findContours(contour_map, contour, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
		// get the moment--> for a given map, there should only be one contour
		cv::Moments moment = cv::moments(contour[0], false);
		// calculate rotation center
		center_of_rotation = cv::Point(moment.m10/moment.m00 , moment.m01/moment.m00 );
	}
	else
		center_of_rotation = *center;

	// compute rotation
	R = cv::getRotationMatrix2D(center_of_rotation, (rotation_angle*180)/CV_PI, 1.0);

	// determine bounding rectangle to find the size of the new image
	bounding_rect = cv::RotatedRect(center_of_rotation, room_map.size(), (rotation_angle*180)/CV_PI).boundingRect();

	// adjust transformation matrix
	R.at<double>(0,2) += bounding_rect.width/2.0 - center_of_rotation.x;
	R.at<double>(1,2) += bounding_rect.height/2.0 - center_of_rotation.y;
}

// computes the major direction of the walls from a map (preferably one room)
// the map (room_map, CV_8UC1) is black (0) at impassable areas and white (255) on drivable areas
double RoomRotator::computeRoomMainDirection(const cv::Mat& room_map, const double map_resolution)
{
	// compute Hough transform on edge image of the map
	cv::Mat edge_map;
	cv::Canny(room_map, edge_map, 50, 150, 3);
	std::vector<cv::Vec4i> lines;
	double min_line_length = 1.0;	// in [m]
	for (; min_line_length > 0.1; min_line_length -= 0.2)
	{
		cv::HoughLinesP(edge_map, lines, 1, CV_PI/180, min_line_length/map_resolution, min_line_length/map_resolution, 1.5*min_line_length/map_resolution);
		cv::Mat room_hough = edge_map.clone();
		for (size_t i=0; i<lines.size(); ++i)
		{
			cv::Point p1(lines[i][0], lines[i][1]), p2(lines[i][2], lines[i][3]);
			cv::line(room_hough, p1, p2, cv::Scalar(128), 2);
		}
		//cv::imshow("room_hough", room_hough);
		if (lines.size() >= 4)
			break;
	}
	// setup a histogram on the line directions weighted by their length to determine the major direction
	Histogram<double> direction_histogram(0, CV_PI, 36);
	for (size_t i=0; i<lines.size(); ++i)
	{
		double dx = lines[i][2] - lines[i][0];
		double dy = lines[i][3] - lines[i][1];
		if(dy*dy+dx*dx > 0.0)
		{
			double current_direction = std::atan2(dy, dx);
			while (current_direction < 0.)
				current_direction += CV_PI;
			while (current_direction > CV_PI)
				current_direction -= CV_PI;
			direction_histogram.addData(current_direction, sqrt(dy*dy+dx*dx));
			//std::cout << " dx=" << dx << "   dy=" << dy << "   dir=" << current_direction << "   len=" << sqrt(dy*dy+dx*dx) << std::endl;
		}
	}
	return direction_histogram.getMaxBinPreciseVal();
}

void RoomRotator::transformPathBackToOriginalRotation(const std::vector<cv::Point>& fov_middlepoint_path, std::vector<geometry_msgs::Pose2D>& path_fov_poses, const cv::Mat& R)
{
	path_fov_poses.clear();

	// transform the calculated path back to the originally rotated map
	cv::Mat R_inv;
	cv::invertAffineTransform(R, R_inv);
	std::vector<cv::Point> fov_middlepoint_path_transformed;
	cv::transform(fov_middlepoint_path, fov_middlepoint_path_transformed, R_inv);

	// create poses with an angle
	transformPointPathToPosePath(fov_middlepoint_path_transformed, path_fov_poses);
}

void RoomRotator::transformPointPathToPosePath(const std::vector<cv::Point>& point_path, std::vector<geometry_msgs::Pose2D>& pose_path)
{
	// create poses with an angle
	for(size_t point_index = 0; point_index < point_path.size(); ++point_index)
	{
		// get the vector from the previous to the current point
		const cv::Point& current_point = point_path[point_index];

		// add the next navigation goal to the path
		geometry_msgs::Pose2D current_pose;
		current_pose.x = current_point.x;
		current_pose.y = current_point.y;
		current_pose.theta = 0.;
		if (point_index > 0)
		{
			cv::Point previous_point = point_path[point_index-1];
			cv::Point vector = current_point - previous_point;
			current_pose.theta = std::atan2(vector.y, vector.x);
		}
		else if (point_path.size() >= 2)
		{
			cv::Point next_point = point_path[point_index+1];
			cv::Point vector = next_point - current_point;
			current_pose.theta = std::atan2(vector.y, vector.x);
		}
		pose_path.push_back(current_pose);
	}
}

void RoomRotator::transformPointPathToPosePath(std::vector<geometry_msgs::Pose2D>& pose_path)
{
	// create point vector
	std::vector<cv::Point> point_path;
	for (size_t i=0; i<pose_path.size(); ++i)
		point_path.push_back(cv::Point(pose_path[i].x, pose_path[i].y));

	// create poses with an angle
	pose_path.clear();
	transformPointPathToPosePath(point_path, pose_path);
}
