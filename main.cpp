#include "MainWindow.h"

#include <QApplication>
#include <QFont>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QTextCodec>
#endif

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // 启用高分屏适配 (Qt 5 兼容)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    // 全局强制统一 UTF-8 编码，杜绝终端和文件读写乱码
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
#endif

    QApplication a(argc, argv);
    a.setApplicationName(QStringLiteral("AnsiblePilot"));
    a.setOrganizationName(QStringLiteral("DevOpsTools"));

    // 设置在各类 Linux 系统下通用的无衬线默认字体
    QFont appFont(QStringLiteral("Noto Sans CJK SC"), 10);
    appFont.setStyleHint(QFont::SansSerif);
    a.setFont(appFont);

    MainWindow w;
    w.show();
    return a.exec();
}
