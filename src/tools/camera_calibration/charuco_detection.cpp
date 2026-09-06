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

  // Create board that matches the one used for calibration
  const int    squaresX      = 6;
  const int    squaresY      = 8;
  const float  squareLength  = 0.03f;
  const float  markerLength  = 0.015f;
  const auto   dictionary  = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_5X5_100);
  const auto   board       = cv::aruco::CharucoBoard(cv::Size(squaresX, squaresY), squareLength, markerLength, dictionary);

  // Create detector object 
  cv::aruco::CharucoDetector detector(board);

  std::vector<cv::Point2f> charuco_corners;
  std::vector<std::vector<cv::Point2f>> marker_corners;
  std::vector<int> charuco_ids, marker_ids;
  detector.detectBoard(image, charuco_corners, charuco_ids, marker_corners, marker_ids);

  if (!marker_ids.empty()) {
    cv::aruco::drawDetectedMarkers(image, marker_corners, marker_ids);
  }
  if (!charuco_corners.empty()) {
    // Yellow corners
    cv::aruco::drawDetectedCornersCharuco(image, charuco_corners, cv::noArray(), cv::Scalar(0, 255, 255));
  }

  std::cout << "Detected markers: " << marker_ids.size() << std::endl;
  std::cout << "Detected ChArUco corners: " << charuco_ids.size() << std::endl;
  std::cout << "Press ESC to exit" << std::endl;
  cv::imshow("ChArUco marker detection", image);
  cv::waitKey(0);

  return 0;
}