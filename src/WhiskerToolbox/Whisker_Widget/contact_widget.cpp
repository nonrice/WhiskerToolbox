
#include "contact_widget.hpp"

#include "ui_contact_widget.h"

#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QElapsedTimer>

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>

Contact_Widget::Contact_Widget(std::shared_ptr<DataManager> data_manager, TimeScrollBar* time_scrollbar, QWidget *parent) :
    QWidget(parent),
    _data_manager{data_manager},
    _contact_start{0},
    _contact_epoch(false),
    _contactEvents{std::vector<ContactEvent>()},
    _image_buffer_size{5},
    _pole_pos{std::make_tuple(250,250)},
    _pole_select_mode{false},
    _bounding_box_width{130},
    _time_scrollbar{time_scrollbar},
    ui(new Ui::contact_widget)
{
    ui->setupUi(this);

    _contact_imgs = std::vector<QImage>();

    for (int i = 0; i < _image_buffer_size; i++) {
        _contact_imgs.push_back(QImage(130,130,QImage::Format_Grayscale8));
    }

    _scene = new QGraphicsScene();
    _scene->setSceneRect(0, 0, 650, 150);

    ui->graphicsView->setScene(_scene);
    ui->graphicsView->show();

    ui->graphicsView->setTransformationAnchor(QGraphicsView::NoAnchor);

};

Contact_Widget::~Contact_Widget() {
    delete ui;
}

void Contact_Widget::openWidget() {

    connect(ui->contact_button, SIGNAL(clicked()), this, SLOT(_contactButton()));
    connect(ui->save_contact_button, SIGNAL(clicked()), this, SLOT(_saveContact()));
    connect(ui->load_contact_button, SIGNAL(clicked()), this, SLOT(_loadContact()));
    connect(ui->pole_select, SIGNAL(clicked()),this, SLOT(_poleSelectButton()));
    connect(ui->bounding_box_size, SIGNAL(valueChanged(int)),this,SLOT(_setBoundingBoxWidth(int)));

    connect(ui->contact_number, SIGNAL(valueChanged(int)),this,SLOT(_contactNumberSelect(int)));

    if (_contact.empty()) {
        _contact = std::vector<Contact>(_data_manager->getTime()->getTotalFrameCount());
    }

    this->show();

}

void Contact_Widget::closeEvent(QCloseEvent *event) {

    disconnect(ui->contact_button, SIGNAL(clicked()), this, SLOT(_contactButton()));
    disconnect(ui->save_contact_button, SIGNAL(clicked()), this, SLOT(_saveContact()));
    disconnect(ui->load_contact_button, SIGNAL(clicked()), this, SLOT(_loadContact()));
    disconnect(ui->pole_select, SIGNAL(clicked()),this, SLOT(_poleSelectButton()));
    disconnect(ui->bounding_box_size, SIGNAL(valueChanged(int)),this,SLOT(_setBoundingBoxWidth(int)));
    disconnect(ui->contact_number, SIGNAL(valueChanged(int)),this,SLOT(_contactNumberSelect(int)));

}

void Contact_Widget::setPolePos(float pole_x, float pole_y)
{
    if (_pole_select_mode) {
        _pole_pos = std::make_tuple(static_cast<int>(pole_x),static_cast<int>(pole_y));
        std::string new_label = "(" + std::to_string(static_cast<int>(pole_x)) + ", " + std::to_string(static_cast<int>(pole_y)) + ")";
        ui->current_location_label->setText(QString::fromStdString(new_label));
        _pole_select_mode = false;
        updateFrame(_data_manager->getTime()->getLastLoadedFrame());
    }
}

void Contact_Widget::_poleSelectButton()
{
    _pole_select_mode = true;
}

void Contact_Widget::_setBoundingBoxWidth(int value)
{
    _bounding_box_width = value;
    updateFrame(_data_manager->getTime()->getLastLoadedFrame());
}

void Contact_Widget::updateFrame(int frame_id)
{

    QElapsedTimer timer2;
    timer2.start();

    auto _media = _data_manager->getMediaData();

    float pole_x = std::get<0>(_pole_pos);
    float pole_y = std::get<1>(_pole_pos);

    for (int i = -2; i < 3; i++) {

        if (frame_id + i < 0) {
            continue;
        }

        auto media_data = _media->getRawData(frame_id + i);

        auto unscaled_image = QImage(&media_data[0],
                                 _media->getWidth(),
                                 _media->getHeight(),
                                 _getQImageFormat()
                                 );

        QRect rect(pole_x - _bounding_box_width/2, pole_y - _bounding_box_width/2, _bounding_box_width, _bounding_box_width);

        auto cropped_image = unscaled_image.copy(rect);

        _contact_imgs[i + 2] = cropped_image.scaled(130,130);

        auto pixmap = QPixmap::fromImage(_contact_imgs[i + 2]);
        QGraphicsPixmapItem* pixmap_item = new QGraphicsPixmapItem(pixmap);

        _scene->addItem(pixmap_item);
        pixmap_item->setTransform(QTransform().translate(130 * (i + 2),20),true);

        if (_contactEvents.size() == 0) { continue;}

        QPainterPath contact_rectangle;
        contact_rectangle.addRect(0,0,130,20);
        contact_rectangle.setFillRule(Qt::WindingFill);
        QGraphicsPathItem* rect_item = new QGraphicsPathItem();

        if (_contact[frame_id + i] == Contact::Contact) {
            rect_item = _scene->addPath(contact_rectangle,QPen(Qt::red),QBrush(Qt::red));
        } else {
            rect_item = _scene->addPath(contact_rectangle,QPen(Qt::green),QBrush(Qt::green));
        }
        rect_item->setTransform(QTransform().translate(130 * (i + 2),0),true);
    }

    int t1 = timer2.elapsed();

    qDebug() << "Drawing 5 frames took " << t1;
}

QImage::Format Contact_Widget::_getQImageFormat() {

    auto _media = _data_manager->getMediaData();
    switch(_media->getFormat())
    {
    case MediaData::DisplayFormat::Gray:
        return QImage::Format_Grayscale8;
    case MediaData::DisplayFormat::Color:
        return QImage::Format_RGBA8888;
    }
}

void Contact_Widget::_contactButton() {

    auto frame_num = _data_manager->getTime()->getLastLoadedFrame();

    // If we are in a contact epoch, we need to mark the termination frame and add those to block
    if (_contact_epoch) {

        _contact_epoch = false;

        ui->contact_button->setText("Mark Contact");

        for (int i = _contact_start; i < frame_num; i++) {
            _contact[i] = Contact::Contact;
        }

    } else {
        // If we are not already in contact epoch, start one
        _contact_start = frame_num;

        _contact_epoch = true;

        ui->contact_button->setText("Mark Contact End");
    }

    _calculateContactPeriods();
}

void Contact_Widget::_saveContact() {


    std::fstream fout;

    fout.open("contact.csv", std::fstream::out);

    for (auto &frame_contact: _contact) {
        if (frame_contact == Contact::Contact) {
            fout << "Contact" << "\n";
        } else {
            fout << "Nocontact" << "\n";
        }
    }

    fout.close();
}

void Contact_Widget::_loadContact() {

    auto contact_filename = QFileDialog::getOpenFileName(
        this,
        "Load Video File",
        QDir::currentPath(),
        "All files (*.*) ;; MP4 (*.mp4)");


    std::ifstream fin(contact_filename.toStdString());

    std::string line;

    int row_num = 0;

    while (std::getline(fin, line)) {
        if (line == "Contact") {
            _contact[row_num] = Contact::Contact;
        }
        row_num++;
    }

    fin.close();

    _calculateContactPeriods();
}

void Contact_Widget::_buildContactTable()
{
    ui->contact_table->setRowCount(0);
    for (int i=0; i < _contactEvents.size(); i++)
    {
        ui->contact_table->insertRow(ui->contact_table->rowCount());
        ui->contact_table->setItem(i,0,new QTableWidgetItem(QString::number(_contactEvents[i].start)));
        ui->contact_table->setItem(i,1,new QTableWidgetItem(QString::number(_contactEvents[i].end)));

    }
}

void Contact_Widget::_calculateContactPeriods()
{
    bool in_contact = false;
    int contact_start = 0;

    _contactEvents = std::vector<ContactEvent>();

    for (int i = 0; i < _contact.size(); i++)
    {
        if (in_contact)
        {
            if (_contact[i] == Contact::NoContact) {
                _contactEvents.push_back(ContactEvent{contact_start,i});
                in_contact = false;
            }
        } else {
            if (_contact[i] == Contact::Contact)
            {
                in_contact = true;
                contact_start = i;
            }
        }
    }

    std::cout << "There are " << _contactEvents.size() << " contact events" << std::endl;

    ui->total_contact_label->setText(QString::number(_contactEvents.size()));
    ui->contact_number->setMaximum(_contactEvents.size());

    _buildContactTable();
}

void Contact_Widget::_contactNumberSelect(int value)
{

    auto frame_id = _contactEvents[value].start;
    _time_scrollbar->changeScrollBarValue(frame_id);
}
