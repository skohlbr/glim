#include <glim/preprocess/cloud_preprocessor.hpp>

#include <iostream>
#include <gtsam_ext/ann/kdtree.hpp>

#include <glim/util/config.hpp>
#include <glim/util/console_colors.hpp>
#include <glim/preprocess/downsampling.hpp>

namespace glim {

CloudPreprocessorParams::CloudPreprocessorParams() {
  Config config(GlobalConfig::get_config_path("config_preprocess"));

  use_random_grid_downsampling = config.param<bool>("preprocess", "use_random_grid_downsampling", false);

  distance_near_thresh = config.param<double>("preprocess", "distance_near_thresh", 1.0);
  distance_far_thresh = config.param<double>("preprocess", "distance_far_thresh", 100.0);
  downsample_resolution = config.param<double>("preprocess", "downsample_resolution", 0.15);
  downsample_rate = config.param<double>("preprocess", "random_downsample_rate", 0.3);
  k_correspondences = config.param<int>("preprocess", "k_correspondences", 8);
}

CloudPreprocessorParams::~CloudPreprocessorParams() {}

CloudPreprocessor::CloudPreprocessor(const CloudPreprocessorParams& params) : params(params) {}

CloudPreprocessor::~CloudPreprocessor() {}

PreprocessedFrame::Ptr CloudPreprocessor::preprocess(double stamp, const std::vector<double>& times, const Points& points) const {
  auto frame = distance_filter(times, points);
  if (params.use_random_grid_downsampling) {
    frame = downsample_randomgrid(frame->times, frame->points, mt, params.downsample_resolution, params.downsample_rate);
  } else {
    frame = downsample(frame->times, frame->points, params.downsample_resolution);
  }
  frame = sort_by_time(frame->times, frame->points);

  frame->stamp = stamp;
  frame->scan_end_time = stamp + frame->times.back();
  frame->k_neighbors = params.k_correspondences;
  frame->neighbors = find_neighbors(frame->points, params.k_correspondences);

  return frame;
}

PreprocessedFrame::Ptr CloudPreprocessor::sort_by_time(const std::vector<double>& times, const Points& points) const {
  // sort by time
  std::vector<int> indices(times.size());
  std::iota(indices.begin(), indices.end(), 0);
  std::sort(indices.begin(), indices.end(), [&](const int& lhs, const int& rhs) { return times[lhs] < times[rhs]; });

  PreprocessedFrame::Ptr sorted(new PreprocessedFrame());
  sorted->times.resize(times.size());
  sorted->points.resize(points.size());
  for (int i = 0; i < times.size(); i++) {
    const int index = indices[i];
    sorted->times[i] = times[index];
    sorted->points[i] = points[index];
  }

  return sorted;
}

PreprocessedFrame::Ptr CloudPreprocessor::distance_filter(const std::vector<double>& times, const Points& points) const {
  PreprocessedFrame::Ptr filtered(new PreprocessedFrame());
  filtered->times.reserve(times.size());
  filtered->points.reserve(points.size());

  for (int i = 0; i < points.size(); i++) {
    const double dist = points[i].norm();
    if (!std::isfinite(dist)) {
      std::cout << console::yellow << "warning: an invalid point found!! " << points[i].transpose() << console::reset << std::endl;
      continue;
    }

    if (dist < params.distance_near_thresh || dist > params.distance_far_thresh) {
      continue;
    }

    filtered->times.push_back(times[i]);
    filtered->points.push_back(points[i]);
  }

  return filtered;
}

std::vector<int> CloudPreprocessor::find_neighbors(const Points& points, int k) const {
  gtsam_ext::KdTree tree(points.data(), points.size());

  std::vector<int> neighbors(points.size() * k);

  std::vector<size_t> k_indices(k);
  std::vector<double> k_sq_dists(k);
  for (int i = 0; i < points.size(); i++) {
    tree.knn_search(points[i].data(), k, k_indices.data(), k_sq_dists.data());
    std::copy(k_indices.begin(), k_indices.end(), neighbors.begin() + i * k);
  }

  return neighbors;
}

}  // namespace glim