#ifndef HOBJECTDETECTIONWDG_H
#define HOBJECTDETECTIONWDG_H

#include "BaseSystemFuncWdg.h"
#include "HObjectDetection.h"

namespace Ui
{
class HObjectDetectionWdg;
}

class HObjectDetectionWdg : public BaseSystemFuncWdg
{
    Q_OBJECT

public:
    explicit HObjectDetectionWdg(
            XvCore::HObjectDetection *func,QWidget *parent=nullptr);
    ~HObjectDetectionWdg() override;

protected:
    void initFrm() override;

protected slots:
    void onFuncRunUpdate() override;
    void onShow() override;

private:
    void selectModel();
    void selectPreprocess();
    void configureCandidate(const QString &modelPath,
                            const QString &preprocessPath);
    void updateStatus();

    Ui::HObjectDetectionWdg *ui=nullptr;
};

#endif // HOBJECTDETECTIONWDG_H
