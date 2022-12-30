#ifndef VIDEO_WINDOW_H
#define VIDEO_WINDOW_H

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsPixmapItem>
#include <QImage>
#include <ffmpeg_wrapper/videodecoder.h>

class Video_Window : public QGraphicsScene
{
Q_OBJECT
public:
    Video_Window(QObject *parent = 0) : QGraphicsScene(parent) {
        this->myimage = QImage(640,480,QImage::Format_Grayscale8);
        this->pixmap_item = addPixmap(QPixmap::fromImage(this->myimage));
        vd = std::make_unique<ffmpeg_wrapper::VideoDecoder>();
        last_loaded_frame = 0;
    }

    template <typename T>
    void addLine(std::vector<T>& x, std::vector<T>& y, QPen color) {
        QPainterPath* path = new QPainterPath();

        path->moveTo(QPointF(x[0],y[0]));

        for (int i = 1; i < x.size(); i++) {
            path->lineTo(QPointF(x[i],y[i]));
        }

        addLine(path,color);
    }

    void addLine(QPainterPath* path, QPen color) {
        line_paths.append(addPath(*path,color));
    }

    void clearLines() {
        for (auto pathItem : this->line_paths) {
            removeItem(pathItem);
        }
        this->line_paths.clear();
    }

    void UpdateCanvas(QImage& img)
    {
        clearLines();
        this->pixmap_item->setPixmap(QPixmap::fromImage(img));
    }

    std::vector<uint8_t> getCurrentFrame() const {
        return this->current_frame;
    }

    int GetVideoInfo(std::string name)
    {
        this->vid_name = name;
        this->vd->createMedia(this->vid_name);

        this->current_frame.resize(vd->getHeight()*vd->getWidth());

        return vd->getFrameCount();
    }

    // Advance from current frame by num_frames
    int AdvanceFrame(int num_frames) {
        return LoadFrame(this->last_loaded_frame + num_frames, true);
    }

    //Jump to specific frame designated by frame_id
    int LoadFrame(int frame_id,bool frame_by_frame = false)
    {
        std::vector<uint8_t> image = vd->getFrame( frame_id, frame_by_frame);

        this->current_frame = image;

        QImage img = QImage(&image[0],vd->getWidth(), vd->getHeight(), QImage::Format_Grayscale8);
        UpdateCanvas(img);
        this->last_loaded_frame = frame_id;
        return this->last_loaded_frame;
    }

    int getLastLoadedFrame() const {
        return last_loaded_frame;
    }

protected:

    void mousePressEvent(QGraphicsSceneMouseEvent *event) {
        if (event->button() == Qt::LeftButton) {
            emit leftClick(event->scenePos().x(),event->scenePos().y());
        } else if (event->button() == Qt::RightButton){

        }
    }
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {

    }
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) {

    }

    std::string vid_name;
    QVector<QGraphicsPathItem*> line_paths;
    QGraphicsPixmapItem* pixmap_item;
    QImage myimage;
    std::unique_ptr<ffmpeg_wrapper::VideoDecoder> vd;
    std::vector<uint8_t> current_frame;
    int last_loaded_frame;


signals:
    void leftClick(qreal,qreal);
};


#endif // VIDEO_WINDOW_H
