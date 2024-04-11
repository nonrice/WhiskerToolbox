//
// Created by wanglab on 4/10/2024.
//

#include "Video_Data.hpp"

VideoData::VideoData() {
    _vd = std::make_unique<ffmpeg_wrapper::VideoDecoder>();

    _last_decoded_frame = 0;
}

int VideoData::LoadMedia(std::string name) {
    setFilename(name);
    _vd->createMedia(name);

    updateHeight(_vd->getHeight());
    updateWidth(_vd->getWidth());

    this->data.resize(getWidth() * getHeight());

    setFormat(QImage::Format_Grayscale8);

    return _vd->getFrameCount(); // Total frames
}

void VideoData::LoadFrame(int frame_id) {

    //In most circumstances, we want to decode forward from the current frame without reseeking to a keyframe
    bool frame_by_frame = true;

    if (frame_id == 0) {
        frame_by_frame = false;
    } else if (frame_id >= this->getTotalFrameCount() - 1) {
        frame_by_frame = false;
    } else if (frame_id <= _last_decoded_frame) {
        frame_by_frame = false;
    }

    //We load the data associated with the frame
    this->data = _vd->getFrame( frame_id, frame_by_frame);
    _last_decoded_frame = frame_id;

}

std::string VideoData::GetFrameID(int frame_id) {
    return std::to_string(frame_id);
}

int VideoData::FindNearestSnapFrame(int frame_id) const {
    return _vd->nearest_iframe(frame_id);
}
