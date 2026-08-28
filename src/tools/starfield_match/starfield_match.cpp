/**
 * @file starfield_match.cpp
 * @brief C++ OpenCV tool for detecting star centroids in sky images.
 * @details Calculates a median-filtered background, subtracts it from the original
 * image, applies thresholding, detects connected components, filters them by size,
 * and writes the coordinates to a text file. Also generates a marked visualization image.
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <iomanip>
#include <opencv2/opencv.hpp>
#include "argparse.hpp"

int main(int argc, char* argv[]) {
    argparse::ArgumentParser program("starfield_match", "1.0.0");
    program.add_description("Detect star centroids in a background-subtracted sky image using OpenCV.");

    program.add_argument("-i", "--input")
           .help("Path to the input starfield image")
           .default_value(std::string("star_field_2.jpg"));

    program.add_argument("-o", "--output")
           .help("Path to the output text file containing star coordinates")
           .default_value(std::string("detected_stars.txt"));

    program.add_argument("-v", "--visual")
           .help("Path to the output visualization image")
           .default_value(std::string("detected_stars.jpg"));

    program.add_argument("-k", "--kernel-size")
           .help("Median filter kernel size (must be a positive odd integer)")
           .scan<'i', int>()
           .default_value(31);

    program.add_argument("-t", "--threshold")
           .help("Threshold value for binarization of subtracted image (0-255)")
           .scan<'i', int>()
           .default_value(20);

    program.add_argument("--min-size")
           .help("Minimum star size/area in pixels")
           .scan<'i', int>()
           .default_value(2);

    program.add_argument("--max-size")
           .help("Maximum star size/area in pixels")
           .scan<'i', int>()
           .default_value(500);

    try {
        program.parse_args(argc, argv);
    } catch (const std::runtime_error& err) {
        std::cerr << "Error parsing arguments: " << err.what() << std::endl;
        std::cerr << program;
        return 1;
    }

    std::string input_path = program.get<std::string>("--input");
    std::string output_txt = program.get<std::string>("--output");
    std::string output_visual = program.get<std::string>("--visual");
    int kernel_size = program.get<int>("--kernel-size");
    int threshold_val = program.get<int>("--threshold");
    int min_size = program.get<int>("--min-size");
    int max_size = program.get<int>("--max-size");

    // Validate inputs
    if (kernel_size % 2 == 0 || kernel_size <= 0) {
        std::cerr << "Error: Median kernel size must be a positive odd integer (e.g., 15, 31, 51)." << std::endl;
        return 1;
    }
    if (threshold_val < 0 || threshold_val > 255) {
        std::cerr << "Error: Threshold value must be between 0 and 255." << std::endl;
        return 1;
    }
    if (min_size < 1) {
        std::cerr << "Error: Minimum size must be at least 1 pixel." << std::endl;
        return 1;
    }
    if (max_size < min_size) {
        std::cerr << "Error: Maximum size must be greater than or equal to minimum size." << std::endl;
        return 1;
    }

    // 1. Read input image
    std::cout << "[Step 1/7] Loading input image: " << input_path << " ..." << std::endl;
    cv::Mat img = cv::imread(input_path, cv::IMREAD_COLOR);
    if (img.empty()) {
        std::cerr << "Error: Could not read image at " << input_path << std::endl;
        return 1;
    }
    std::cout << "  Image resolution: " << img.cols << "x" << img.rows << " (" << img.channels() << " channels)" << std::endl;

    // Convert to grayscale for background estimation and detection
    cv::Mat gray;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);

    // 2. Median blur to estimate varying background
    std::cout << "[Step 2/7] Estimating sky background using median filter (kernel: " << kernel_size << "x" << kernel_size << ") ..." << std::endl;
    cv::Mat background;
    cv::medianBlur(gray, background, kernel_size);

    // 3. Subtract background
    std::cout << "[Step 3/7] Subtracting background from original grayscale image ..." << std::endl;
    cv::Mat subtracted;
    cv::subtract(gray, background, subtracted);

    // Save subtracted image for debug
    std::string subtracted_path = output_txt.substr(0, output_txt.find_last_of(".")) + "_subtracted.jpg";
    cv::imwrite(subtracted_path, subtracted);
    std::cout << "  Subtracted image saved to: " << subtracted_path << std::endl;

    // 4. Threshold background subtracted image to isolate potential stars
    std::cout << "[Step 4/7] Segmenting image using threshold (" << threshold_val << ") ..." << std::endl;
    cv::Mat binary;
    cv::threshold(subtracted, binary, threshold_val, 255, cv::THRESH_BINARY);

    // 5. Detect centroids and filter blobs by size
    std::cout << "[Step 5/7] Detecting connected components and calculating centroids ..." << std::endl;
    cv::Mat labels, stats, centroids;
    int num_components = cv::connectedComponentsWithStats(binary, labels, stats, centroids);
    std::cout << "  Total blobs found (including background): " << num_components << std::endl;

    std::vector<cv::Point2d> stars;
    std::vector<int> star_areas;

    // Index 0 represents the background, so we start at 1
    for (int i = 1; i < num_components; ++i) {
        int area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area >= min_size && area <= max_size) {
            double cx = centroids.at<double>(i, 0);
            double cy = centroids.at<double>(i, 1);
            stars.push_back(cv::Point2d(cx, cy));
            star_areas.push_back(area);
        }
    }
    std::cout << "  Validated stars after size filter [" << min_size << ", " << max_size << "]: " << stars.size() << std::endl;

    // 6. Write detected stars to text file
    std::cout << "[Step 6/7] Writing star list coordinates to text file: " << output_txt << " ..." << std::endl;
    std::ofstream out_file(output_txt);
    if (!out_file.is_open()) {
        std::cerr << "Error: Could not open output text file for writing: " << output_txt << std::endl;
        return 1;
    }

    out_file << "# Starfield Centroid Detection Output" << std::endl;
    out_file << "# Input Image: " << input_path << std::endl;
    out_file << "# Total Detected Stars: " << stars.size() << std::endl;
    out_file << "# Columns: star_id x_pixel y_pixel area_pixels" << std::endl;
    
    for (size_t i = 0; i < stars.size(); ++i) {
        out_file << i << " " 
                 << std::fixed << std::setprecision(4) 
                 << stars[i].x << " " 
                 << stars[i].y << " " 
                 << star_areas[i] << std::endl;
    }
    out_file.close();
    std::cout << "  Successfully wrote coordinates for " << stars.size() << " stars." << std::endl;

    // 7. Save visualization image
    std::cout << "[Step 7/7] Generating and saving marked visualization image: " << output_visual << " ..." << std::endl;
    cv::Mat visual_img = img.clone();
    for (size_t i = 0; i < stars.size(); ++i) {
        cv::Point center(static_cast<int>(std::round(stars[i].x)), static_cast<int>(std::round(stars[i].y)));
        
        // Draw crosshair at the centroid
        cv::line(visual_img, cv::Point(center.x - 4, center.y), cv::Point(center.x + 4, center.y), cv::Scalar(0, 0, 255), 1, cv::LINE_AA);
        cv::line(visual_img, cv::Point(center.x, center.y - 4), cv::Point(center.x, center.y + 4), cv::Scalar(0, 0, 255), 1, cv::LINE_AA);
        
        // Draw circle enclosing the star
        cv::circle(visual_img, center, 8, cv::Scalar(0, 0, 255), 1, cv::LINE_AA);
        
        // Label the star with its index
        cv::putText(visual_img, std::to_string(i), cv::Point(center.x + 10, center.y - 4),
                    cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 255, 0), 1, cv::LINE_AA);
    }

    if (cv::imwrite(output_visual, visual_img)) {
        std::cout << "  Successfully saved visualization image." << std::endl;
    } else {
        std::cerr << "Error: Could not write visualization image at: " << output_visual << std::endl;
        return 1;
    }

    std::cout << "\nStarfield detection process completed successfully!" << std::endl;
    return 0;
}
