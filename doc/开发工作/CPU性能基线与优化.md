# CPU 性能基线与优化（2026-09-28）

## 范围
CPU 工作站，无 NVIDIA GPU。删除 HModelMatch、HObjectDetection、HSemanticSegmentation、HALCON 专用互操作层及 SDK/运行库。旧项目如含删除角色，明确拒绝加载且不替换当前项目，不静默映射到行为不同的算子。HALCON .hdl 模型不能直接用作 ONNX .onnx 模型。

当前为 **39 个标准角色、126 个预设**；通信、机器学习、识别、缺陷检测、定位和标定相关算子重新归类。空分类隐藏，抽屉按需创建。新增能力评估见《算子分类与开放实现评估》。Qt、OpenCV、ONNX 等第三方许可声明仍须保留。

## 基线方法
Linux x86-64，16 个逻辑 CPU，GCC 9.3，Qt 5.12.8，Release，ONNX Runtime 1.18.1 CPU。同机预热 3 次、测量 15 次，报告中位数/P95。本地 OpenCV C++ 后端未启用，完整 OpenCV 回归由 Windows 验收。

模型：固定 [1,16,128,128] 恒等图；相同图带 32 MiB 文档字段（隔离文件扫描成本）；六层 3×3 卷积/ReLU，随机种子 19。预处理：1920×1080 RGB 到 640×640。均不使用客户数据。

| 测试 | 优化前 | 优化后 |
| --- | ---: | ---: |
| GUI 提交 10000 条日志的阻塞 | 893 ms | 3 ms |
| 已积压 10000 条工作线程日志的事件处理 | 943 ms | <1 ms，绘制随后分批执行 |
| 点击运行 400 ms 延时算子 | 400 ms | <1 ms，算子仍执行 400 ms |
| 32 MiB 恒等图完整算子 | 335.08 ms | 0.27 ms |
| 六层卷积完整算子 | 11.68 ms | 3.57 ms |
| 图像预处理 | 56.33 ms | 9.96 ms |

这些是合成基线，不代表实际检测模型帧率。独立线程对比中 1/2/4 线程卷积中位数约为 9.19/5.19/4.23 ms，不同轮次存在调度波动。图优化可能改变浮点累加顺序：逐一比较 262144 个输出值，最大绝对差 4.249159246683121e-9，通过 rtol=1e-4、atol=1e-6。预处理与参考结果逐字节一致，包括灰度、RGB/BGR、NCHW/NHWC、预乘透明格式和 uint8/float32。

## 运行规则
- 配置、保存和加载完整 SHA-256 校验；每次推理用文件身份、大小、修改和变更时间快检，变化再重算摘要；其他平台回退完整校验。同大小改写、还原修改时间、文件替换和丢失都有回归。
- 推理持有会话快照，UI 元数据读取不等待推理。会话内仍串行执行，失败不提交结果。
- 默认线程数 min(4,max(1,逻辑CPU数-1))，完整图优化，关闭空闲自旋。启动前用 XVISION_ONNX_THREADS=1..32 调整；非法设置明确拒绝加载。多流程并行时应分别实测 1/2/4 线程。
- 图像行读取与 256 值归一化查表；特殊格式保留原始舍入语义。
- 日志每 50 ms 刷新，显示队列最多 200 条，单条显示最多 8192 字符，文件日志不截断。清空会同步清除待显示队列。
- 单步只执行选中算子，不隐式运行上下游；使用流程受管线程。停止后当前算子结束前仍保留编辑锁；循环等待可被唤醒。
- 首次模型配置仍有完整校验和图优化成本；真实模型、相机和现场数据需要后续业务基准。

## 复现命令
生成器独立环境需要 onnx==1.16.2 与 numpy；它们不是 EXE 运行依赖。

    python XVision/scripts/Generate-CpuBenchmarkModels.py /path/to/models
    cmake --build <build-directory> --target XvOnnxCpuBenchmark XvUiResponsivenessTests
    XvOnnxCpuBenchmark /path/to/models/identity-32mb.onnx
    XvOnnxCpuBenchmark /path/to/models/conv.onnx optional-output-float32.bin
    ctest --test-dir <build-directory> -R "onnx_operators|ui_responsiveness|systemplugin" --output-on-failure

Windows 基准程序在 tests/Release；PATH 需包含 Bin 和 Bin/XvFuncCollection。部署包只含主程序运行依赖，不含 Python 或基准模型。

## 验收
GUI 原始基线先复现 3 项失败；修复后验证全量 CTest、PowerShell 包契约、含陈旧 HALCON DLL 的隔离打包夹具、原生 Windows 全功能测试和 SDK 隔离启动。最终提交、Windows run ID 与包 SHA-256 记录在本次交付记录。
