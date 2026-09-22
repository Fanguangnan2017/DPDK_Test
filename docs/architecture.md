# DPDK_Test 架构说明

## 1. 目标

本文件对项目整体进行架构级梳理，说明各模块职责、依赖关系和运行方式。

## 2. 总体架构图

```text
+----------------------------------------------------+
|                    应用层 / 业务层                   |
|  PrimaryReceiver  ChannelWorker  StateMachine      |
+--------------------+-------------------------------+
                     |
                     v
+----------------------------------------------------+
|                    处理层 / 协议层                   |
|  parse_frame  command lifecycle  WAL  metrics      |
+--------------------+-------------------------------+
                     |
                     v
+----------------------------------------------------+
|                    DPDK 数据平面                     |
|  rte_eth_rx_burst  rte_ring  rte_mbuf  rte_mempool |
+--------------------+-------------------------------+
                     |
                     v
+----------------------------------------------------+
|                    NIC / 硬件层                       |
|  2 x NIC  ports / 12 channels                       |
+----------------------------------------------------+
```

## 3. 模块分层

### 3.1 应用层

包括：

- `PrimaryReceiver`
- `ChannelWorker`
- `ChannelStateMachine`
- `ReceiverConfig`

作用：

- 启动和管理
- 数据分发
- 通道状态管理
- 控制命令发送与恢复

### 3.2 协议层

包括：

- `protocol.hpp / protocol.cpp`
- `IndustrialHeader`
- `ParsedFrame`

作用：

- 解析工业帧头
- 执行 CRC 校验
- 提取 sequence / command id / payload length
- 识别消息类型

### 3.3 数据平面

包括：

- `mempool.cpp`
- `port.cpp`
- `tx.cpp`
- `wal.cpp`

作用：

- 初始化 mempool
- 配置 NIC port
- burst 收发
- WAL 落盘

### 3.4 运行时对象

包括：

- `runtime stats`
- `ChannelStats`
- `ChannelRings`
- `ChannelStatus`

作用：

- 提供统计信息
- 汇总运行状态
- 支持实时监控和故障分析

## 4. 运行时流转

### 4.1 接收流

```text
NIC -> DPDK RX burst -> mbuf -> parse_frame -> channel dispatch -> ring -> worker
```

### 4.2 控制流

```text
PrimaryReceiver -> control_tx ring -> tx loop -> NIC
```

### 4.3 恢复流

```text
timeout / nack / error -> state machine -> Recovering -> retry / resume
```

## 5. 依赖关系

- `PrimaryReceiver` 依赖 `ReceiverConfig`
- `PrimaryReceiver` 依赖 `ChannelRings`
- `ChannelWorker` 依赖 `rte_ring` 和 `rte_mbuf`
- `ChannelStateMachine` 依赖 `ChannelStatus` 与 `ChannelCommand`
- `Protocol` 依赖 `IndustrialHeader`
- `wal` 依赖文件系统

## 6. 模块边界

### 边界清晰的部分

- protocol 与 DPDK 之间相对隔离
- state machine 与 ring 之间通过接口调用
- WAL 与 metrics 作为观测层与业务层解耦

### 需要继续强化的部分

- 真实控制命令编码格式
- 硬件级 flow control 接口
- 统一 error callback / event loop

## 7. 扩展方向

后续可以向以下方向扩展：

1. 更强的命令优先级管理
2. 更细粒度的 channel 监控
3. 更完善的 WAL snapshot 和分析工具
4. 实际工业设备场景中的协议适配
5. 统一监控平台接入

## 8. 结论

本项目的架构设计强调：

- DPDK 做数据平面
- protocol 做协议解析
- state machine 做可靠性管理
- ring / worker 做 channel 处理
- WAL / metrics 做可观测性

这样可以让系统既保持高性能，又具备工业级可靠管理能力。
