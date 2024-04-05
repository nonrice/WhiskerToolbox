#ifndef WHISKER_WIDGET_H
#define WHISKER_WIDGET_H

#include <QWidget>

#include <memory>

#include "whiskertracker.h"
#include "Media_Window.h"

#include "ui_Whisker_Widget.h"

#include "TimeFrame.h"

/*

This is our interface to using the Janelia whisker tracker.


*/

enum class Contact : int {
    Contact = 1,
    NoContact = 0
};

class Whisker_Widget : public QWidget, private Ui::Whisker_Widget
{
    Q_OBJECT
public:
    Whisker_Widget(Media_Window* scene, std::shared_ptr<TimeFrame> time, QWidget *parent = 0) : QWidget(parent) {
        setupUi(this);

        _scene = scene;

        _time = time;

        _createActions();

        _wt = std::make_unique<WhiskerTracker>();
        _selected_whisker = 0;
        _selection_mode = Whisker_Select;
        //connect(ui->trace_button,SIGNAL(clicked()),this,SLOT(openConfig()));

    };

    void openWidget(); // Call
protected:
    void closeEvent(QCloseEvent *event);

private:
    void _createActions();
    void _openActions();
    void _closeActions();
    void _DrawWhiskers();

    std::unique_ptr<WhiskerTracker> _wt;
    Media_Window * _scene;
    std::shared_ptr<TimeFrame> _time;
    int _selected_whisker;
    enum Selection_Type {Whisker_Select,
                          Whisker_Pad_Select};
    Whisker_Widget::Selection_Type _selection_mode;

    std::vector<Contact> _contact;
    int _contact_start;
    bool _contact_epoch;

private slots:
    void _TraceButton();
    void _SaveImageButton();
    void _SaveWhiskerMaskButton();

    void _ContactButton();
    void _SaveContact();
    void _LoadContact();

    void _ClickedInVideo(qreal x,qreal y);
    /*
    void openConfig() {
        config_win->updateValues();
        config_win->show();
    };
    void updateValues() {
        std::cout << "Y Max: " << c_opt->y_max << std::endl;
        std::cout << "Y Min: " << c_opt->y_min << std::endl;

        //Replot
    };
*/
};

#endif // WHISKER_WIDGET_H
