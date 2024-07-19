#include "mainwindow.hpp"
#include "ui_mainwindow.h"

#include "analog_viewer.hpp"
#include "DataManager.hpp"
#include "Image_Processing_Widget/Image_Processing_Widget.hpp"
#include "Label_Widget.hpp"
#include "Media_Window.hpp"
#include "Tongue_Widget/Tongue_Widget.hpp"
#include "Whisker_Widget.hpp"

#include <QFileDialog>
#include <QImage>
#include <QKeyEvent>

#include <QTimer>
#include <QElapsedTimer>

#include <filesystem>
#include <iostream>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow),
    _data_manager{std::make_shared<DataManager>()}

{
    ui->setupUi(this);

    _m_DockManager = new ads::CDockManager(this);

    //This is necessary to accept keyboard events
    this->setFocusPolicy(Qt::StrongFocus);

    _scene = new Media_Window(_data_manager, this);

    _verbose = false;

    ui->time_scrollbar->setDataManager(_data_manager);

    _updateMedia();

    _createActions(); // Creates callback functions

    QWidget::grabKeyboard();

    registerDockWidget("media", ui->graphicsView, ads::TopDockWidgetArea);
    registerDockWidget("scrollbar", ui->time_scrollbar, ads::BottomDockWidgetArea);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::_createActions()
{
    connect(ui->actionLoad_Video, &QAction::triggered, this, &MainWindow::Load_Video);

    connect(ui->actionLoad_Images, &QAction::triggered,this, &MainWindow::Load_Images);

    connect(ui->actionLoad_Time_Series_CSV, &QAction::triggered, this, &MainWindow::_loadTimeSeriesCSV);

    connect(ui->time_scrollbar, &TimeScrollBar::timeChanged, _scene, &Media_Window::LoadFrame);

    connect(ui->actionWhisker_Tracking, &QAction::triggered, this, &MainWindow::openWhiskerTracking);
    connect(ui->actionLabel_Maker, &QAction::triggered, this, &MainWindow::openLabelMaker);
    connect(ui->actionAnalog_Viewer, &QAction::triggered, this, &MainWindow::openAnalogViewer);
    connect(ui->actionImage_Processing, &QAction::triggered, this, &MainWindow::openImageProcessing);
    connect(ui->actionTongue_Tracking, &QAction::triggered, this, &MainWindow::openTongueTracking);

    connect(ui->time_scrollbar, &TimeScrollBar::timeChanged, _scene, &Media_Window::LoadFrame);
}

/*
The Load_Video callback is executed whenever a the load_video option is selected.
If a video is selected, that video will be loaded and the first frame will be
drawn on the video screen.

*/
void MainWindow::Load_Video()
{
    auto vid_name =  QFileDialog::getOpenFileName(
                this,
                "Load Video File",
                QDir::currentPath(),
                "All files (*.*) ;; MP4 (*.mp4)");

    if (vid_name.isNull()) {
        return;
    }

    // Create video data object
    // Pass video data object to scene?

    _data_manager->createMedia(DataManager::Video);

    _updateMedia();

    _LoadData(vid_name.toStdString());

}

void MainWindow::Load_Images() {
    auto dir_name =  QFileDialog::getExistingDirectory(
        this,
        "Load Video File",
        QDir::currentPath());

    if (dir_name.isNull()) {
        return;
    }

    _data_manager->createMedia(DataManager::Images);

    _updateMedia();

    _LoadData(dir_name.toStdString());

}

void MainWindow::_loadTimeSeriesCSV()
{
    auto filename =  QFileDialog::getOpenFileName(
        this,
        "Load Video File",
        QDir::currentPath(),
        "All files (*.*) ;; CSV (*.csv)");

    if (filename.isNull()) {
        return;
    }

    auto series = load_series_from_csv(filename.toStdString());

    auto path = std::filesystem::path(filename.toStdString());
    auto key = path.filename().replace_extension("").string();

    _data_manager->createAnalogTimeSeries(key);

    _data_manager->getAnalogTimeSeries(key)->setData(series);

    std::cout << "Loaded series " << key << " with " <<
        _data_manager->getAnalogTimeSeries(key)->getAnalogTimeSeries().size() << " points " << std::endl;

    if (_widgets.find("analog_viewer") != _widgets.end()) {
        dynamic_cast<Analog_Viewer*>(_widgets["analog_viewer"].get())->plotLine(key);
    }
}

void MainWindow::_LoadData(std::string filepath) {

    _data_manager->getMediaData()->LoadMedia(filepath);
    auto frame_count = _data_manager->getMediaData()->getTotalFrameCount() - 1;

    _data_manager->getTime()->updateTotalFrameCount(frame_count);

    ui->time_scrollbar->updateScrollBarNewMax(_data_manager->getTime()->getTotalFrameCount());

    ui->time_scrollbar->changeScrollBarValue(0);

}

//If we load new media, we need to update the references to it. Widgets that use that media need to be updated to it.
//these include whisker_widget and label_widget
//There are probably better ways to do this.
void MainWindow::_updateMedia() {

    ui->graphicsView->setScene(_scene);
    ui->graphicsView->show();

}

void MainWindow::openWhiskerTracking() {
    std::string const key = "whisker_widget";

    if (_widgets.find(key) == _widgets.end()) {
        auto whiskerWidget = std::make_unique<Whisker_Widget>(
                _scene,
                _data_manager,
                ui->time_scrollbar,
                this);
        connect(ui->time_scrollbar, &TimeScrollBar::timeChanged, whiskerWidget.get(), &Whisker_Widget::LoadFrame);

        _widgets[key] = std::move(whiskerWidget);
        registerDockWidget(key, _widgets[key].get(), ads::RightDockWidgetArea);
    }

    dynamic_cast<Whisker_Widget*>(_widgets[key].get())->openWidget();
    showDockWidget(key);
}

void MainWindow::registerDockWidget(std::string const & key, QWidget* widget, ads::DockWidgetArea area)
{
    auto dock_widget = new ads::CDockWidget(QString::fromStdString(key));
    dock_widget->setWidget(widget);
    _m_DockManager->addDockWidget(area, dock_widget);
}

void MainWindow::showDockWidget(std::string const & key)
{
    _m_DockManager->findDockWidget(QString::fromStdString(key))->toggleView();
}

void MainWindow::openLabelMaker() {

    std::string const key = "label_maker";

    if (_widgets.find(key) == _widgets.end()) {
        auto labelMaker = std::make_unique<Label_Widget>(_scene, _data_manager);
        labelMaker->setObjectName(key);
        registerDockWidget(key, labelMaker.get(), ads::RightDockWidgetArea);
        _widgets[key] = std::move(labelMaker);
    }

    auto ptr = dynamic_cast<Label_Widget*>(_widgets[key].get());
    ptr->openWidget();

    showDockWidget(key);
}

void MainWindow::openAnalogViewer()
{
    std::string const key = "analog_viewer";

    if (_widgets.find(key) == _widgets.end()) {
        auto analogViewer = std::make_unique<Analog_Viewer>(_scene, _data_manager, ui->time_scrollbar, this);
        analogViewer->setObjectName(key);
        registerDockWidget(key, analogViewer.get(), ads::RightDockWidgetArea);
        _widgets[key] = std::move(analogViewer);

        connect(ui->time_scrollbar, &TimeScrollBar::timeChanged, dynamic_cast<Analog_Viewer*>(_widgets[key].get()), &Analog_Viewer::SetFrame);
    }

    auto ptr = dynamic_cast<Analog_Viewer*>(_widgets[key].get());
    ptr->openWidget();

    showDockWidget(key);
}

void MainWindow::openImageProcessing()
{
    std::string const key = "image_processing";

    if (_widgets.find(key) == _widgets.end()) {
        auto imageProcessing = std::make_unique<Image_Processing_Widget>(_scene, _data_manager);
        imageProcessing->setObjectName(key);
        registerDockWidget(key, imageProcessing.get(), ads::RightDockWidgetArea);
        _widgets[key] = std::move(imageProcessing);
    }

    auto ptr = dynamic_cast<Image_Processing_Widget*>(_widgets[key].get());
    ptr->openWidget();

    showDockWidget(key);
}

void MainWindow::openTongueTracking()
{
    std::string const key = "tongue_widget";

    if (_widgets.find(key) == _widgets.end()) {
        auto tongueWidget = std::make_unique<Tongue_Widget>(_scene, _data_manager, ui->time_scrollbar);
        tongueWidget->setObjectName(key);
        registerDockWidget(key, tongueWidget.get(), ads::RightDockWidgetArea);
        _widgets[key] = std::move(tongueWidget);
    }

    auto ptr = dynamic_cast<Tongue_Widget*>(_widgets[key].get());
    ptr->openWidget();

    showDockWidget(key);
}

void MainWindow::keyPressEvent(QKeyEvent *event) {

    if (event->key() == Qt::Key_Right) {
        ui->time_scrollbar->changeScrollBarValue(1,true);
    } else if (event->key() == Qt::Key_Left){
        ui->time_scrollbar->changeScrollBarValue(-1,true);
    } else {

        auto ww = QApplication::focusWidget();
        if (!this->isActiveWindow()) {
            std::cout << "Sending event to another window" << std::endl;
            QApplication::sendEvent(ww, event);
        } else {
            std::cout << "Focus widget: " << ww->objectName().toStdString() << std::endl;
            QApplication::sendEvent(ww, event);
        }
    }
}

