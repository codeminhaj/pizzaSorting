#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <algorithm>
#include <cmath>

// Tall-ish ROI is fine; we process full frame here. Adjust if you want.
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

    ui->statusbar->showMessage(
        QString("Valid = {green, blue} AND 20–30 mm Ø. Height %1 mm. %2")
        .arg(kHeightMM)
        .arg(kMMPerPixel > 0.0f ? "" : "Set kMMPerPixel in mainwindow.h!")
    );
}

MainWindow::~MainWindow() {
    timer_.stop();
    if (cap_.isOpened()) cap_.release();
    delete ui;
}

QString MainWindow::classifyHSV(const cv::Vec3b& hsv) {
    int h = hsv[0], s = hsv[1], v = hsv[2];
    if (v < 40 || s < 40) return "unknown";
    if (h < 10 || h > 170) return "red";
    if (h > 90 && h < 135) return "blue";
    if (h > 35 && h < 85)  return "green";
    return "unknown";
}

QImage MainWindow::matToQImage(const cv::Mat& bgr) {
    cv::Mat rgb; cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
    return QImage(rgb.data, rgb.cols, rgb.rows, int(rgb.step), QImage::Format_RGB888).copy();
}

void MainWindow::processFrame() {
    cv::Mat frame;
    if (!cap_.read(frame) || frame.empty()) return;

    // --- Preprocess ---
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(kBlurKsize, kBlurKsize), 0);

    // --- Detect circles ---
    std::vector<cv::Vec3f> circles;
    cv::HoughCircles(gray, circles, cv::HOUGH_GRADIENT, kDp, kMinDist, kCannyHi, kAccum, kRmin, kRmax);

    // Reject near edges
    std::vector<cv::Vec3f> filt;
    filt.reserve(circles.size());
    for (auto c : circles) {
        cv::Point2f p(c[0], c[1]); float r = c[2];
        if (p.x - r < kEdgeMargin || p.y - r < kEdgeMargin ||
            p.x + r > frame.cols - kEdgeMargin || p.y + r > frame.rows - kEdgeMargin) continue;
        filt.push_back(c);
    }

    // Classify + measure
    cv::Mat hsv; cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
    struct Disc { cv::Point2f p; float r; QString color; int d_mm; float d_px; bool valid; QString reason; };
    std::vector<Disc> discs;
    discs.reserve(filt.size());
    for (auto c : filt) {
        cv::Point pt(cvRound(c[0]), cvRound(c[1]));
        pt.x = std::clamp(pt.x, 0, hsv.cols - 1);
        pt.y = std::clamp(pt.y, 0, hsv.rows - 1);

        QString color = classifyHSV(hsv.at<cv::Vec3b>(pt));
        float d_px = 2.0f * c[2];

        int d_mm = -1;
        if (kMMPerPixel > 0.0f) {
            d_mm = int(std::lround(d_px * kMMPerPixel));   // nearest whole mm
        }

        // Rule check
        bool color_ok = kAllowedColors.contains(color);
        bool size_ok  = (kMMPerPixel > 0.0f) ? (d_mm >= kMinMM && d_mm <= kMaxMM) : true; // if not calibrated, skip size rule
        bool valid = color_ok && size_ok;

        QString reason;
        if (!valid) {
            if (!color_ok) reason = "color";
            else if (kMMPerPixel > 0.0f && !size_ok) reason = "size";
            else reason = "uncalibrated";
        }

        // Keep even invalid ones so you can see why they failed
        if (color != "unknown")
            discs.push_back({cv::Point2f(c[0], c[1]), c[2], color, d_mm, d_px, valid, reason});
    }

    // Sort vertical (top→bottom) just for consistent labeling
    std::sort(discs.begin(), discs.end(),
              [](const Disc& a, const Disc& b){ return a.p.y < b.p.y; });

    // Draw overlays
    int invalid_count = 0;
    for (const auto& d : discs) {
        cv::Scalar edge = d.valid ? cv::Scalar(0,255,0) : cv::Scalar(0,0,255); // green for valid, red for invalid
        cv::circle(frame, d.p, int(d.r), edge, 3);

        // Label above: color + size
        std::string sizeTxt;
        if (kMMPerPixel > 0.0f && d.d_mm >= 0) sizeTxt = cv::format("%d mm", d.d_mm);
        else sizeTxt = cv::format("%.0f px", d.d_px);

        std::string label = d.color.toStdString() + "  " + sizeTxt;
        cv::putText(frame, label, d.p + cv::Point2f(-40, -d.r - 12),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255,255,255), 2);

        // Reason tag if invalid
        if (!d.valid) {
            ++invalid_count;
            std::string tag = (d.reason=="color") ? "INVALID: color"
                             : (d.reason=="size")  ? "INVALID: size"
                             : "UNCALIBRATED";
            cv::putText(frame, tag, d.p + cv::Point2f(-60, +d.r + 18),
                        cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0,0,255), 2);
        }
    }

    // Verdict banner: if any detected disc violates rules -> "invalid"
    std::string verdict = (discs.empty() ? "waiting..." : (invalid_count ? "invalid" : "valid"));
    cv::Scalar vcol = (verdict == "valid") ? cv::Scalar(0,255,0) : cv::Scalar(0,0,255);
    int base=0; cv::Size vsz = cv::getTextSize(verdict, cv::FONT_HERSHEY_SIMPLEX, 0.9, 2, &base);
    cv::Point vorg(10, 30);
    cv::rectangle(frame, cv::Rect(vorg.x-6, vorg.y-24, vsz.width+12, vsz.height+12), cv::Scalar(0,0,0), cv::FILLED);
    cv::putText(frame, verdict, cv::Point(vorg.x, vorg.y), cv::FONT_HERSHEY_SIMPLEX, 0.9, vcol, 2);

    // Height info (fixed 3 mm)
    std::string htxt = "height: 3 mm";
    cv::putText(frame, htxt, cv::Point(10, 55), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(200,200,200), 1);

    ui->videoLabel->setPixmap(QPixmap::fromImage(matToQImage(frame)));
}
