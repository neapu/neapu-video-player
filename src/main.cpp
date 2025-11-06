#include <QApplication>
#include <logger.h>
#include <QDebug>
#include "view/MainWindow.h"

// clang-format off
void qtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    switch (type) {
    case QtDebugMsg:
        neapu::Logger(NEAPU_LOG_LEVEL_DEBUG, context.file, context.line, context.function) << msg.toStdString();
        break;
    case QtInfoMsg:
        neapu::Logger(NEAPU_LOG_LEVEL_INFO, context.file, context.line, context.function) << msg.toStdString();
        break;
    case QtWarningMsg:
        neapu::Logger(NEAPU_LOG_LEVEL_WARNING, context.file, context.line, context.function) << msg.toStdString();
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        neapu::Logger(NEAPU_LOG_LEVEL_ERROR, context.file, context.line, context.function) << msg.toStdString();
        break;
    }
}
// clang-format on

int main(int argc, char* argv[])
{
    neapu::Logger::setPrintLevel(NEAPU_LOG_LEVEL_INFO);
#ifdef DEBUG
    neapu::Logger::setLogLevel(NEAPU_LOG_LEVEL_DEBUG, SOURCE_DIR "/logs", "NeapuVideoPlayer");
#else
    neapu::Logger::setLogLevel(NEAPU_LOG_LEVEL_DEBUG, "logs", "NeapuVideoPlayer");
#endif
    qInstallMessageHandler(qtMessageHandler);

    QApplication app(argc, argv);

    view::MainWindow w;
    w.show();

    return app.exec();
}