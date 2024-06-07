
#include "Whisker_Widget.h"
#include "DataManager/Lines/Line_Data.hpp"

#include <QFileDialog>
#include <QElapsedTimer>
#include "qevent.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <filesystem>

#include "ui_Whisker_Widget.h"
#include "utils/container.hpp"

const std::vector<QColor> whisker_colors = {QColor("red"),
                                            QColor("green"),
                                            QColor("cyan"),
                                            QColor("magenta"),
                                            QColor("yellow")};

Whisker_Widget::Whisker_Widget(Media_Window *scene, std::shared_ptr<DataManager> data_manager, TimeScrollBar* time_scrollbar, QWidget *parent) :
        QMainWindow(parent),
        _wt{std::make_shared<WhiskerTracker>()},
        _scene{scene},
        _data_manager{data_manager},
        _time_scrollbar{time_scrollbar},
        _selected_whisker{0},
        _selection_mode{Whisker_Select},
        _face_orientation{Facing_Top},
        _num_whisker_to_track{0},
        _save_by_frame_name{false},
        ui(new Ui::Whisker_Widget)
        {
    ui->setupUi(this);

    _data_manager->createLine("unlabeled_whiskers");
    _scene->addLineDataToScene("unlabeled_whiskers");
    _scene->addLineColor("unlabeled_whiskers",QColor("blue"));
    _janelia_config_widget = new Janelia_Config(_wt);
    _contact_widget = new Contact_Widget(_data_manager, time_scrollbar);

    connect(ui->trace_button, &QPushButton::clicked, this, &Whisker_Widget::_traceButton);
    connect(ui->actionJanelia_Settings, &QAction::triggered, this, &Whisker_Widget::_openJaneliaConfig);
    connect(ui->face_orientation, &QComboBox::currentIndexChanged, this, &Whisker_Widget::_selectFaceOrientation);
    connect(ui->length_threshold_spinbox, &QDoubleSpinBox::valueChanged, this,
            &Whisker_Widget::_changeWhiskerLengthThreshold);
    connect(ui->whisker_pad_select, &QPushButton::clicked, this, &Whisker_Widget::_selectWhiskerPad);
    connect(ui->whisker_number, &QSpinBox::valueChanged, this, &Whisker_Widget::_selectNumWhiskersToTrack);

    connect(ui->actionSave_Snapshot, &QAction::triggered, this, &Whisker_Widget::_saveImageButton);
    connect(ui->actionSave_Whisker_Mask, &QAction::triggered, this, &Whisker_Widget::_saveFaceMaskButton);
    connect(ui->export_image_csv, &QPushButton::clicked, this, &Whisker_Widget::_exportImageCSV);

    connect(ui->actionLoad_Janelia_Whiskers, &QAction::triggered, this, &Whisker_Widget::_loadJaneliaWhiskers);
    connect(ui->actionMask,  &QAction::triggered, this, &Whisker_Widget::_loadHDF5Whiskers);
    connect(ui->actionLoad_CSV_Whiskers, &QAction::triggered, this, &Whisker_Widget::_loadSingleCSVWhisker);
    connect(ui->actionLoad_CSV_Whiskers_Multiple, &QAction::triggered, this, &Whisker_Widget::_loadMultiCSVWhiskers);

    connect(ui->actionOpen_Contact_Detection, &QAction::triggered, this, &Whisker_Widget::_openContactWidget);

    connect(ui->mask_alpha_slider, &QSlider::valueChanged, this, &Whisker_Widget::_setMaskAlpha);

    connect(ui->tracked_whisker_number, &QSpinBox::valueChanged, this, &Whisker_Widget::_skipToTrackedFrame);

    connect(ui->calculate_mask, &QPushButton::clicked, this, &Whisker_Widget::_calculateMask);
};

Whisker_Widget::~Whisker_Widget() {
    delete ui;
    delete _contact_widget;
    delete _janelia_config_widget;
}

void Whisker_Widget::openWidget() {

    std::cout << "Whisker Widget Opened" << std::endl;

    connect(_scene, SIGNAL(leftClick(qreal, qreal)), this, SLOT(_clickedInVideo(qreal, qreal)));

    this->show();
}

void Whisker_Widget::closeEvent(QCloseEvent *event) {
    std::cout << "Close event detected" << std::endl;

    disconnect(_scene, SIGNAL(leftClick(qreal, qreal)), this, SLOT(_clickedInVideo(qreal, qreal)));
}

void Whisker_Widget::keyPressEvent(QKeyEvent *event) {

    //Data manager should be responsible for loading new value of data object
    //Main window can update displays with new data object position
    //Frame label is also updated.

    if (event->key() == Qt::Key_T) {
        _traceButton();
    } else if (event->key() == Qt::Key_E){
        _exportImageCSV();
    } else {
        QMainWindow::keyPressEvent(event);

    }
}

/////////////////////////////////////////////

void Whisker_Widget::_traceButton() {
    QElapsedTimer timer2;
    timer2.start();

    auto media = _data_manager->getMediaData();
    auto current_time = _data_manager->getTime()->getLastLoadedFrame();

    _wt->trace(media->getProcessedData(current_time), media->getHeight(), media->getWidth());


    //Add lines to data manager
    _addWhiskersToData();

    int t1 = timer2.elapsed();
    _drawWhiskers();

    int t2 = timer2.elapsed();

    qDebug() << "The tracing took" << t1 << "ms and drawing took" << (t2 - t1);
}

void Whisker_Widget::_selectWhiskerPad() {
    _selection_mode = Selection_Type::Whisker_Pad_Select;
}

void Whisker_Widget::_changeWhiskerLengthThreshold(double new_threshold) {
    _wt->setWhiskerLengthThreshold(static_cast<float>(new_threshold));
}

void Whisker_Widget::_selectFaceOrientation(int index) {
    if (index == 0) {
        _face_orientation = Face_Orientation::Facing_Top;
    } else if (index == 1) {
        _face_orientation = Face_Orientation::Facing_Bottom;
    } else if (index == 2) {
        _face_orientation = Face_Orientation::Facing_Left;
    } else {
        _face_orientation = Face_Orientation::Facing_Right;
    }
}

void Whisker_Widget::_selectNumWhiskersToTrack(int n_whiskers) {
    _num_whisker_to_track = n_whiskers;

    if (n_whiskers == 0) {
        return;
    }

    std::string whisker_name = "whisker_" + std::to_string(n_whiskers-1);

    _createNewWhisker(whisker_name, n_whiskers-1);
}

/////////////////////////////////////////////

void Whisker_Widget::_saveImageButton() {
    _saveImage("./");
}

void Whisker_Widget::_saveImage(std::string const& folder)
{
    auto media = _data_manager->getMediaData();
    auto const frame_id = _data_manager->getTime()->getLastLoadedFrame();

    auto data = media->getRawData(frame_id);

    auto const width = media->getWidth();
    auto const height = media->getHeight();

    QImage labeled_image(&data[0], width, height, QImage::Format_Grayscale8);

    auto saveName = _getImageSaveName(frame_id);

    std::cout << "Saving file " << saveName << std::endl;

    labeled_image.save(QString::fromStdString(folder + saveName));
}

void Whisker_Widget::_saveFaceMaskButton() {

    auto const media_data = _data_manager->getMediaData();

    auto const width = media_data->getWidth();
    auto const height = media_data->getHeight();

    auto const frame_id = _data_manager->getTime()->getLastLoadedFrame();

    auto mask = media_data->getRawData(frame_id);

    auto m2 = convert_vector_to_mat(mask, width, height);

    cv::medianBlur(m2,m2,35);

    m2.reshape(1,width*height);

    mask.assign(m2.data, m2.data + m2.total() *m2.channels());

    auto mask_image = QImage(&mask[0],
                                 width,
                                 height,
                                  QImage::Format_Grayscale8
                                 );

    std::string saveName = "mask" + pad_frame_id(frame_id, 7) + ".png";
    std::cout << "Saving file " << saveName << std::endl;

    mask_image.save(QString::fromStdString(saveName));
}

void Whisker_Widget::_saveWhiskerAsCSV(const std::string& folder, const std::vector<Point2D>& whisker)
{
    auto const frame_id = _data_manager->getTime()->getLastLoadedFrame();

    auto saveName = _getWhiskerSaveName(frame_id);

    save_line_as_csv(whisker, folder + saveName);
}

std::string Whisker_Widget::_getWhiskerSaveName(int const frame_id) {

    if (_save_by_frame_name) {
        auto frame_string = _data_manager->getMediaData()->GetFrameID(frame_id);
        frame_string = remove_extension(frame_string);

        //Strip off the img prefix and leave the number


        return frame_string + ".csv";
    } else {

        std::string saveName = pad_frame_id(frame_id, 7) + ".csv";
        return saveName;
    }
}


void Whisker_Widget::_exportImageCSV()
{

    std::string folder = "./images/";

    std::filesystem::create_directory(folder);
    _saveImage(folder);

    auto current_time = _data_manager->getTime()->getLastLoadedFrame();

    for (int i = 0; i<_num_whisker_to_track; i++)
    {
        std::string whisker_name = "whisker_" + std::to_string(i + 1);

        auto whiskers = _data_manager->getLine(whisker_name)->getLinesAtTime(current_time);

        std::string folder = "./" + std::to_string(i) + "/";
        std::filesystem::create_directory(folder);

        _saveWhiskerAsCSV(folder, whiskers[0]);
    }
}

std::string Whisker_Widget::_getImageSaveName(int const frame_id)
{
    if (_save_by_frame_name)
    {
        auto saveName = _data_manager->getMediaData()->GetFrameID(frame_id);
        return saveName;
    } else {

        std::string saveName = "img" + pad_frame_id(frame_id, 7) + ".png";
        return saveName;
    }
}

/////////////////////////////////////////////

void Whisker_Widget::_createNewWhisker(std::string const & whisker_name, const int whisker_id)
{
    if (!_data_manager->getLine(whisker_name)) {
        std::cout << "Creating " << whisker_name << std::endl;
        _data_manager->createLine(whisker_name);
        _scene->addLineDataToScene(whisker_name);
        _scene->addLineColor(whisker_name,whisker_colors[whisker_id]);
    }
}

/**
 *
 *
 *
 * @brief Whisker_Widget::_addWhiskersToData
 */
void Whisker_Widget::_addWhiskersToData() {

    auto current_time = _data_manager->getTime()->getLastLoadedFrame();
    _data_manager->getLine("unlabeled_whiskers")->clearLinesAtTime(current_time);

    for (auto &w: _wt->whiskers) {
        _data_manager->getLine("unlabeled_whiskers")->addLineAtTime(current_time, w.x, w.y);
    }

    if (_num_whisker_to_track > 0) {
        _orderWhiskersByPosition();
    }
}

void Whisker_Widget::_drawWhiskers() {
    _scene->UpdateCanvas();
}

void Whisker_Widget::_clickedInVideo(qreal x_canvas, qreal y_canvas) {

    float x_media = x_canvas / _scene->getXAspect();
    float y_media = y_canvas / _scene->getYAspect();

    switch (_selection_mode) {
        case Whisker_Select: {
            std::tuple<float, int> nearest_whisker = _wt->get_nearest_whisker(x_media, y_media);
            if (std::get<0>(nearest_whisker) < 10.0f) {
                _selected_whisker = std::get<1>(nearest_whisker);
                _drawWhiskers();
            }
            break;
        }
        case Whisker_Pad_Select: {
            _wt->setWhiskerPad(x_media,y_media);
            std::string whisker_pad_label =
                    "(" + std::to_string(static_cast<int>(x_media)) + "," + std::to_string(static_cast<int>(y_media)) +
                    ")";
            ui->whisker_pad_pos_label->setText(QString::fromStdString(whisker_pad_label));
            _selection_mode = Whisker_Select;
            break;
        }
        default:
            break;
    }
    _contact_widget->setPolePos(x_media,y_media); // Pass forward to contact widget
}

void Whisker_Widget::_loadJaneliaWhiskers() {
    auto janelia_name = QFileDialog::getOpenFileName(
            this,
            "Load Whisker File",
            QDir::currentPath(),
            "All files (*.*) ;; whisker file (*.whiskers)");

    if (janelia_name.isNull()) {
        return;
    }

    auto whiskers_from_janelia = _wt->load_janelia_whiskers(janelia_name.toStdString());

    for (auto &[time, whiskers_in_frame]: whiskers_from_janelia) {
        for (auto &w: whiskers_in_frame) {
            _data_manager->getLine("unlabeled_whiskers")->addLineAtTime(time, w.x, w.y);
        }
    }
}

void Whisker_Widget::_loadHDF5Whiskers()
{
    auto filename = QFileDialog::getOpenFileName(
        this,
        "Load Whisker File",
        QDir::currentPath(),
        "All files (*.*) ;; whisker file (*.whiskers)");

    if (filename.isNull()) {
        return;
    }

    auto frames =  _data_manager->read_array_hdf5(filename.toStdString(), "frames");
    auto probs = _data_manager->read_ragged_hdf5(filename.toStdString(), "probs");
    auto x_coords = _data_manager->read_ragged_hdf5(filename.toStdString(), "heights");
    auto y_coords = _data_manager->read_ragged_hdf5(filename.toStdString(), "widths");

    auto mask_num = _data_manager->getMaskKeys().size();

    auto mask_key = "Whisker_Mask" + std::to_string(mask_num);

    _data_manager->createMask(mask_key);

    auto mask = _data_manager->getMask(mask_key);

    for (int i = 0; i < frames.size(); i ++) {
        mask->addMaskAtTime(frames[i], x_coords[i], y_coords[i]);
    }

    _scene->addMaskDataToScene(mask_key);
    _scene->addMaskColor(mask_key, whisker_colors[mask_num]);
}

void Whisker_Widget::_loadCSVWhiskerFromDir(std::string const & dir_name)
{
    auto dir_path = std::filesystem::path(dir_name);
    auto const whisker_number = std::stoi(dir_path.filename().string());

    std::string const whisker_name = "whisker_" + std::to_string(whisker_number);

    _createNewWhisker(whisker_name, whisker_number);

    int whisker_count = 0;
    for (const auto & entry : std::filesystem::directory_iterator(dir_name))
    {
        auto const frame_num = remove_extension(entry.path().filename().string());
        auto whisker = load_line_from_csv(entry.path().string());
        _data_manager->getLine(whisker_name)->addLineAtTime(std::stoi(frame_num), whisker);
        whisker_count++;
    }

    std::cout << "Loaded " << whisker_count << " whiskers" << std::endl;
}

void Whisker_Widget::_loadSingleCSVWhisker()
{
    auto const dir_name =  QFileDialog::getExistingDirectory(
        this,
        "Load Whisker CSV Files",
        QDir::currentPath()).toStdString();

    if (dir_name.empty()) {
        return;
    }

    _loadCSVWhiskerFromDir(dir_name);
}

void Whisker_Widget::_loadMultiCSVWhiskers()
{
    auto const dir_name =  QFileDialog::getExistingDirectory(
                              this,
                              "Load Whisker CSV Files",
                              QDir::currentPath()).toStdString();

    if (dir_name.empty()) {
        return;
    }

    for (const auto & entry : std::filesystem::directory_iterator(dir_name))
    {
        if (entry.is_directory())
        {
            _loadCSVWhiskerFromDir(entry.path().string());
        }
    }

}


/**
 * @brief Whisker_Widget::_orderWhiskersByPosition
 *
 * (0,0) coordinate is the top left of the video. Here we arrange the whiskers
 * such that the most posterior whisker is given identity of 1, next most posterior is
 * 2, etc.
 *
 */
void Whisker_Widget::_orderWhiskersByPosition() {
    auto base_positions = _getWhiskerBasePositions();

    std::vector<int> base_position_order = std::vector<int>(base_positions.size());
    std::iota(base_position_order.begin(), base_position_order.end(), 0);

    if (_face_orientation == Facing_Top) {
        // Facing toward 0 y, so larger Y is more posterior
        std::sort(std::begin(base_position_order),
                  std::end(base_position_order),
                  [&](int i1, int i2) { return base_positions[i1].y > base_positions[i2].y; });
    } else if (_face_orientation == Facing_Bottom) {
        // Facing toward maximum y, so smaller y is more posterior
        std::sort(std::begin(base_position_order),
                  std::end(base_position_order),
                  [&](int i1, int i2) { return base_positions[i1].y < base_positions[i2].y; });
    } else if (_face_orientation == Facing_Left) {
        // Facing toward 0 x, so larger x is more posterior
        std::sort(std::begin(base_position_order),
                  std::end(base_position_order),
                  [&](int i1, int i2) { return base_positions[i1].x > base_positions[i2].x; });
    } else {
        // Facing toward maximum x, so smaller x is more posterior
        std::sort(std::begin(base_position_order),
                  std::end(base_position_order),
                  [&](int i1, int i2) { return base_positions[i1].x < base_positions[i2].x; });
    }

    const auto current_time = _data_manager->getTime()->getLastLoadedFrame();
    auto whiskers = _data_manager->getLine("unlabeled_whiskers")->getLinesAtTime(current_time);

    for (int i = 0; i < _num_whisker_to_track; i++) {

        if (i >= base_position_order.size()) {
            break;
        }

        std::cout << "The " << i << " position whisker is " << base_position_order[i];
        std::cout << " with follicle at " << "(" << base_positions[base_position_order[i]].x << ","
                  << base_positions[base_position_order[i]].y << ")" << std::endl;

        std::string whisker_name = "whisker_" + std::to_string(i);

        _data_manager->getLine(whisker_name)->addLineAtTime(current_time, whiskers[base_position_order[i]]);
    }
}

std::vector<Point2D> Whisker_Widget::_getWhiskerBasePositions() {
    auto base_positions = std::vector<Point2D>{};

    const auto current_time = _data_manager->getTime()->getLastLoadedFrame();

    const auto whiskers = _data_manager->getLine("unlabeled_whiskers")->getLinesAtTime(current_time);

    for (auto &whisker: whiskers) {
        base_positions.push_back(whisker[0]);
    }

    //_printBasePositionOrder(base_positions);

    return base_positions;
}




void Whisker_Widget::_openJaneliaConfig()
{
    _janelia_config_widget->openWidget();
}

void Whisker_Widget::_openContactWidget()
{
    _contact_widget->openWidget();
}

void Whisker_Widget::LoadFrame(int frame_id)
{
    _contact_widget->updateFrame(frame_id);
}

void Whisker_Widget::_setMaskAlpha(int alpha)
{
    _scene->setMaskAlpha(alpha);
}

void Whisker_Widget::_skipToTrackedFrame(int index)
{
    if (_data_manager->getLine("whisker_0")) {
        std::vector<int> tracked_frames = _data_manager->getLine("whisker_0")->getTimesWithLines();
        auto frame_id = tracked_frames[index];
        _time_scrollbar->changeScrollBarValue(frame_id);
    }
}

void Whisker_Widget::_calculateMask()
{
    auto const frame_id = _data_manager->getTime()->getLastLoadedFrame();

    auto const media_data = _data_manager->getMediaData();

    auto mask = media_data->getRawData(frame_id);

    auto m2 = convert_vector_to_mat(mask, media_data->getWidth(), media_data->getHeight());

    cv::medianBlur(m2,m2,35);

    cv::Mat output = cv::Mat::zeros(m2.rows, m2.cols, CV_8UC1);

    auto const wp = _wt->getWhiskerPad();

    cv::circle(output,cv::Point(
                           static_cast<int>(std::get<0>(wp)),
                                 static_cast<int>(std::get<1>(wp))
                        ),10.0, cv::Scalar(255,255,255));

    //cv::grabCut(m2,output,)


}

/////////////////////////////////////////////

void _printBasePositionOrder(const std::vector<Point2D> &base_positions) {
    std::cout << "The order of whisker base positions: " << std::endl;

    for (int i = 0; i < base_positions.size(); i++) {
        std::cout << "Whisker " << i << " at " << "(" << base_positions[i].x << "," << base_positions[i].y << ")"
                  << std::endl;
    }
}

//https://stackoverflow.com/questions/6417817/easy-way-to-remove-extension-from-a-filename
std::string remove_extension(const std::string& filename) {
    const size_t lastdot = filename.find_last_of(".");
    if (lastdot == std::string::npos) return filename;
    return filename.substr(0, lastdot);
}

std::string pad_frame_id(int frame_id, int pad_digits)
{
    std::stringstream ss;
    ss << std::setw(pad_digits) << std::setfill('0') << frame_id;

    return ss.str();
}
