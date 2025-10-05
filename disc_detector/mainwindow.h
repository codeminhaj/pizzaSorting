#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <opencv2/opencv.hpp>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void processFrame();

private:
    Ui::MainWindow *ui;
    cv::VideoCapture cap_;
    QTimer timer_;

    QString classifyHSV(const cv::Vec3b& hsv);
    QImage  matToQImage(const cv::Mat& bgr);
};

// ---- Constants ----
static constexpr int   kBlurKsize   = 9;
static constexpr double kDp          = 1.5;
static constexpr double kMinDist     = 40.0;
static constexpr double kCannyHi     = 120.0;
static constexpr double kAccum       = 30.0;
static constexpr int    kRmin        = 15;
static constexpr int    kRmax        = 80;
static constexpr int    kEdgeMargin  = 10;

// Conversion / overlay constants (optional)
static constexpr float  kMMPerPixel  = 0.0f;  // set later if you calibrate
static constexpr int    kHeightMM    = 3;

#endif // MAINWINDOW_H
