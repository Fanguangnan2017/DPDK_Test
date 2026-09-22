# DPDK_Test 验收标准

## 1. 目标

本文件用于统一判断项目是否达到工程可用状态和工业可靠性要求。

## 2. 功能验收

### 2.1 DPDK 基础功能

- 能在目标环境中安装并识别 DPDK
- 能执行 `rte_eal_init`
- 能初始化 port
- 能创建 mbuf pool
- 能创建 ring

验收标准：

- 编译通过
- 运行无崩溃
- 可以看到 port 上线

### 2.2 协议解析功能

- 能识别 `IndustrialHeader`
- 能检查 magic 和 ether type
- 能检查 CRC
- 能正确提取 `channel_id`、`command_id`、`stream_sequence`
- 能识别 `MessageType`

验收标准：

- 正常帧能被解析
- 非法帧能被识别并丢弃
- CRC 错误可统计

### 2.3 channel 分发功能

- 每个 channel 都有独立的 ring
- LVDS 数据进入 `lvds_rx`
- control result 进入 `control_result_rx`
- control command 进入 `control_tx`

验收标准：

- channel 分发行为正确
- 不发生 ring 错分配
- queue depth 可统计

### 2.4 状态机功能

- 每个 channel 都维护状态
- sequence 连续性可检测
- ACK/NACK 可对应 command
- timeout 可进入 Recovering
- 高低水位可触发 pause/resume

验收标准：

- 乱序和丢包可识别
- NACK 与 timeout 可被记录
- 运行状态切换符合设计

## 3. 可靠性验收

### 3.1 丢包处理

- sequence 断裂可以记录
- dropped_frames 증가
- invalid_sequence 可见

### 3.2 恢复能力

- command timeout 可进入恢复状态
- channel 可以重新进入 Running
- `Recovering` 过程具有明确状态转移

### 3.3 WAL 审计能力

- 关键事件可落盘
- 能对故障回溯
- WAL 失败可统计

## 4. 监控验收

必须具备以下指标：

- total_rx_frames
- total_rx_bytes
- total_dropped
- total_crc_errors
- wal_write_failures
- per-channel lvds_frames
- per-channel control_result_frames
- per-channel dropped_frames
- per-channel last_sequence
- per-channel queue_depth

## 5. 运维验收

### 5.1 启动脚本

- 可以启动 primary
- 可以启动 channel_worker
- 配置脚本可执行
- hugepage 与 NIC 绑定脚本可用于部署

### 5.2 日志和故障排查

- 可以通过日志追踪状态切换
- 可以通过 WAL 回溯异常帧和命令
- 可以通过 metrics 观测部署健康度

## 6. 性能验收

- 应支持 burst 接收模式
- 应尽量避免不必要的复制
- ring 访问应在低延迟范围内
- 在高负载下尽量保持稳定

## 7. 退出标准

项目达到以下状态即可视为“工程可交付”：

- 能在仿真或真实环境中启动
- 能接收并解析包
- 能按 channel 分发
- 能检测和恢复错误
- 能输出状态指标
- 能进行 WAL 审计

## 8. 当前工程状态

当前项目已达到：

- 框架搭建完成
- protocol 与 state machine 设计已落地
- channel 分发与 metrics 框架已具备

尚未完成：

- 完整真实网卡部署验证
- 实际命令格式完整闭环验证
- 更严苛的工业场景恢复测试

## 9. 结论

本验收标准的核心思想，是把“能跑”与“能用”区分开：

- 能跑：工程编译成功，DPDK 启动正常
- 能用：协议解析、状态机、channel 分发、恢复机制、监控与 WAL 均具备

当前项目已具备基础工程能力，后续要持续验证真实环境中的稳定性与可靠性。
