#ifndef HSEMANTICSEGMENTATIONWDG_H
#define HSEMANTICSEGMENTATIONWDG_H

#include "BaseSystemFuncWdg.h"
#include "HSemanticSegmentation.h"

namespace Ui
{
class HSemanticSegmentationWdg;
}

class HSemanticSegmentationWdg : public BaseSystemFuncWdg
{
    Q_OBJECT

public:
    explicit HSemanticSegmentationWdg(
            XvCore::HSemanticSegmentation *func,QWidget *parent=nullptr);
    ~HSemanticSegmentationWdg() override;

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

    Ui::HSemanticSegmentationWdg *ui=nullptr;
};

#endif // HSEMANTICSEGMENTATIONWDG_H
