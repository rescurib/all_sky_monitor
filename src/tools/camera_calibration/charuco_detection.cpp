#include <iostream>
#include <vector>

#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>

int main() {
  const std::string image_path = "charuco_demo.jpg";

  cv::Mat image = cv::imread(image_path);
  if (image.empty()) 
  {
    std::cerr << "Could not read image: " << image_path << std::endl;
    return 1;
  }

  const cv::aruco::Dictionary dictionary =
      cv::aruco::getPredefinedDictionary(cv::aruco::DICT_5X5_100);

  // Create detector object with default parameters
  cv::aruco::ArucoDetector detector(dictionary);

  std::vector<std::vector<cv::Point2f>> marker_corners;
  std::vector<int> marker_ids;
  detector.detectMarkers(image, marker_corners, marker_ids);

  cv::aruco::drawDetectedMarkers(image, marker_corners, marker_ids);

  std::cout << "Detected markers: " << marker_ids.size() << std::endl;
  cv::imshow("ChArUco marker detection", image);
  cv::waitKey(0);

  return 0;
}