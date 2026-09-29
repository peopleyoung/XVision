#include "GlobalScript.h"
#include <cstring>
#include "XvSingleApplication.h"

int main(int argc, char *argv[])
{
    if(argc==2 && std::strcmp(argv[1],"--script-worker")==0) return runGlobalScriptWorker(argc,argv);
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
