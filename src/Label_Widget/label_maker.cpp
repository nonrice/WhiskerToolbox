
#include "label_maker.h"

#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

LabelMaker::LabelMaker() {
    this->label_name = "Label1";
    this->saveFilePath = "./test.json";
}

void LabelMaker::addLabel(std::string frame_id, int x, int y) {
    this->point_labels[frame_id] = label_point{x,y};

    printLabels();
}

void LabelMaker::printLabels() {
    for (auto i : this->point_labels) {
        std::cout << "Label on frame " << i.first << " at location x: " << i.second.x << " y: " << i.second.y << std::endl;
    }
}

void LabelMaker::saveLabelsJSON() {

    json j = json::array();

    for (auto i : this->getLabels()) {
        json json_object = json::object();

        json_object["image"] = makeFrameName(i.first);
        json_object["labels"][this->label_name] = {i.second.x, i.second.y};
        j.push_back(json_object);
    }

    std::cout << j.dump(2) << std::endl;

}

std::string LabelMaker::makeFrameName(std::string frame_id) {

    std::stringstream a;
    a << std::setw(7) << std::setfill('0') << frame_id;
    std::string frame_name = "scene" + a.str() + ".png";

    return frame_name;
}
