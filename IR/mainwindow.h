#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <atomic>
#include <cstdint>

extern "C" {
#include <pigpiod_if2.h>  // pigpio daemon client API
}

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr) noexcept;
    ~MainWindow() override;

signals:
    // Emitted on every increment (percentStep is 0/20/40/60/80/100)
    void sensorStepChanged(int sensorIndex, int percentStep, int rawCount);

private slots:
    void onSensorStepChanged(int sensorIndex, int percentStep, int rawCount);
    void on_btnReset_clicked();

private:
    void setupGpio(int gpioA, int gpioB);
    void teardownGpio();

    // pigpio callback thunks
    static void cbThunkA(int pi, unsigned user_gpio, unsigned level, uint32_t tick, void *userdata);
    static void cbThunkB(int pi, unsigned user_gpio, unsigned level, uint32_t tick, void *userdata);

    void handleRising(int sensorIndex, uint32_t tick);
    void resetAll();

private:
    Ui::MainWindow *ui{nullptr};

    // pigpio daemon handle
    int pi_{-1};

    // GPIOs (BCM numbering)
    int gpioA_{17}; // pin 11
    int gpioB_{27}; // pin 13

    // callback IDs
    int cbIdA_{-1};
    int cbIdB_{-1};

    // counts/debounce
    std::atomic<int> countA_{0};
    std::atomic<int> countB_{0};
    std::atomic<uint32_t> lastTickA_{0};
    std::atomic<uint32_t> lastTickB_{0};

    // last published step (0..100 in 20% increments)
    std::atomic<int> stepA_{0};
    std::atomic<int> stepB_{0};
};

#endif // MAINWINDOW_H
