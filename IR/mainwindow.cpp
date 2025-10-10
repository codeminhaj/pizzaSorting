#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <algorithm>

// ---- helpers ----
static inline int bucketPercentFromCount(int c) {
    if (c <= 0)  return 0;
    if (c >= 40) return 100;
    int bucket = c / 8;                 // 8 counts per 20% step
    if (bucket > 5) bucket = 5;
    return bucket * 20;                 // 0,20,40,60,80,100
}

static inline bool debounceOk(uint32_t prevTick, uint32_t currTick, uint32_t us = 3000) {
    return static_cast<uint32_t>(currTick - prevTick) > us; // pigpio tick = 32-bit µs
}

// ---- MainWindow ----
MainWindow::MainWindow(QWidget *parent) noexcept
    : QMainWindow(parent),
      ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // init UI
    ui->progressA->setRange(0, 100);
    ui->progressB->setRange(0, 100);
    ui->progressA->setValue(0);
    ui->progressB->setValue(0);
    ui->labelA->setText("Sensor A: 0 / 40 (0%)");
    ui->labelB->setText("Sensor B: 0 / 40 (0%)");
    ui->labelHint->setText(
        "Wiring (BCM):\n"
        "• Sensor A → GPIO17 (pin 11), 3.3V, GND\n"
        "• Sensor B → GPIO27 (pin 13), 3.3V, GND\n"
        "Notes: 3.3 V logic only. Rising edge = +1 (debounce 3 ms). Max 40 counts.\n"
        "Running via pigpio daemon (no sudo for this app)."
    );

    // queued connection (callbacks come from pigpio thread)
    connect(this, &MainWindow::sensorStepChanged,
            this, &MainWindow::onSensorStepChanged,
            Qt::QueuedConnection);

    setupGpio(gpioA_, gpioB_);
}

MainWindow::~MainWindow() {
    teardownGpio();
    delete ui;
}

void MainWindow::setupGpio(int gpioA, int gpioB) {
    // connect to local pigpio daemon (localhost:8888)
    pi_ = pigpio_start(nullptr, nullptr);
    if (pi_ < 0) {
        ui->labelA->setText("Sensor A: pigpio init FAILED");
        ui->labelB->setText("Sensor B: pigpio init FAILED");
        return;
    }

    set_mode(pi_, gpioA, PI_INPUT);
    set_pull_up_down(pi_, gpioA, PI_PUD_DOWN);
    set_mode(pi_, gpioB, PI_INPUT);
    set_pull_up_down(pi_, gpioB, PI_PUD_DOWN);

    // rising-edge callbacks
    cbIdA_ = callback_ex(pi_, gpioA, RISING_EDGE, &MainWindow::cbThunkA, this);
    cbIdB_ = callback_ex(pi_, gpioB, RISING_EDGE, &MainWindow::cbThunkB, this);
}

void MainWindow::teardownGpio() {
    if (pi_ >= 0) {
        if (cbIdA_ >= 0) callback_cancel(cbIdA_);
        if (cbIdB_ >= 0) callback_cancel(cbIdB_);
        pigpio_stop(pi_);
        pi_ = -1;
    }
}

// static thunks
void MainWindow::cbThunkA(int /*pi*/, unsigned /*user_gpio*/, unsigned level, uint32_t tick, void *userdata) {
    if (level != 1) return; // only rising
    auto *self = static_cast<MainWindow*>(userdata);
    if (self) self->handleRising(0, tick);
}
void MainWindow::cbThunkB(int /*pi*/, unsigned /*user_gpio*/, unsigned level, uint32_t tick, void *userdata) {
    if (level != 1) return;
    auto *self = static_cast<MainWindow*>(userdata);
    if (self) self->handleRising(1, tick);
}

void MainWindow::handleRising(int sensorIndex, uint32_t tick) {
    constexpr uint32_t DB_US = 3000;

    if (sensorIndex == 0) {
        uint32_t last = lastTickA_.load(std::memory_order_relaxed);
        if (!debounceOk(last, tick, DB_US)) return;
        lastTickA_.store(tick, std::memory_order_relaxed);

        int c = countA_.load(std::memory_order_relaxed);
        if (c >= 40) return;

        c = countA_.fetch_add(1, std::memory_order_relaxed) + 1;  // +1 each edge
        int step = bucketPercentFromCount(c);
        stepA_.store(step, std::memory_order_relaxed);
        emit sensorStepChanged(0, step, c);

    } else {
        uint32_t last = lastTickB_.load(std::memory_order_relaxed);
        if (!debounceOk(last, tick, DB_US)) return;
        lastTickB_.store(tick, std::memory_order_relaxed);

        int c = countB_.load(std::memory_order_relaxed);
        if (c >= 40) return;

        c = countB_.fetch_add(1, std::memory_order_relaxed) + 1;
        int step = bucketPercentFromCount(c);
        stepB_.store(step, std::memory_order_relaxed);
        emit sensorStepChanged(1, step, c);
    }
}

void MainWindow::onSensorStepChanged(int sensorIndex, int percentStep, int rawCount) {
    int clamped = std::min(rawCount, 40);

    if (sensorIndex == 0) {
        ui->progressA->setValue(percentStep);
        ui->labelA->setText(QString("Sensor A: %1 / 40 (%2%)").arg(clamped).arg(percentStep));
    } else {
        ui->progressB->setValue(percentStep);
        ui->labelB->setText(QString("Sensor B: %1 / 40 (%2%)").arg(clamped).arg(percentStep));
    }
}

void MainWindow::on_btnReset_clicked() {
    resetAll();
}

void MainWindow::resetAll() {
    // reset state
    countA_.store(0, std::memory_order_relaxed);
    countB_.store(0, std::memory_order_relaxed);
    lastTickA_.store(0, std::memory_order_relaxed);
    lastTickB_.store(0, std::memory_order_relaxed);
    stepA_.store(0, std::memory_order_relaxed);
    stepB_.store(0, std::memory_order_relaxed);

    // refresh UI
    ui->progressA->setValue(0);
    ui->progressB->setValue(0);
    ui->labelA->setText("Sensor A: 0 / 40 (0%)");
    ui->labelB->setText("Sensor B: 0 / 40 (0%)");
}
