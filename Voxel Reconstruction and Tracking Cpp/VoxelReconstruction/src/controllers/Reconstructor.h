/*
 * Reconstructor.h
 *
 *  Created on: Nov 15, 2013
 *      Author: coert
 */

#ifndef RECONSTRUCTOR_H_
#define RECONSTRUCTOR_H_

#include <opencv2/core/core.hpp>
#include <opencv2/core/operations.hpp>
#include <stddef.h>
#include <vector>

#include "Camera.h"
#include "PixelVoxel.h"

namespace nl_uu_science_gmt
{

	class Reconstructor
	{
	public:
		std::vector<cv::Mat> m_camera_foregrounds;			// NEW:  Previous foregrounds of each camera

	private:
		const std::vector<Camera*> &m_cameras;  // vector of pointers to cameras
		const int m_height;                     // Cube half-space height from floor to ceiling
		const int m_step;                       // Step size (space between voxels)

		std::vector<cv::Point3f*> m_corners;    // Cube half-space corner locations

		size_t m_voxels_amount;                 // Voxel count
		cv::Size m_plane_size;                  // Camera FoV plane WxH

		std::vector<PixelVoxel::Voxel*> m_voxels;           // Pointer vector to all voxels in the half-space
		std::vector<PixelVoxel::Voxel*> m_visible_voxels;   // Pointer vector to all visible voxels

		std::vector<cv::Mat> m_camera_xors;					// NEW:  Xors for each camera on this frame

		cv::Mat m_cluster_labels;                           // NEW:  The labels for the clusters
		cv::Mat m_cluster_centers;                          // NEW:  The cluster centers

		std::vector < std::vector<int>> m_initial_histograms;   // NEW:  Histograms for clusters 0-3

		std::vector<int> m_best;								// NEW:  Best matches between current and initial histograms

		std::vector<std::vector<cv::Point>> m_center_trails;		// NEW:  The trails the cluster_centers leave

		void initialize();

	public:
		Reconstructor(
			const std::vector<Camera*> &);
		virtual ~Reconstructor();

		void update();

		void init2();

		bool isFirstOn(PixelVoxel::Voxel*, int i);

		std::vector<std::vector<int>> getInitialHistograms() const
		{
			return m_initial_histograms;
		}

		std::vector<std::vector<cv::Point>> getCenterTrails() const
		{
			return m_center_trails;
		}

		cv::Mat floorProject(std::vector<PixelVoxel::Voxel*> visibleVoxels);

		const std::vector<PixelVoxel::Voxel*>& getVisibleVoxels() const
		{
			return m_visible_voxels;
		}

		const cv::Mat getClusterCenters() const
		{
			return m_cluster_centers;
		}

		const std::vector<int> getBest() const
		{
			return m_best;
		}

		const std::vector<PixelVoxel::Voxel*>& getVoxels() const
		{
			return m_voxels;
		}

		void setVisibleVoxels(
			const std::vector<PixelVoxel::Voxel*>& visibleVoxels)
		{
			m_visible_voxels = visibleVoxels;
		}

		void setVoxels(
			const std::vector<PixelVoxel::Voxel*>& voxels)
		{
			m_voxels = voxels;
		}

		const std::vector<cv::Point3f*>& getCorners() const
		{
			return m_corners;
		}

		int getSize() const
		{
			return m_height;
		}

		const cv::Size& getPlaneSize() const
		{
			return m_plane_size;
		}
	};

} /* namespace nl_uu_science_gmt */

#endif /* RECONSTRUCTOR_H_ */
