#ifndef OPENCV_UTILITY_HPP
#define OPENCV_UTILITY_HPP

#include "Points/Point_Data.hpp"

#include "opencv2/core/mat.hpp"

#include <vector>
#include <string>

cv::Mat convert_vector_to_mat(std::vector<uint8_t>& vec, int const width, int const height);

cv::Mat convert_vector_to_mat(std::vector<Point2D<float>>& vec, int const width, int const height);

void convert_mat_to_vector(std::vector<uint8_t>& vec, cv::Mat & mat, const int width, const int height);

std::vector<Point2D<float>> create_mask(cv::Mat const & mat);

/**
 *
 * Loads in image that is black and white. Data manager will assume that masked region is "black"
 * so invert option can be used to use "white" mask images.
 *
 * @brief load_mask_from_image
 * @param filename
 * @param invert
 * @return
 */
cv::Mat load_mask_from_image(std::string const & filename, bool const invert=false);

void grow_mask(cv::Mat & mat, int const dilation_size);

void median_blur(cv::Mat & mat, int const kernel_size);

#endif // OPENCV_UTILITY_HPP
