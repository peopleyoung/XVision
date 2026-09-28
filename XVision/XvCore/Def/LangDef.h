#ifndef LANGDEF_H
#define LANGDEF_H
#include "XLanguage.h"
#include "XLogger.h"

#define Core_Plg_InitFail               "Core_Plg_InitFail"                     //插件初始化失败
#define Core_XvFunc_RegFail            "Core_XvFunc_RegFail"                  //算子注册失败
#define Core_XvFunc_RegSuccess         "Core_XvFunc_RegSuccess"               //算子注册成功

#define Core_XvCoreMgr_CreateProjectError1       "Core_XvCoreMgr_CreateProjectError1" //创建项目失败,项目无法释放
#define Core_XvCoreMgr_ProjectRunning            "Core_XvCoreMgr_ProjectRunning"
#define Core_XvCoreMgr_ValidateProjectError      "Core_XvCoreMgr_ValidateProjectError"
#define Core_XvCoreMgr_LoadProjectError          "Core_XvCoreMgr_LoadProjectError"
#define Core_XvCoreMgr_LoadProjectSuccess        "Core_XvCoreMgr_LoadProjectSuccess"
#define Core_XvCoreMgr_ProjectIsNull             "Core_XvCoreMgr_ProjectIsNull"
#define Core_XvCoreMgr_ProjectPathEmpty          "Core_XvCoreMgr_ProjectPathEmpty"
#define Core_XvCoreMgr_SaveProjectError          "Core_XvCoreMgr_SaveProjectError"
#define Core_XvCoreMgr_SaveProjectSuccess        "Core_XvCoreMgr_SaveProjectSuccess"
#define Core_XvCoreMgr_ValidateFlowError         "Core_XvCoreMgr_ValidateFlowError"
#define Core_XvCoreMgr_ImportFlowError           "Core_XvCoreMgr_ImportFlowError"
#define Core_XvCoreMgr_ImportFlowSuccess         "Core_XvCoreMgr_ImportFlowSuccess"
#define Core_XvCoreMgr_ExportFlowError           "Core_XvCoreMgr_ExportFlowError"
#define Core_XvCoreMgr_ExportFlowSuccess         "Core_XvCoreMgr_ExportFlowSuccess"
#define Core_XvCoreMgr_CurrentProjectIsNull      "Core_XvCoreMgr_CurrentProjectIsNull"
#define Core_XvCoreMgr_ProjectStateChanged       "Core_XvCoreMgr_ProjectStateChanged"
#define Core_XvCoreMgr_AdoptFlowFailed           "Core_XvCoreMgr_AdoptFlowFailed"
#define Core_XvCoreMgr_FlowUiRestoreFailed       "Core_XvCoreMgr_FlowUiRestoreFailed"
#define Core_XvCoreMgr_FlowNotFound              "Core_XvCoreMgr_FlowNotFound"
#define Core_XvCoreMgr_FlowPathEmpty              "Core_XvCoreMgr_FlowPathEmpty"

//XvFuncType
#define Core_XvFuncType_Null                                "Core_XvFuncType_Null"                      //无
#define Core_XvFuncType_ImageAcquisition                    "Core_XvFuncType_ImageAcquisition"          //图像采集
#define Core_XvFuncType_Calibration                         "Core_XvFuncType_Calibration"               //校正标定
#define Core_XvFuncType_Contraposition                      "Core_XvFuncType_Contraposition"            //对位
#define Core_XvFuncType_Location                            "Core_XvFuncType_Location"                  //定位
#define Core_XvFuncType_ImageProcessing                     "Core_XvFuncType_ImageProcessing"           //图像处理
#define Core_XvFuncType_Measurement                         "Core_XvFuncType_Measurement"               //测量
#define Core_XvFuncType_DefectDetection                     "Core_XvFuncType_DefectDetection"           //检测
#define Core_XvFuncType_Recognition                         "Core_XvFuncType_Recognition"               //识别
#define Core_XvFuncType_Communication                       "Core_XvFuncType_Communication"             //通讯
#define Core_XvFuncType_MachineLearning                     "Core_XvFuncType_MachineLearning"           //机器学习
#define Core_XvFuncType_DataProcessing                      "Core_XvFuncType_DataProcessing"            //数据处理
#define Core_XvFuncType_Logic                               "Core_XvFuncType_Logic"                     //逻辑
#define Core_XvFuncType_Special                             "Core_XvFuncType_Special"                   //特殊
#define Core_XvFuncType_Other                               "Core_XvFuncType_Other"                     //其他
#define Core_XvFuncType_User                                "Core_XvFuncType_User"                      //用户

//XvProject
#define Core_XvProject_RemoveXvFuncError1                        "Core_XvProject_RemoveXvFuncError1"                      //移除流程[%1]错误，流程无法释放
#define Core_XvProject_RemoveXvFuncError2                        "Core_XvProject_RemoveXvFuncError2"                      //移除流程错误，流程Id[%1]不存在
#define Core_XvProject_RunStatusRunning                          "Core_XvProject_RunStatusRunning"
#define Core_XvProject_RunStatusOk                               "Core_XvProject_RunStatusOk"
#define Core_XvProject_RunStatusFail                             "Core_XvProject_RunStatusFail"
#define Core_XvProject_RunStatusError                            "Core_XvProject_RunStatusError"
#define Core_XvProject_RunStatusStopped                          "Core_XvProject_RunStatusStopped"
#define Core_XvProject_RunThreadError                            "Core_XvProject_RunThreadError"
//XvFlow
#define Core_XvFlow_CreateXvFuncError1                        "Core_XvFlow_CreateXvFuncError1"                      //创建算子错误，标识符[%1]无效
#define Core_XvFlow_RemoveXvFuncError1                        "Core_XvFlow_RemoveXvFuncError1"                      //移除算子[%1]错误，算子无法释放
#define Core_XvFlow_RemoveXvFuncError2                        "Core_XvFlow_RemoveXvFuncError2"                      //移除算子错误，算子Id[%1]不存在

#define Core_XvFlow_RunStatusInit                           "Core_XvFlow_RunStatusInit"    //初始状态
#define Core_XvFlow_RunStatusRunning                        "Core_XvFlow_RunStatusRunning"    //正在运行
#define Core_XvFlow_RunStatusOk                             "Core_XvFlow_RunStatusOk"    //运行成功
#define Core_XvFlow_RunStatusFail                           "Core_XvFlow_RunStatusFail"    //运行失败
#define Core_XvFlow_RunStatusError                          "Core_XvFlow_RunStatusError"    //运行错误


#define Core_XvFlow_FlowLegalError1                        "Core_XvFlow_FlowLegalError1"                      ///流程合法性错误，起始节点(无父节点)数量不等于1
//XvFunc
#define Core_XvFunc_RunStatusInit                           "Core_XvFunc_RunStatusInit"    //初始状态
#define Core_XvFunc_RunStatusRunning                        "Core_XvFunc_RunStatusRunning"    //正在运行
#define Core_XvFunc_RunStatusOk                             "Core_XvFunc_RunStatusOk"    //运行成功
#define Core_XvFunc_RunStatusFail                           "Core_XvFunc_RunStatusFail"    //运行失败
#define Core_XvFunc_RunStatusError                          "Core_XvFunc_RunStatusError"    //运行错误
#endif // LANGDEF_H
