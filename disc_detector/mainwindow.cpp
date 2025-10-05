#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <algorithm>
#include <cmath>

// ---- Pixel rule ----
static constexpr float kMinPx = 69.0f;
static constexpr float kMaxPx = 71.0f;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    if (!cap_.open(0)) {
        ui->statusbar->showMessage("Camera open failed");
        return;
    }

    cap_.set(cv::CAP_PROP_FRAME_WIDTH,  640);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    connect(&timer_, &QTimer::timeout, this, &MainWindow::processFrame);
    timer_.start(33);

    ui->statusbar->showMessage("Valid = {green, blue} AND 69–71 px Ø");
}

MainWindow::~MainWindow()
{
    timer_.stop();
    if (cap_.isOpened()) cap_.release();
    delete ui;
}

QString MainWindow::classifyHSV(const cv::Vec3b& hsv)
{
    int h = hsv[0], s = hsv[1], v = hsv[2];
    if (h < 40 || s < 40) return "unknown";
    if (h < 10 || h > 170) return "red";
    if (h > 90 && h < 135) return "blue";
    if (h > 35 && h < 85)  return "green";
    return "unknown";
}

QImage MainWindow::matToQImage(const cv::Mat& bgr)
{
    cv::Mat rgb;
    cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
    return QImage(rgb.data, rgb.cols, rgb.rows, int(rgb.step),
                  QImage::Format_RGB888).copy();
}

void MainWindow::processFrame()
{
    cv::Mat frame;
    if (!cap_.read(frame) || frame.empty()) return;

    // --- Preprocess ---
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(kBlurKsize, kBlurKsize), 0);

    // --- Detect circles ---
    std::vector<cv::Vec3f> circles;
    cv::HoughCircles(gray, circles, cv::HOUGH_GRADIENT, kDp, kMinDist,
                     kCannyHi, kAccum, kRmin, kRmax);

    // --- Reject near edges ---
    std::vector<cv::Vec3f> filt;
    for (auto c : circles) {
        cv::Point2f p(c[0], c[1]); float r = c[2];
        if (p.x - r < kEdgeMargin || p.y - r < kEdgeMargin ||
            p.x + r > frame.cols - kEdgeMargin ||
            p.y + r > frame.rows - kEdgeMargin)
            continue;
        filt.push_back(c);
    }

    // --- Classify & check rules ---
    cv::Mat hsv;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

    struct Disc {
        cv::Point2f p; float r; QString color;
        float d_px; bool valid; QString reason;
    };

    std::vector<Disc> discs;
    for (auto c : filt) {
        cv::Point pt(cvRound(c[0]), cvRound(c[1]));
        pt.x = std::clamp(pt.x, 0, hsv.cols - 1);
        pt.y = std::clamp(pt.y, 0, hsv.rows - 1);

        QString color = classifyHSV(hsv.at<cv::Vec3b>(pt));
        float d_px = 2.0f * c[2];

        bool color_ok = (color == "green" || color == "blue");
        bool size_ok  = (d_px >= kMinPx && d_px <= kMaxPx);
        bool valid    = color_ok && size_ok;

        QString reason;
        if (!valid) {
            if (!color_ok) reason = "color";
            else if (!size_ok) reason = "size_px";
        }

        if (color != "unknown")
            discs.push_back({cv::Point2f(c[0], c[1]), c[2],
                             color, d_px, valid, reason});
    }

    std::sort(discs.begin(), discs.end(),
              [](const Disc& a, const Disc& b){ return a.p.y < b.p.y; });

    int invalid_count = 0;
    for (const auto& d : discs) {
        cv::Scalar edge = d.valid ? cv::Scalar(0,255,0) : cv::Scalar(0,0,255);
        cv::circle(frame, d.p, int(d.r), edge, 3);

        std::string sizeTxt = cv::format("%.0f px", d.d_px);
        std::string label   = d.color.toStdString() + "  " + sizeTxt;
        cv::putText(frame, label, d.p + cv::Point2f(-40, -d.r - 12),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6,
                    cv::Scalar(255,255,255), 2);

        if (!d.valid) {
            ++invalid_count;
            std::string tag = (d.reason=="color")   ? "INVALID: color"
                             : (d.reason=="size_px") ? "INVALID: size(px)"
                             : "INVALID";
            cv::putText(frame, tag, d.p + cv::Point2f(-60, +d.r + 18),
                        cv::FONT_HERSHEY_SIMPLEX, 0.6,
                        cv::Scalar(0,0,255), 2);
        }
    }

    std::string verdict = (discs.empty() ? "waiting..." :
                          (invalid_count ? "invalid" : "valid"));
    cv::Scalar vcol = (verdict == "valid") ?
                      cv::Scalar(0,255,0) : cv::Scalar(0,0,255);
    int base=0;
    cv::Size vsz = cv::getTextSize(verdict,
                                   cv::FONT_HERSHEY_SIMPLEX, 0.9, 2, &base);
    cv::Point vorg(10, 30);
    cv::rectangle(frame, cv::Rect(vorg.x-6, vorg.y-24,
                                  vsz.width+12, vsz.height+12),
                  cv::Scalar(0,0,0), cv::FILLED);
    cv::putText(frame, verdict, cv::Point(vorg.x, vorg.y),
                cv::FONT_HERSHEY_SIMPLEX, 0.9, vcol, 2);

    ui->videoLabel->setPixmap(QPixmap::fromImage(matToQImage(frame)));
}
