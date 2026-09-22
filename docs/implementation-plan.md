# DPDK_Test 实施计划

## 1. 总体目标

本项目的实施目标，是在 DPDK 平台上构建一个具备工业级可靠性要求的接收框架，支持：

- 两张网卡的高吞吐输入
- 12 个 channel 的并发/隔离处理
- 协议头解析与校验
- ACK/NACK 与 timeout 管理
- flow control 与 channel pause/resume
- WAL 审计与 metrics 统计

实施路线分为四个阶段：

1. 基础框架搭建
2. 协议解析与 ring 分发
3. 状态机与可靠性控制
4. 监控、运维与优化

## 2. 第一步：基础框架搭建

### 2.1 目标

建立工程骨架，保证 DPDK 环境能够正常初始化与运行。

### 2.2 工作项

- 使用 CMake 构建工程
- 配置 DPDK include 与 lib 路径
- 创建 `PrimaryReceiver` 与 `ChannelWorker` 基本结构
- 创建 mempool 与 port 配置模块
- 创建 ring 与 channel 结构体

### 2.3 验收标准

- cmake 能成功配置
- make 能成功编译
- 程序可以启动 DPDK EAL
- 端口和 ring 能够创建成功

## 3. 第二步：协议解析与分发

### 3.1 目标

让系统能够识别工业数据帧并按 channel 分发。

### 3.2 工作项

- 定义 `IndustrialHeader`
- 实现 `parse_frame`
- 校验 magic、ether type、length、header_crc32
- 提取 `channel_id`、`command_id`、`stream_sequence`
- 将 payload 分发到 channel 对应 ring

### 3.3 验收标准

- 帧头可识别
- payload 正确通过 ring 传递
- 非法帧被丢弃并记录日志/统计

## 4. 第三步：状态机与可靠性控制

### 4.1 目标

让每个 channel 都能自行处理可靠性问题。

### 4.2 工作项

- 建立 `ChannelStateMachine`
- 实现 `on_lvds_frame`
- 实现 `on_control_result`
- 实现 `on_timeout_check`
- 实现 `should_pause_for_high_water`
- 实现 `should_resume_for_low_water`
- 实现 command lifecycle 和 retries

### 4.3 关键行为

- 如果 sequence 断裂，标记丢包
- 如果 command 超时，进入 Recovering
- 如果 channel 负载过高，进入 Paused
- 如果 channel 恢复，回到 Running

### 4.4 验收标准

- sequence 乱序可检测
- timeout 可触发恢复
- ACK/NACK 能对应 command
- high-water / low-water 可切换状态

## 5. 第四步：监控与运维

### 5.1 目标

使系统具备运营可见性与故障定位能力。

### 5.2 工作项

- 输出 metrics
- 写 WAL
- 将状态变更记录到日志
- 提供 health check 与状态查询
- 设计脚本启动/监控方案

### 5.3 验收标准

- 能看到 total_rx_frames 等统计值
- 能观察到 per-channel 状态
- 能看到丢包、CRC 错误、timeout
- 能通过 WAL 回溯异常

## 6. 关键实施原则

- 先构建可运行框架，再逐步完善协议细节
- 核心可靠性功能优先于“花哨优化”
- ring 与 state machine 应保持独立模块化
- 任何异常都需要可统计、可观测、可恢复

## 7. 风险与应对

### 7.1 DPDK 环境差异

风险：版本差异导致 API 不匹配。

对策：固定版本、确认 header 与 lib 路径、统一构建脚本。

### 7.2 PCI/NIC 差异

风险：不同网卡绑定方式不同。

对策：通过脚本设置、明确端口绑定方式、测试环境先单端口再双端口。

### 7.3 真实协议不一致

风险：工业帧的定义与假设不一致。

对策：协议定义必须在最早阶段定稿，后续不得随意变更字段布局。

## 8. 实施时间安排

### 第一周

- DPDK 环境准备
- CMake 工程完成
- 实现 port / mempool / ring 基础框架

### 第二周

- 协议解析实现
- 解析 sequence 和 command_id
- 完成 channel 分发

### 第三周

- 状态机上线
- ACK/NACK/timeout 处理
- flow control 与 pause/resume

### 第四周

- WAL 与 metrics
- 故障恢复测试
- 启动脚本与 README 最终整理

## 9. 结论

该实施计划遵循“先框架、后协议、后状态机、再运维”的顺序推进，能够稳步实现从基础 DPDK 收包到工业级可靠接收的完整闭环。
