#include "XvSingleApplication.h"

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt 6 enables these by default; Qt 5 needs them before application creation.
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    XvSingleApplication a(argc, argv);

    a.init();
    int nRet=a.run();
    a.uninit();

    return nRet;
 }
