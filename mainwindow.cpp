#include "mainwindow.h"
#include "ui_mainwindow.h"

#ifndef count_of
#define count_of(array) (int)(sizeof (array) / sizeof (array[0]))
#endif

static inline void delay(unsigned long sec) {
    while (sec >= 1000) {
        usleep(1000 * 1000);
        sec -= 1000;
    };
    if (sec != 0)
        usleep(sec * 1000);
}

MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);

    connect(ui->start_lidar, &QPushButton::clicked, this, &MainWindow::start_lidar);
    connect(ui->end_lidar, &QPushButton::clicked, this, &MainWindow::end_lidar);

    lidar = *sl::createLidarDriver();
    if (!lidar) {
        std::cerr << "Insufficent memory, exit" << std::endl;
        exit(-1);
    }

    sl_lidar_response_device_health_t health_info;
    sl_lidar_response_device_info_t dev_info;

        sl::IChannel *channel = *sl::createSerialPortChannel("/dev/ttyUSB0", 460800);

        if (SL_IS_FAIL(lidar->connect(channel)))
            std::cerr << "Error, cannot bind to the specified serial port /dev/ttyUSB0" << std::endl;

        sl_result operat_result = lidar->getDeviceInfo(dev_info);

        if (SL_IS_FAIL(operat_result)) {
            if (operat_result == SL_RESULT_OPERATION_TIMEOUT)
                std::cerr << "Error, operation time out" << std::endl;
            else
                std::cerr << "Error, unexpected error, code: " << operat_result << std::endl;
        }

        std::cout << "Version: " << (int)dev_info.hardware_version << std::endl;

        operat_result = lidar->getHealth(health_info);
        if (SL_IS_OK(operat_result)) {
            std::cout << "Lidar health status : ";
            switch (health_info.status) {
                case SL_LIDAR_STATUS_OK:
                    std::cout << "OK!";
                    break;
                case SL_LIDAR_STATUS_WARNING:
                    std::cout << "Warning!";
                    break;
                case SL_LIDAR_STATUS_ERROR:
                    std::cout << "Error!";
                    break;
            }
            std::cerr << std::endl << "errorcode: " << health_info.error_code << std::endl;

        } else
            std::cerr << "Error, cannot retrieve the lidar health code: " << operat_result << std::endl;


        if (health_info.status == SL_LIDAR_STATUS_ERROR) {
            std::cerr << "Error, slamtec lidar internal error detected. Please reboot the device to retry" << std::endl;
            lidar->reset();
        }

        lidar->setMotorSpeed();

        if (SL_IS_FAIL(lidar->startScan(0, 1))) {
            std::cerr << "Error, cannot start the scan operation" << std::endl;
        }

        num_paint = 0;
        delay(3000);
}

void MainWindow::paintEvent(QPaintEvent *event) {
    if (num_paint > 10) {
    QPainter painter(this);
    QPen pen1(Qt::blue, 8, Qt::DotLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen1);

    QPoint origin_point(300, 300);

    painter.drawPoint(origin_point);

    QPen pen2(Qt::red, 6, Qt::DotLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen2);

    for (int num = 0; num < (int) count; ++num) {
//        std::cout << "Theta: " << std::fixed << std::setprecision(2) << nodes[num].angle_z_q14 * 90.f / 16384.f;
//        std::cout << "  Distance: " << nodes[num].dist_mm_q2 / 4.0f;
//        std::cout << "  Quality: " << (nodes[num].quality >> SL_LIDAR_RESP_MEASUREMENT_QUALITY_SHIFT) << std::endl;

        if ((nodes[num].quality >> SL_LIDAR_RESP_MEASUREMENT_QUALITY_SHIFT) != 0) {
            double x = origin_point.x() + (nodes[num].dist_mm_q2 / 4.0f) * sin((nodes[num].angle_z_q14 * 90.f / 16384.f) * 3.14 / 180);
            double y = origin_point.y() - (nodes[num].dist_mm_q2 / 4.0f) * cos((nodes[num].angle_z_q14 * 90.f / 16384.f) * 3.14 / 180);

            painter.drawPoint(QPoint(int(x), int(y)));

            //painter.drawPoint(int(nodes[num].angle_z_q14 * 90.f / 16384.f), int(nodes[num].dist_mm_q2 / 4.0f));
        }
    }
    }
    ++num_paint;
}

void MainWindow::timerEvent(QTimerEvent *event)
{
    get_lidar_point_info();
}

sl_result MainWindow::get_point_info_one_cycle(sl::ILidarDriver *drv) {
    count = count_of(nodes);
    std::cout << "Count before getting the data: " << count << std::endl;

    std::cout << "waiting for data..." << std::endl;

    sl_result result = drv->grabScanDataHq(nodes, count, 0);
    std::cout << "Count after getting the data: " << count << std::endl;
    if (SL_IS_OK(result) || result == SL_RESULT_OPERATION_TIMEOUT) {
        drv->ascendScanData(nodes, count);

        update();
    } else
        std::cerr << "error code: " << result << std::endl;
    return result;
}

int MainWindow::get_lidar_point_info() {
    if (SL_IS_FAIL(get_point_info_one_cycle(lidar)))
        std::cerr << "Error, cannot grab scan data" << std::endl;

    return 0;
}

void MainWindow::start_lidar()
{
    timer_id = startTimer(110);
}

void MainWindow::end_lidar()
{
    killTimer(timer_id);
}

MainWindow::~MainWindow() {
    lidar->stop();
    delay(20);
    lidar->setMotorSpeed(0);

    if (lidar) {
        delete lidar;
        lidar = nullptr;
    }
    delete ui;
}
