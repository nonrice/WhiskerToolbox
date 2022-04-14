#ifndef VIDEODECODER_H
#define VIDEODECODER_H

#include "libavinc.hpp"
#include <string>
#include <vector>

class VideoDecoder {

public:
    VideoDecoder();
    VideoDecoder(std::string filename);
    void createMedia(std::string filename);
    std::vector<uint8_t> getFrame(int frame_id, bool frame_by_frame = false);

    int getFrameCount() const;
    int getWidth() const;
    int getHeight() const;

private:
    libav::AVFormatContext media; //This is a unique_ptr
    libav::AVPacket pkt; //This is a unique ptr

    int frame_count;
    long long last_decoded_frame;
    int width;
    int height;
    void yuv420togray8(std::shared_ptr<::AVFrame>& frame,std::vector<uint8_t>&);
};



#endif // VIDEODECODER_H
