#include "whiskertracker.h"

#include <cmath>
#include <stdio.h>
#include "io.hpp"

Image<uint8_t> bg = Image<uint8_t>(640,480,std::vector<uint8_t>(640*480,0));

WhiskerTracker::WhiskerTracker()
{
    _janelia = JaneliaTracker();
    _janelia_init = false;
    whiskers = std::vector<Whisker>{};
}

void WhiskerTracker::trace(const std::vector<uint8_t>& image, const int image_height, const int image_width) {

    if (this->_janelia_init == false) {
        this->_janelia.bank = LineDetector(this->_janelia.config);
        this->_janelia.half_space_bank = HalfSpaceDetector(this->_janelia.config);
        this->_janelia_init = true;
    }

    whiskers.clear();

    Image<uint8_t>img = Image<uint8_t>(image_width,image_height,image);
    std::vector<Whisker_Seg> j_segs = _janelia.find_segments(1,img,bg);

    int whisker_count = 1;
    for (auto& w_seg : j_segs) {
        whiskers.push_back(Whisker(whisker_count++,std::move(w_seg.x),std::move(w_seg.y)));
    }
}

std::tuple<float,int> WhiskerTracker::get_nearest_whisker(float x_p, float y_p) {

    float nearest_distance = 1000.0;
    int whisker_id = 1;

    float current_d = 0.0f;
    int current_whisker_id = 0;

    for (auto& w : this->whiskers) {
        for (int i = 0; i < w.x.size(); i++) {
            current_d = sqrt(pow(x_p - w.x[i],2) + pow(y_p - w.y[i],2));
            if (current_d < nearest_distance) {
                nearest_distance = current_d;
                whisker_id = w.id;
            }
        }
    }

    return std::make_tuple(nearest_distance,whisker_id);
}

std::map<int,std::vector<Whisker>> WhiskerTracker::load_janelia_whiskers(const std::string filename)
{
    auto j_segs = load_binary_data(filename);

    auto output_whiskers = std::map<int,std::vector<Whisker>>();

    for (auto& w_seg : j_segs) {

        if (output_whiskers.find(w_seg.time) == output_whiskers.end()) { // Key doesn't exist
            output_whiskers[w_seg.time] = std::vector<Whisker>();
        }

         output_whiskers[w_seg.time].push_back(Whisker(w_seg.id,std::move(w_seg.x),std::move(w_seg.y)));

    }

    return output_whiskers;
}

float WhiskerTracker::calculateWhiskerLength(const Whisker& whisker)
{
    float length = 0.0;

    for (int i = 1; i < whisker.x.size(); i++)
    {
        length += sqrt(pow((whisker.x[i] - whisker.x[i-1]),2) + pow((whisker.y[i] - whisker.y[i-1]),2));
    }

    return length;
}

void WhiskerTracker::alignWhiskerToFollicle(Whisker& whisker, float follicle_x, float follicle_y)
{
    auto start_distance = sqrt(pow((whisker.x[0] - follicle_x),2) + pow((whisker.y[0] - follicle_y),2));

    auto end_distance = sqrt(pow((whisker.x.back() - follicle_x),2) + pow((whisker.y.back() - follicle_y),2));

    if (start_distance > end_distance) {
        std::reverse(whisker.x.begin(),whisker.x.end());
        std::reverse(whisker.y.begin(),whisker.y.end());
    }
}
