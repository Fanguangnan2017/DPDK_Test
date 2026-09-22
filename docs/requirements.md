# DPDK_Test 项目需求梳理

## 1. 项目背景

本项目的目标，是构建一个基于 DPDK 的工业级接收框架，能够从两块网卡接收数据，并按照通道化方式进行分发、状态跟踪和控制命令处理。

该项目的设计目标不是“简单收包”，而是要满足工业场景中的高可靠、低丢包、可恢复、可监控的要求。

## 2. 核心业务目标

### 2.1 目标系统

系统需要具备以下能力：

- 从 2 个 DPDK 端口接收工业数据帧
- 将数据流按 12 个 channel 进行分发
- 通道之间进行独立 ring 传递
- 支持 LVDS 数据、控制结果、控制命令、状态报告、ACK/NACK 等类型
- 支持 TX 侧发送控制命令和状态消息
- 在运行中提供状态、统计和故障检测能力

### 2.2 关键需求

- 支持高吞吐数据接收
- 支持多个通道并行处理
- 支持低延迟 RING 间传递
- 支持丢包检测与恢复
- 支持 packet 校验和 CRC 校验
- 支持 WAL 日志落盘
- 支持 flow control 和 pause/resume 机制
- 支持 ACK/NACK 超时重试机制

## 3. 需求拆解

### 3.1 网络接收需求

- 使用 DPDK 作为高速数据平面
- 每个 port 需要支持 rx/tx queue
- 每个端口需要完成设备配置、队列绑定和启动
- 需要支持 burst 接收，降低调度开销
- 对数据包执行工业协议解析

### 3.2 协议需求

- 需要定义工业帧头结构
- 头部至少包含：
  - magic
  - version
  - message type
  - channel id
  - stream sequence
  - timestamp
  - payload length
  - command id
  - CRC
- 必须支持多种 message type：
  - LvdsData
  - ControlCommand
  - ControlResult
  - StatusReport
  - Heartbeat
  - Ack
  - Nack
  - FlowControl
  - ErrorReport
- 需要对数据包进行合法性校验，避免伪造帧或损坏帧进入处理链路

### 3.3 通道管理需求

- 通道数量固定为 12
- 每个 channel 至少具有：
  - LVDS 数据 ring
  - control result ring
  - control tx ring
- channel 状态必须独立维护
- 每个 channel 需要具备状态生命周期：
  - Init
  - Ready
  - Running
  - Paused
  - Error
  - Recovering

### 3.4 状态机需求

- 每个 channel 都应维护一个状态机
- 状态机用于：
  - 检测 LVDS sequence 连续性
  - 检测丢包、重复、乱序
  - 管理 ACK/NACK
  - 管理 command timeout
  - 触发 recover / retry
  - 判定 flow pause 和 resume
- 关键状态：
  - on_lvds_frame
  - on_control_result
  - on_control_command
  - on_timeout_check
  - should_pause_for_high_water
  - should_resume_for_low_water

### 3.5 发送控制需求

- PrimaryReceiver 需要支持向某个 channel 发送控制命令
- Worker 需要支持把待发控制命令放入 TX ring
- TX 模块需要支持：
  - 从 ring 中 dequeue
  - 构造 mbuf
  - 调用 rte_eth_tx_burst 发送
- 发送逻辑必须具备失败重试或错误记录能力

### 3.6 恢复与高可靠需求

- 当发生乱序或丢包，应输出 dropped_frames / invalid_sequence
- 当 command 超时，应进入 Recovering 状态
- 当 channel 使用率超过高水位，应进入 Paused 状态
- 当 channel 降到低水位后，应重新恢复到 Running
- 错误情况应持续记录在 WAL 中，便于后续异常排查

### 3.7 监控需求

系统需要能够对以下指标进行统计：

- total_rx_frames
- total_rx_bytes
- total_dropped
- total_crc_errors
- wal_write_failures
- per-channel LVDS frames
- per-channel result frames
- per-channel control tx count
- per-channel sequence lost
- per-channel queue depth
- last_sequence

## 4. 非功能需求

### 4.1 性能需求

- 需要尽量以 DPDK 高性能模式处理收发
- 需要支持 burst 处理
- 需要尽量少的额外拷贝
- 需要保证 ring 的非阻塞/低等待模式

### 4.2 稳定性需求

- 需要避免竞争条件导致状态损坏
- 需要保证多线程中对状态的访问安全
- 需要对 DPDK 环境初始化失败做出明确处理
- 需要在初始化、启动、停止和错误恢复等路径中保持一致性

### 4.3 可扩展性需求

- 需要保持 channel 层和 port 层解耦
- 状态机和协议解析逻辑应该独立于 DPDK 具体调用细节
- 后续可进一步支撑：
  - 更复杂的 control protocol
  - 更高层的 industrial command bus
  - 更细粒度的 ACK/NACK 语义
  - 设备级 flow control 处理

## 5. 架构需求

### 5.1 组件拆分

系统应至少包含以下模块：

- protocol：协议解析和 CRC 校验
- receiver_config：配置对象
- shared_channel：ring 和统计结构定义
- worker：ChannelWorker，负责 channel 级工作线程
- primary：PrimaryReceiver，负责总入口和 RX/TX 总控制
- mempool：mbuf pool 创建
- port：端口配置和网卡初始化
- wal：WAL 写入
- tx：TX 发送函数
- channel_state_machine：通道状态机

### 5.2 进程模型

- PrimaryReceiver 负责：
  - DPDK EAL 初始化
  - 端口配置
  - ring 创建
  - 接收 burst 批处理
  - 状态汇总
  - TX flush
- ChannelWorker 负责：
  - attach channel ring
  - dequeue LVDS / result
  - 发送 control command
  - 维护 channel 状态

## 6. 关键设计原则

- 尽量保持 DPDK 逻辑与上层工业协议分离
- 多线程状态必须受保护
- 丢包和错误必须可观测
- control plane 与 data plane 必须清晰分层
- 状态机应支持恢复，而非简单失败退出

## 7. 验收标准

在工程层面，项目应满足以下验收条件：

- 可以编译并链接成功
- 可以初始化 DPDK EAL
- 可以绑定网卡并启动端口
- 可以创建 ring
- 可以对真实帧执行 parse_frame
- 可以按 channel 分发数据
- 可以记录超时、丢包、CRC 错误、WAL 失败
- 可以维护每个通道状态
- 可以在 channel 级别处理 flow pause / retry / recover

## 8. 当前实现状态说明

当前项目在代码层面已经具备：

- DPDK 基础工程骨架
- ring 和 memory pool 结构
- channel / worker / primary 的基本分层
- state machine 的核心接口和状态模型
- ACK/NACK / timeout / pause / recover 的基础框架

但仍然需要继续补齐以下实际能力：

- 真实网卡 PCI 绑定与驱动选择
- 真实 DPDK PMD 和 port 配置
- 真实工业协议字段的最终落地
- 对 `IndustrialHeader` 的完整解码与编码
- 更严格的 flow-control 定义
- 更真实的 command replay 和 retries

## 9. 结论

本项目的本质需求可以概括为：

“构建一个高性能、可观测、可恢复、符合工业协议语义的 DPDK 数据接收和控制框架，能够在 12 个通道中稳定接收、筛选、分发和管理数据，并具备 ACK/NACK、超时、丢包检测、flow control 和 WAL 审计能力。”

这正是当前项目的主要目标，也是后续工程实现的核心方向。
