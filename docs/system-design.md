# DPDK_Test 系统设计说明

## 1. 设计目标

本系统的目标，是构建一个基于 DPDK 的工业接收平台，围绕两张网卡与 12 个 channel 进行数据接入、状态管理、协议解析和控制响应。

系统目标并不是单纯接收数据，而是要在工业场景中保证：

- 数据流稳定
- 通道隔离
- 乱序可检测
- 丢包可发现
- 控制命令可追踪
- ACK/NACK 可处理
- timeout 可恢复
- WAL 可审计
- flow control 可触发

## 2. 总体架构

系统分为三层：

### 2.1 数据平面（DPDK）

- 使用 DPDK 的 rte_eal、rte_ethdev、rte_mbuf、rte_ring、rte_mempool
- 完成网卡初始化、端口配置、队列设置、burst 接收
- 负责将原始帧放入 ring 中，供上层处理

### 2.2 处理平面（工作模块）

- PrimaryReceiver：总入口，管理 port、ring、统计、启动与停止
- ChannelWorker：按 channel 处理数据和命令
- ChannelStateMachine：对每个 channel 单独维护状态
- Protocol Parser：解析工业数据头，识别序列号、消息类型、command_id 等字段

### 2.3 控制平面

- 通过 ring 传递控制消息
- 通过 ACK/NACK 通知发送端状态
- 通过 WAL 记录执行记录
- 通过 metrics 统计系统状态

## 3. 组件职责

### 3.1 PrimaryReceiver

职责：

- 初始化 DPDK 环境
- 创建 mbuf pool
- 创建 per-channel ring
- 配置网卡端口
- 启动 rx/tx loop
- 汇总 channel 状态和统计数据
- 负责 TX 侧发送控制信息

### 3.2 ChannelWorker

职责：

- 绑定 channel 唯一 ring
- 处理 lvds ring 中的数据
- 处理 control result ring 中的数据
- 发出 control tx 指令
- 维护 channel 级状态更新和心跳

### 3.3 ChannelStateMachine

职责：

- 维护每个 channel 的状态
- 跟踪 sequence 连续性
- 维护 command lifecycle
- 处理 ACK/NACK
- 根据高低水位触发 pause/resume
- 在 timeout 后进入 recover

### 3.4 Protocol

职责：

- 解析帧头
- 提取 payload
- 校验 magic、ether type、header length
- 提取 stream sequence / timestamp / command id
- 校验 header_crc32

## 4. 数据流设计

### 4.1 接收链路

```text
NIC port -> rte_eth_rx_burst -> mbuf -> parse_frame -> channel dispatch -> ring -> worker
```

### 4.2 分发原则

- 按 channel_id 分发
- 每个 channel 拥有独立 ring
- lvds 数据走 lvds ring
- 控制结果走 control result ring
- 停止 / pause / recover 状态由 state machine 控制

### 4.3 TX 链路

```text
command -> control_tx ring -> tx_loop -> rte_eth_tx_burst -> NIC port
```

## 5. Ring 设计

每个 channel 至少具备三类 ring：

- lvds_rx
- control_result_rx
- control_tx

使用目的：

- lvds_rx：保存普通 LVDS 数据
- control_result_rx：保存 command 执行结果
- control_tx：保存待发送的管理/控制消息

## 6. 状态机设计

### 6.1 ChannelState

```text
Init -> Ready -> Running -> Paused / Error / Recovering
```

状态含义：

- Init：刚创建，未初始化
- Ready：初始化完成，可接收
- Running：正常运行
- Paused：由于高负载或 flow control 暂停
- Error：校验失败、异常场景
- Recovering：超时重试/恢复阶段

### 6.2 CommandState

```text
Idle -> Sent -> Acked / Nacked / Timeout / Failed
```

## 7. 事件设计

关键事件包括：

- 新帧接收
- sequence 递增
- 丢包检测
- CRC 错误
- ACK 收到
- NACK 收到
- command timeout
- high-water trigger
- low-water resume

## 8. 可靠性设计

### 8.1 丢包检测

- 使用 stream_sequence 进行顺序检查
- 若 sequence 间断或回退，则标记 dropped_frames / invalid_sequence

### 8.2 CRC 校验

- 每个工业帧的 `header_crc32` 需要和解析后的 header 进行验证
- 失败则视为 CRC 错误并丢弃帧

### 8.3 超时恢复

- 对每个 control command 设置 deadline_ms
- 若超过 deadline 未收到 ACK，则状态转为 Timeout
- 再进入 Recovering 状态

### 8.4 WAL

- 对每个处理结果记录到 WAL
- 可用于事后追踪故障和恢复路径

## 9. 流量控制设计

### 9.1 高水位

当某个 channel 的数据量/负载超过阈值时：

- 进入 Paused 状态
- 发送 FlowControl 或本地阻断消息

### 9.2 低水位

当负载下降到低水位以下时：

- 恢复到 Running
- 允许继续接收和处理

## 10. 监控与统计

系统应持续输出：

- total_rx_frames
- total_rx_bytes
- total_dropped
- total_crc_errors
- wal_write_failures
- per-channel frame count
- per-channel sequence lost
- per-channel last sequence
- per-channel queue depth

## 11. 安全与稳定性

### 11.1 多线程安全

- state machine 内部使用 mutex 保护状态
- ring 访问应保证接收与发送逻辑的可见性
- 不允许跨线程直接访问 shared state

### 11.2 错误处理

- DPDK 初始化失败返回明确错误
- 端口初始化失败应日志输出
- ring 创建失败应失败退出
- packet 校验失败应记录但不影响系统继续工作

## 12. 扩展性

后续可扩展项：

- 更复杂的工业控制协议
- 更严格的控制命令优先级
- 更细粒度的 ACK 确认策略
- 更统一的元数据、拓扑和故障诊断模块
- 更高层的监控仪表盘和告警中心

## 13. 当前实现状态

当前仓库中的实现已经具备：

- DPDK 工程骨架
- ring / queue 结构
- 多 channel 分发思路
- protocol 解析基础
- state machine 框架
- 高低水位与 timeout 最小模型

但还未完全实现：

- 真正的设备级 flow-control 接口
- 完整 command 编码/解码
- 真实网络协议与 payload 语义
- 端到端重试和恢复流程

## 14. 结论

本系统的设计重点是：

- 稳定接收数据
- 按通道隔离处理
- 使用状态机保证可靠性
- 通过 ACK/NACK、timeout、pause/resume、WAL 与 metrics 实现工业级可追踪性

确定了这些设计原则后，后续代码实现可以围绕：

- 接收 pipeline
- ring dispatch
- channel state machine
- tx control
- health monitoring

这一主线持续推进。
