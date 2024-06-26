
#include "Tongue_Widget.hpp"

#include "utils/opencv_utility.hpp"
#include "utils/string_manip.hpp"

#include <QElapsedTimer>
#include <QFileDialog>
#include <QPushButton>
#include "qevent.h"

#include "ui_Tongue_Widget.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <filesystem>

#include "utils/string_manip.hpp"
#include "Points/Point_Data.hpp"

#include <QPushButton>
#include <opencv2/opencv.hpp>

const std::vector<QColor> tongue_colors = {
    QColor("darkRed"),
    QColor("darkGreen"),
    QColor("darkMagenta"),
    QColor("darkYellow"),
    QColor("darkBlue")
};
Tongue_Widget::Tongue_Widget(Media_Window *scene, std::shared_ptr<DataManager> data_manager, TimeScrollBar* time_scrollbar, QWidget *parent) :
    QMainWindow(parent),
    _scene{scene},
    _data_manager{data_manager},
    _time_scrollbar{time_scrollbar},
    ui(new Ui::Tongue_Widget)
{
    ui->setupUi(this);

    connect(ui->load_hdf_btn, &QPushButton::clicked, this, &Tongue_Widget::_loadHDF5TongueMasks);
    connect(ui->load_img_btn, &QPushButton::clicked, this, &Tongue_Widget::_loadImgTongueMasks);
    connect(ui->load_jaw_btn, &QPushButton::clicked, this, &Tongue_Widget::_loadCSVJawKeypoints);
};

Tongue_Widget::~Tongue_Widget() {
    delete ui;
}

void Tongue_Widget::openWidget() {

    std::cout << "Tongue Widget Opened" << std::endl;

    this->show();
}

void Tongue_Widget::closeEvent(QCloseEvent *event) {
    std::cout << "Close event detected" << std::endl;

}

void Tongue_Widget::keyPressEvent(QKeyEvent *event) {

    //Data manager should be responsible for loading new value of data object
    //Main window can update displays with new data object position
    //Frame label is also updated.

    QMainWindow::keyPressEvent(event);

}

void Tongue_Widget::_loadHDF5TongueMasks()
{
    auto filename = QFileDialog::getOpenFileName(
        this,
        "Load Tongue File",
        QDir::currentPath(),
        "All files (*.*)");

    if (filename.isNull()) {
        return;
    }

    auto frames =  _data_manager->read_array_hdf5(filename.toStdString(), "frames");
    auto probs = _data_manager->read_ragged_hdf5(filename.toStdString(), "probs");
    auto y_coords = _data_manager->read_ragged_hdf5(filename.toStdString(), "heights");
    auto x_coords = _data_manager->read_ragged_hdf5(filename.toStdString(), "widths");

    auto mask_num = _data_manager->getMaskKeys().size();

    auto mask_key = "Tongue_Mask" + std::to_string(mask_num);

    _data_manager->createMask(mask_key);

    auto mask = _data_manager->getMask(mask_key);

    for (std::size_t i = 0; i < frames.size(); i ++) {
        mask->addMaskAtTime(frames[i], x_coords[i], y_coords[i]);
    }

    _scene->addMaskDataToScene(mask_key);
    _scene->addMaskColor(mask_key, tongue_colors[mask_num]);
}

void Tongue_Widget::_loadImgTongueMasks(){
    auto const dir_name =  QFileDialog::getExistingDirectory(
                              this,
                              "Load Tongue image files",
                              QDir::currentPath()).toStdString();
    if (dir_name.empty()) {
        return;
    }
    auto dir_path = std::filesystem::path(dir_name);

    auto mask_num = _data_manager->getMaskKeys().size();
    auto mask_key = "Tongue_Mask" + std::to_string(mask_num);
    _data_manager->createMask(mask_key);
    auto mask = _data_manager->getMask(mask_key);

    mask->setMaskHeight(480);
    mask->setMaskWidth(640);

    for (const auto & img_it : std::filesystem::directory_iterator(dir_name))
    {

        auto img = load_mask_from_image(img_it.path().string(), true);

        auto img_mask = create_mask(img);

        auto const frame_num = remove_extension(extract_numbers_from_string(img_it.path().filename().string()));
        auto const frame_index = _data_manager->getMediaData()->getFrameIndexFromNumber(std::stoi(frame_num));

        mask->addMaskAtTime(frame_index, img_mask);
        //std::cout << "Added " << x_coords.size() << " pts at frame " << frame_index << '\n';
    }

    _scene->addMaskDataToScene(mask_key);
    _scene->addMaskColor(mask_key, tongue_colors[mask_num]);
}

void Tongue_Widget::_loadCSVJawKeypoints(){
    auto filename = QFileDialog::getOpenFileName(
        this,
        "Load Jaw CSV File",
        QDir::currentPath(),
        "All files (*.*)");

    if (filename.isNull()) {
        return;
    }

    auto keypoints = load_points_from_csv(filename.toStdString(), 0, 1, 2);

    auto point_num = _data_manager->getPointKeys().size();

    std::cout << "There are " << point_num << " keypoints loaded" << std::endl;

    auto keypoint_key = "keypoint_" + std::to_string(point_num);

    _data_manager->createPoint(keypoint_key);

    auto point = _data_manager->getPoint(keypoint_key);

    for (auto & [key, val] : keypoints) {
        point->addPointAtTime(key, val.x, val.y);
    }

    _scene->addPointDataToScene(keypoint_key);
    _scene->addPointColor(keypoint_key, tongue_colors[point_num]);
}


