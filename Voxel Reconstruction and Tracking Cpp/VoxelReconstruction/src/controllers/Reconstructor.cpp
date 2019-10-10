/*
 * Reconstructor.cpp
 *
 *  Created on: Nov 15, 2013
 *      Author: coert
 */

#include "Reconstructor.h"

#include <opencv2/core/mat.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/core/operations.hpp>
#include <opencv2/core/types_c.h>
#include <cassert>
#include <iostream>

#include "../utilities/General.h"
#include "../controllers/PixelVoxel.h"

using namespace std;
using namespace cv;

namespace nl_uu_science_gmt
{

	/**
	 * Constructor
	 * Voxel reconstruction class
	 */
	Reconstructor::Reconstructor(
		const vector<Camera*> &cs) :
		m_cameras(cs),
		m_height(2048),
		m_step(32)
	{
		for (size_t c = 0; c < m_cameras.size(); ++c)
		{
			if (m_plane_size.area() > 0)
				assert(m_plane_size.width == m_cameras[c]->getSize().width && m_plane_size.height == m_cameras[c]->getSize().height);
			else
				m_plane_size = m_cameras[c]->getSize();
		}

		const size_t edge = 2 * m_height;
		m_voxels_amount = (edge / m_step) * (edge / m_step) * (m_height / m_step);

		initialize();
	}

	/**
	 * Deconstructor
	 * Free the memory of the pointer vectors
	 */
	Reconstructor::~Reconstructor()
	{
		for (size_t c = 0; c < m_corners.size(); ++c)
			delete m_corners.at(c);
		for (size_t v = 0; v < m_voxels.size(); ++v)
			delete m_voxels.at(v);
	}

	/**
	 * Create some Look Up Tables
	 * 	- LUT for the scene's box corners
	 * 	- LUT with a map of the entire voxelspace: point-on-cam to voxels
	 * 	- LUT with a map of the entire voxelspace: voxel to cam points-on-cam
	 */
	void Reconstructor::initialize()
	{
		// Cube dimensions from [(-m_height, m_height), (-m_height, m_height), (0, m_height)]
		const int xL = -m_height / 4;
		const int xR = 7 * m_height / 4;
		const int yL = -m_height;
		const int yR = m_height;
		const int zL = 0;
		const int zR = m_height;
		const int plane_y = (yR - yL) / m_step;
		const int plane_x = (xR - xL) / m_step;
		const int plane = plane_y * plane_x;

		// Save the 8 volume corners
		// bottom
		m_corners.push_back(new Point3f((float)xL, (float)yL, (float)zL));
		m_corners.push_back(new Point3f((float)xL, (float)yR, (float)zL));
		m_corners.push_back(new Point3f((float)xR, (float)yR, (float)zL));
		m_corners.push_back(new Point3f((float)xR, (float)yL, (float)zL));

		// top
		m_corners.push_back(new Point3f((float)xL, (float)yL, (float)zR));
		m_corners.push_back(new Point3f((float)xL, (float)yR, (float)zR));
		m_corners.push_back(new Point3f((float)xR, (float)yR, (float)zR));
		m_corners.push_back(new Point3f((float)xR, (float)yL, (float)zR));

		// Acquire some memory for efficiency
		cout << "Initializing " << m_voxels_amount << " voxels ";
		m_voxels.resize(m_voxels_amount);

		int z;
		int pdone = 0;
#pragma omp parallel for schedule(auto) private(z) shared(pdone)
		for (z = zL; z < zR; z += m_step)
		{
			const int zp = (z - zL) / m_step;
			int done = cvRound((zp * plane / (double)m_voxels_amount) * 100.0);

#pragma omp critical
			if (done > pdone)
			{
				pdone = done;
				cout << done << "%..." << flush;
			}

			int y, x;
			for (y = yL; y < yR; y += m_step)
			{
				const int yp = (y - yL) / m_step;

				for (x = xL; x < xR; x += m_step)
				{
					const int xp = (x - xL) / m_step;

					// Create all voxels
					PixelVoxel::Voxel* voxel = new PixelVoxel::Voxel;
					voxel->x = x;
					voxel->y = y;
					voxel->z = z;
					voxel->camera_projection = vector<Point>(m_cameras.size());
					voxel->valid_camera_projection = vector<int>(m_cameras.size(), 0);

					const int p = zp * plane + yp * plane_x + xp;  // The voxel's index

					for (size_t c = 0; c < m_cameras.size(); ++c)
					{
						Camera* camera = m_cameras[c];
						Point point = m_cameras[c]->projectOnView(Point3f((float)x, (float)y, (float)z));

						// Save the pixel coordinates 'point' of the voxel projection on camera 'c'
						voxel->camera_projection[(int)c] = point;

						// If it's within the camera's FoV, flag the projection
						if (point.x >= 0 && point.x < m_plane_size.width && point.y >= 0 && point.y < m_plane_size.height)
						{
							voxel->valid_camera_projection[(int)c] = 1;
							// NEW: Since the voxel is within the camera's FoV, save the pointer to it on the pixel it is projected on
							// NEW: Also, save the distance for quick access when looking for occlusion
							m_cameras[c]->m_pixels[point.y * m_plane_size.width + point.x]->voxels.push_back(voxel);
							m_cameras[c]->m_pixels[point.y * m_plane_size.width + point.x]->distance.push_back(sqrt(pow((camera->getCameraLocation().x - voxel->x), 2) + pow((camera->getCameraLocation().y - voxel->y), 2) + pow((camera->getCameraLocation().z - voxel->z), 2)));
						}
					}

					//Writing voxel 'p' is not critical as it's unique (thread safe)
					m_voxels[p] = voxel;
				}
			}
		}

		cout << "done!" << endl;
	}

	/**
	 * Count the amount of camera's each voxel in the space appears on,
	 * if that amount equals the amount of cameras, add that voxel to the
	 * visible_voxels vector
	 */
	void Reconstructor::update()
	{
		// NEW: Save the voxels that were on in the last frame
		std::vector<PixelVoxel::Voxel*> old_on_voxels;
		old_on_voxels.insert(old_on_voxels.end(), m_visible_voxels.begin(), m_visible_voxels.end());

		m_visible_voxels.clear();
		std::vector<PixelVoxel::Voxel*> visible_voxels;

		// NEW: Bitwise_XOR each camera foreground with its last-frame foreground to determine changed pixels, and add their voxels to the old_on_voxels to be checked
		for (size_t c = 0; c < m_camera_foregrounds.size(); ++c)
		{
			bitwise_xor(m_camera_foregrounds[c], m_cameras[c]->getForegroundImage(), m_camera_xors[c]);
			vector<Point> nonZeroes;
			findNonZero(m_camera_xors[c], nonZeroes);
			for (int i = 0; i < nonZeroes.size(); i++)
			{
				old_on_voxels.insert(old_on_voxels.end(), m_cameras[c]->m_pixels[nonZeroes[i].y * m_camera_xors[c].cols + nonZeroes[i].x]->voxels.begin(), m_cameras[c]->m_pixels[nonZeroes[i].y * m_camera_xors[c].cols + nonZeroes[i].x]->voxels.end());
			}
		}

		m_camera_foregrounds.clear();

		// NEW: Save current camera foregrounds for the next frame
		for (size_t c = 0; c < m_cameras.size(); ++c)
		{
			m_camera_foregrounds.push_back(m_cameras[c]->getForegroundImage());
		}

		int v;
		if (old_on_voxels.size() > 0 && !m_cameras[0]->getRefresh())
		{
			// NEW: if a previous frame exists, only check voxels depending on the previously on voxels and changed voxels
#pragma omp parallel for schedule(auto) private(v) shared(visible_voxels)
			for (v = 0; v < (int)old_on_voxels.size(); ++v)
			{
				int camera_counter = 0;
				PixelVoxel::Voxel* voxel = old_on_voxels[v];

				for (size_t c = 0; c < m_cameras.size(); ++c)
				{
					if (voxel->valid_camera_projection[c])
					{
						const Point point = voxel->camera_projection[c];

						//If there's a white pixel on the foreground image at the projection point, add the camera
						if (m_cameras[c]->getForegroundImage().at<uchar>(point) == 255) ++camera_counter;
					}
				}

				// If the voxel is present on all cameras
				if (camera_counter == m_cameras.size())
				{
#pragma omp critical //push_back is critical
					visible_voxels.push_back(voxel);
				}
			}
		}
		else
		{
			// NEW: Only perform this check when old_on_voxels.size() == 0, so only during the first frame (hopefully)
			m_camera_xors.resize(4);
			m_center_trails.resize(4);
			m_cluster_centers = Mat();
#pragma omp parallel for schedule(auto) private(v) shared(visible_voxels)
			for (v = 0; v < (int)m_voxels_amount; ++v)
			{
				int camera_counter = 0;
				PixelVoxel::Voxel* voxel = m_voxels[v];

				for (size_t c = 0; c < m_cameras.size(); ++c)
				{
					if (voxel->valid_camera_projection[c])
					{
						const Point point = voxel->camera_projection[c];

						//If there's a white pixel on the foreground image at the projection point, add the camera
						if (m_cameras[c]->getForegroundImage().at<uchar>(point) == 255) ++camera_counter;
					}
				}

				// If the voxel is present on all cameras
				if (camera_counter == m_cameras.size())
				{
#pragma omp critical //push_back is critical
					visible_voxels.push_back(voxel);
					// NEW: Set voxel visible for quick occlusion checks
					voxel->visible = true;
				}
				else
				{
					voxel->visible = false;
				}
			}
			for (int i = 0; i < m_cameras.size(); i++)
			{
				m_cameras[i]->setRefresh(false);
			}
		}

		m_visible_voxels.insert(m_visible_voxels.end(), visible_voxels.begin(), visible_voxels.end());
		TermCriteria criteria = TermCriteria(CV_TERMCRIT_EPS + CV_TERMCRIT_ITER, 10, 1.0);

		// NEW: check how close the voxels are to centers, and only show those that are close to one (from the previous frame)
		if (m_cluster_centers.rows > 0)
		{
			vector<PixelVoxel::Voxel*> closeVoxels;
			closeVoxels.insert(closeVoxels.end(), m_visible_voxels.begin(), m_visible_voxels.end());

			m_visible_voxels.clear();

			for (int i = 0; i < closeVoxels.size(); i++)
			{
				int closenessCounter = 0;
				for (int j = 0; j < m_cluster_centers.rows; j++)
				{
					if (pow(closeVoxels[i]->x - m_cluster_centers.at<float>(j, 0), 2) + pow(closeVoxels[i]->y - m_cluster_centers.at<float>(j, 1), 2) < 202400)
					{
						closenessCounter++;
					}
				}
				if (closenessCounter > 0)
				{
					m_visible_voxels.push_back(closeVoxels[i]);
					closeVoxels[i]->visible = true;
				}
				else
				{
					closeVoxels[i]->visible = false;
				}
			}
		}

		// NEW: Project points on the floor for easier clustering
		Mat points = floorProject(m_visible_voxels);

		// NEW: Calculate kmeans
		double compactness = kmeans(points, 4, m_cluster_labels, TermCriteria(TermCriteria::EPS + TermCriteria::COUNT, 10, 1.0), 10, KMEANS_PP_CENTERS, m_cluster_centers);

		//histogram pseudocode (takes the hue, saturation and value of the colour of a pixel, returns which bin this belongs in)
		//int blackThreshold, greyThreshold, RGBgreyThreshold, whiteThreshold, RGborder, GBborder, BRborder, RYborder, YGborder, GCborder, CBborder, BMborder, MRborder;
		//Hue 0-360 with red at start and end, Saturation and Value are 0-100 (hah, that was WRONG, they go from 0-255...)
		//360/3 = 120 size of RGBgrey ranges, 60 size of RGBCYM ranges (these have been mapped to 0-255 instead)

		// NEW: Get HSV Values per camera
		vector<Mat> hsvCameraFrames;
		for (int i = 0; i < m_cameras.size(); i++)
		{
			Mat hsv_image;
			cvtColor(m_cameras[i]->getFrame(), hsv_image, CV_BGR2HSV);
			hsvCameraFrames.push_back(hsv_image);
		}

		//initial guesses for good values
		int blackThreshold = 50;
		int greyThreshold = 128;
		int whiteThreshold = 25;
		//int RGborder = 43, GBborder = 128, BRborder = 213, RYborder = 21, YGborder = 64, GCborder = 107, CBborder = 149, BMborder = 192, MRborder = 234;
		int RGborder = 43, GBborder = 128, BRborder = 213, RYborder = 64, YGborder = 85, GCborder = 107, CBborder = 128, BMborder = 170, MRborder = 212;

		// NEW: a vector for each cluster with vectors for each camera with vectors of the pixels on those cameras that see visible voxels
		vector<vector<vector<Point>>> cameraPixelProjections;
		cameraPixelProjections.resize(4);
		for (int i = 0; i < 4; i++)
		{
			cameraPixelProjections[i].resize(4);
		}

		// NEW: Add only voxels that are not occluded
		for (int i = 0; i < m_visible_voxels.size(); i++)
		{
			PixelVoxel::Voxel* voxel = m_visible_voxels[i];
			for (int j = 0; j < m_cameras.size(); j++)
			{
				if (voxel->valid_camera_projection[j] && isFirstOn(voxel, j))
				{
					cameraPixelProjections[m_cluster_labels.at<int>(i)][j].push_back(voxel->camera_projection[j]);
				}
			}
		}

		vector<vector<int>> current_histograms;
		current_histograms.resize(4);
		current_histograms[0].resize(12);
		current_histograms[1].resize(12);
		current_histograms[2].resize(12);
		current_histograms[3].resize(12);

		for (int i = 0; i < 12; i++)
		{
			current_histograms[0][i] = 0;
			current_histograms[1][i] = 0;
			current_histograms[2][i] = 0;
			current_histograms[3][i] = 0;
		}

		for (int i = 0; i < cameraPixelProjections.size(); i++)
		{
			for (int j = 0; j < cameraPixelProjections[i].size(); j++)
			{
				for (int k = 0; k < cameraPixelProjections[i][j].size(); k++)
				{
					Vec3b hsv = hsvCameraFrames[j].at<Vec3b>(cameraPixelProjections[i][j][k]);
					if (hsv[2] < blackThreshold)
					{
						current_histograms[i][0]++;/*return bin = black*/
					}
					else if (hsv[2] > greyThreshold)
					{
						if (hsv[1] < whiteThreshold) { current_histograms[i][1]++;/*return bin = grey*/ }
						else if (BRborder < hsv[0] || hsv[0] < RGborder) { current_histograms[i][2]++;/*return bin = greyRed*/ } //here OR because red is at extreme hue values
						else if (RGborder < hsv[0] && hsv[0] < GBborder) { current_histograms[i][3]++;/*return bin = greyGreen*/ }
						else if (GBborder < hsv[0] && hsv[0] < BRborder) { current_histograms[i][4]++;/*return bin = greyBlue*/ }
					}
					else if (hsv[1] < whiteThreshold) { current_histograms[i][5]++;/*return bin = white*/ }
					else {
						if (MRborder < hsv[0] || hsv[0] < RYborder) { current_histograms[i][6]++;/*return bin = Red*/ } //here OR because red is at extreme hue values
						else if (RYborder < hsv[0] && hsv[0] < YGborder) { current_histograms[i][7]++;/*return bin = Yellow*/ }
						else if (YGborder < hsv[0] && hsv[0] < GCborder) { current_histograms[i][8]++;/*return bin = Green*/ }
						else if (GCborder < hsv[0] && hsv[0] < CBborder) { current_histograms[i][9]++;/*return bin = Cyan*/ }
						else if (CBborder < hsv[0] && hsv[0] < BMborder) { current_histograms[i][10]++;/*return bin = Blue*/ }
						else if (BMborder < hsv[0] && hsv[0] < MRborder) { current_histograms[i][11]++;/*return bin = Magenta*/ }
					}
				}
			}
		}

		// Compare values, then decide which label is which;
		vector<vector<float>> chiSquaredDistances;
		chiSquaredDistances.resize(4);
		chiSquaredDistances[0].resize(4);
		chiSquaredDistances[1].resize(4);
		chiSquaredDistances[2].resize(4);
		chiSquaredDistances[3].resize(4);
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				chiSquaredDistances[i][j] = 0;
			}
		}
		for (int j = 0; j < cameraPixelProjections.size(); j++)
		{
			for (int i = 0; i < 12; i++) 
			{
				for (int k = 0; k < 4; k++)
				{
					if (current_histograms[j][i] + m_initial_histograms[k][i] == 0)
					{
						chiSquaredDistances[j][k] += 0;
					}
					else
					{
						chiSquaredDistances[j][k] += (pow((current_histograms[j][i] - m_initial_histograms[k][i]), 2) / (current_histograms[j][i] + m_initial_histograms[k][i]));
					}
				}
			}
		}
		// NEW: m_best contains the conversion from current clusters to initial clusters
		m_best.clear();
		m_best.resize(4);
		float combinedDistance;
		float minDistance = 1000000.0;
		for (int i = 0; i < 4; i++) 
		{
			for (int j = 0; j< 3; j++) 
			{
				for (int k = 0; k < 2; k++) 
				{
					vector<int> a = { 0,1,2,3 };
					combinedDistance = 0.0;
					combinedDistance += chiSquaredDistances[0][a[i]];
					a.erase(a.begin()+i);
					combinedDistance += chiSquaredDistances[1][a[j]];
					a.erase(a.begin() + j);
					combinedDistance += chiSquaredDistances[2][a[k]];
					a.erase(a.begin() + k);
					combinedDistance += chiSquaredDistances[3][a[0]];
					if (combinedDistance < minDistance) 
					{
						vector<int> a = { 0,1,2,3 };
						m_best[0] = a[i];
						a.erase(a.begin() + i);
						m_best[1] = a[j];
						a.erase(a.begin() + j);
						m_best[2] = a[k];
						a.erase(a.begin() + k);
						m_best[3] = a[0];
						minDistance = combinedDistance;
					}
				}
			}
		}

		// NEW: Save the trails of cluster centers
		for (int i = 0; i < 4; i++)
		{
			m_center_trails[m_best[i]].push_back(Point(m_cluster_centers.at<float>(i, 0), m_cluster_centers.at<float>(i, 1)));
		}

		// NEW: Add converted labels to each voxel
		for (int i = 0; i < m_visible_voxels.size(); i++)
		{
			PixelVoxel::Voxel* voxel = m_visible_voxels[i];
			voxel->label = m_best[m_cluster_labels.at<int>(i)];
		}
	}

	void Reconstructor::init2()
	{
		// NEW: Almost copy-paste of Update() but with specific changes for initial run, especially the Histogram stuff
		std::vector<PixelVoxel::Voxel*> visible_voxels;

		int v;
#pragma omp parallel for schedule(auto) private(v) shared(visible_voxels)
		for (v = 0; v < (int)m_voxels_amount; ++v)
		{
			int camera_counter = 0;
			PixelVoxel::Voxel* voxel = m_voxels[v];

			for (size_t c = 0; c < m_cameras.size(); ++c)
			{
				if (voxel->valid_camera_projection[c])
				{
					const Point point = voxel->camera_projection[c];

					//If there's a white pixel on the foreground image at the projection point, add the camera
					if (m_cameras[c]->getForegroundImage().at<uchar>(point) == 255) ++camera_counter;
				}
			}

			// If the voxel is present on all cameras
			if (camera_counter == m_cameras.size())
			{
#pragma omp critical //push_back is critical
				visible_voxels.push_back(voxel);
				voxel->visible = true;
			}
			else
			{
				voxel->visible = false;
			}
		}

		TermCriteria criteria = TermCriteria(CV_TERMCRIT_EPS + CV_TERMCRIT_ITER, 10, 1.0);
		Mat points = floorProject(visible_voxels);

		Mat clusterLabels, clusterCenters;

		double compactness = kmeans(points, 4, clusterLabels, TermCriteria(TermCriteria::EPS + TermCriteria::COUNT, 10, 1.0), 10, KMEANS_PP_CENTERS, clusterCenters);

		vector<PixelVoxel::Voxel*> closeVoxels;
		closeVoxels.insert(closeVoxels.end(), visible_voxels.begin(), visible_voxels.end());

		visible_voxels.clear();

		for (int i = 0; i < closeVoxels.size(); i++)
		{
			int closenessCounter = 0;
			for (int j = 0; j < clusterCenters.rows; j++)
			{
				if (pow(closeVoxels[i]->x - clusterCenters.at<float>(j, 0), 2) + pow(closeVoxels[i]->y - clusterCenters.at<float>(j, 1), 2) < 202400)
				{
					closenessCounter++;
				}
			}
			if (closenessCounter > 0)
			{
				visible_voxels.push_back(closeVoxels[i]);
				closeVoxels[i]->visible = true;
			}
			else
			{
				closeVoxels[i]->visible = false;
			}
		}

		points = floorProject(visible_voxels);

		compactness = kmeans(points, 4, clusterLabels, TermCriteria(TermCriteria::EPS + TermCriteria::COUNT, 10, 1.0), 10, KMEANS_PP_CENTERS, clusterCenters);

		m_visible_voxels.insert(m_visible_voxels.end(), visible_voxels.begin(), visible_voxels.end());

		//histogram pseudocode (takes the hue, saturation and value of the colour of a pixel, returns which bin this belongs in)
		//int blackThreshold, greyThreshold, RGBgreyThreshold, whiteThreshold, RGborder, GBborder, BRborder, RYborder, YGborder, GCborder, CBborder, BMborder, MRborder;
		//Hue 0-360 with red at start and end, Saturation and Value are 0-100 (hah, that was WRONG, they go from 0-255...)
		//360/3 = 120 size of RGBgrey ranges, 60 size of RGBCYM ranges (these have been mapped to 0-255 instead)

		vector<Mat> hsvCameraFrames;
		for (int i = 0; i < m_cameras.size(); i++)
		{
			Mat hsv_image;
			cvtColor(m_cameras[i]->getFrame(), hsv_image, CV_BGR2HSV);
			hsvCameraFrames.push_back(hsv_image);
		}

		//initial guesses for good values
		int blackThreshold = 50;
		int greyThreshold = 128;
		int whiteThreshold = 25;
		//int RGborder = 43, GBborder = 128, BRborder = 213, RYborder = 21, YGborder = 64, GCborder = 107, CBborder = 149, BMborder = 192, MRborder = 234;
		int RGborder = 43, GBborder = 128, BRborder = 213, RYborder = 64, YGborder = 85, GCborder = 107, CBborder = 128, BMborder = 170, MRborder = 212;

		vector<vector<vector<Point>>> cameraPixelProjections;
		cameraPixelProjections.resize(4);
		for (int i = 0; i < 4; i++)
		{
			cameraPixelProjections[i].resize(4);
		}

		for (int i = 0; i < visible_voxels.size(); i++)
		{
			PixelVoxel::Voxel* voxel = visible_voxels[i];
			for (int j = 0; j < m_cameras.size(); j++)
			{
				if (voxel->valid_camera_projection[j] && isFirstOn(voxel, j))
				{
					cameraPixelProjections[clusterLabels.at<int>(i)][j].push_back(voxel->camera_projection[j]);
				}
			}
		}

		m_initial_histograms.resize(4);

		m_initial_histograms[0].resize(12);
		m_initial_histograms[1].resize(12);
		m_initial_histograms[2].resize(12);
		m_initial_histograms[3].resize(12);

		for (int i = 0; i < 12; i++)
		{
			m_initial_histograms[0][i] = 0;
			m_initial_histograms[1][i] = 0;
			m_initial_histograms[2][i] = 0;
			m_initial_histograms[3][i] = 0;
		}

		for (int i = 0; i < cameraPixelProjections.size(); i++)
		{
			for (int j = 0; j < cameraPixelProjections[i].size(); j++)
			{
				for (int k = 0; k < cameraPixelProjections[i][j].size(); k++)
				{
					Vec3b hsv = hsvCameraFrames[j].at<Vec3b>(cameraPixelProjections[i][j][k]);
					if (hsv[2] < blackThreshold)
					{
						m_initial_histograms[i][0]++;/*return bin = black*/
					}
					else if (hsv[2] > greyThreshold)
					{
						if (hsv[1] < whiteThreshold) { m_initial_histograms[i][1]++;/*return bin = grey*/ }
						else if (BRborder < hsv[0] || hsv[0] < RGborder) { m_initial_histograms[i][2]++;/*return bin = greyRed*/ } //here OR because red is at extreme hue values
						else if (RGborder < hsv[0] && hsv[0] < GBborder) { m_initial_histograms[i][3]++;/*return bin = greyGreen*/ }
						else if (GBborder < hsv[0] && hsv[0] < BRborder) { m_initial_histograms[i][4]++;/*return bin = greyBlue*/ }
					}
					else if (hsv[1] < whiteThreshold) { m_initial_histograms[i][5]++;/*return bin = white*/ }
					else {
						if (MRborder < hsv[0] || hsv[0] < RYborder) { m_initial_histograms[i][6]++;/*return bin = Red*/ } //here OR because red is at extreme hue values
						else if (RYborder < hsv[0] && hsv[0] < YGborder) { m_initial_histograms[i][7]++;/*return bin = Yellow*/ }
						else if (YGborder < hsv[0] && hsv[0] < GCborder) { m_initial_histograms[i][8]++;/*return bin = Green*/ }
						else if (GCborder < hsv[0] && hsv[0] < CBborder) { m_initial_histograms[i][9]++;/*return bin = Cyan*/ }
						else if (CBborder < hsv[0] && hsv[0] < BMborder) { m_initial_histograms[i][10]++;/*return bin = Blue*/ }
						else if (BMborder < hsv[0] && hsv[0] < MRborder) { m_initial_histograms[i][11]++;/*return bin = Magenta*/ }

					}
				}
			}
		}

		m_visible_voxels.clear();
	}

	Mat Reconstructor::floorProject(vector<PixelVoxel::Voxel*> visibleVoxels)
	{
		// NEW: Project all pixels to the floor, remove z-values
		Mat m;
		for (int i = 0; i < visibleVoxels.size(); i++)
		{
			m.push_back(Point2f((float)visibleVoxels[i]->x, (float)visibleVoxels[i]->y));
			//returnVector.push_back(Point(visibleVoxels[i]->x, visibleVoxels[i]->y));
		}
		return m;
	}

	bool Reconstructor::isFirstOn(PixelVoxel::Voxel* voxel, int i)
	{
		// Check all voxels for this pixel and return whether the provided voxel is the closest visible one for that pixel
		PixelVoxel::Pixel* pixel = m_cameras[i]->m_pixels[voxel->camera_projection[i].y * m_plane_size.width + voxel->camera_projection[i].x];
		int minDistance = 1000000;
		int index = -1;
		for (int j = 0; j < pixel->distance.size(); j++)
		{
			if (pixel->distance[j] < minDistance)
			{
				if (pixel->voxels[j]->visible)
				{
					minDistance = pixel->distance[j];
					index = j;
				}
			}
		}

		return voxel == pixel->voxels[index];
	}
} /* namespace nl_uu_science_gmt */
