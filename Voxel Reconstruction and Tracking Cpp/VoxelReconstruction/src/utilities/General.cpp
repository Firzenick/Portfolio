/*
 * General.cpp
 *
 *  Created on: Nov 13, 2013
 *      Author: coert
 */

#include "General.h"

#include <fstream>

using namespace std;

namespace nl_uu_science_gmt
{

const string General::CBConfigFile         = "checkerboard2.xml";
const string General::CalibrationVideo     = "calibration.avi";
const string General::CheckerboadVideo     = "checkerboard2.avi";
const string General::BackgroundImageFile  = "background2.png";
const string General::BackgroundVideoFile  = "background2.avi"; // NEW: Actually having the video file to load
const string General::VideoFile            = "video2.avi";
const string General::IntrinsicsFile       = "intrinsics.xml";
const string General::CheckerboadCorners   = "boardcorners2.xml";
const string General::ConfigFile           = "config2.xml";

/**
 * Linux/Windows friendly way to check if a file exists
 */
bool General::fexists(const std::string &filename)
{
	ifstream ifile(filename.c_str());
	return ifile.is_open();
}

} /* namespace nl_uu_science_gmt */
