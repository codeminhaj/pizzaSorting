#pragma once
#include <QMainWindow>
#include <QTimer>
#include <opencv2/opencv.hpp>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void processFrame();

private:
    Ui::MainWindow *ui;
    QTimer timer_;
    cv::VideoCapture cap_;

    // --- Detection params ---
    static constexpr int   kBlurKsize  = 9;
    static constexpr float kDp         = 1.2f;
    static constexpr float kMinDist    = 35.0f;
    static constexpr int   kCannyHi    = 120;
    static constexpr int   kAccum      = 30;
    static constexpr int   kRmin       = 10;
    static constexpr int   kRmax       = 80;
    static constexpr int   kEdgeMargin = 12;

    // --- Business rules ---
    // Set your calibration: millimetres per pixel (mm/px). Example: 0.12f
    static constexpr float kMMPerPixel = 0.0f;   // <-- SET THIS!
    static constexpr int   kMinMM      = 20;     // inclusive
    static constexpr int   kMaxMM      = 30;     // inclusive
    const QStringList kAllowedColors  = {"green", "blue"};
    static constexpr int   kHeightMM   = 3;      // fixed height (display only)

    // Helpers
    static QString classifyHSV(const cv::Vec3b& hsv);
    static QImage  matToQImage(const cv::Mat& bgr);
};
