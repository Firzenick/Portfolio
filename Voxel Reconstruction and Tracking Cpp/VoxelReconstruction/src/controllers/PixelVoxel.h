#pragma once


#ifndef PIXELVOXEL_H_
#define PIXELVOXEL_H_

#include <opencv2/core/core.hpp>
#include <stddef.h>
#include <vector>

// NEW: Separate class for Voxel and Pixel, making them easily available in different parts of the program
namespace nl_uu_science_gmt
{
	class PixelVoxel
	{
	public:


		/*
		* Voxel structure
		* Represents a 3D pixel in the half space
		*/
		struct Voxel
		{
			int x, y, z;                               // Coordinates
			int label;								   // Label for colouring
			std::vector<cv::Point> camera_projection;  // Projection location for camera[c]'s FoV (2D)
			std::vector<int> valid_camera_projection;  // Flag if camera projection is in camera[c]'s FoV
			bool visible;							   // Whether the voxel is visible or not
		};

		/*
		* Pixel structure
		* Represents a 2D pixel on the camera
		*/
		struct Pixel
		{
			//int x, y;                                  // Coordinates
			std::vector<Voxel*> voxels;				   // Vector of Voxels visible from this pixel
			std::vector<int> distance;				   // 
		};
	};
}

#endif

