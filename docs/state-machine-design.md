# DPDK_Test 状态机设计

## 1. 状态机定位

状态机是整个工业接收系统中最关键的可靠性组件。它用于控制每个 channel 的运行状态，确保：

- 数据帧按 sequence 顺序处理
- ACK/NACK 与 command 关联
- control command 超时可恢复
- flow pause 与 resume 可触发
- 忽略失序、坏帧、重复命令等非正常行为

整个系统中，不同层级都依赖状态机：

- `PrimaryReceiver` 负责 channel 级汇总
- `ChannelWorker` 负责 channel 执行细节
- `ChannelStateMachine` 负责本地状态维护

## 2. 状态模型

### 2.1 ChannelState

```text
Init
  -> Ready
  -> Running
      -> Paused
      -> Error
      -> Recovering
```

状态含义：

- Init：对象刚创建，未初始化
- Ready：初始化完成，等待正常数据
- Running：正常执行，接收或发送都在工作
- Paused：因为高水位或 flow control 暂停处理
- Error：遇到不可恢复错误
- Recovering：等待重试或重新同步状态

### 2.2 CommandState

```text
Idle -> Sent -> Acked
             -> Nacked
             -> Timeout
             -> Failed
```

命令状态表示某个 control command 的生命周期：

- Idle：未创建
- Sent：已经发送
- Acked：已确认
- Nacked：已拒绝
- Timeout：超时未响应
- Failed：失败终止

## 3. 核心事件

### 3.1 on_lvds_frame

在接收到帧时，状态机执行：

- 验证 sequence 是否连续
- 检查是否出现 gap / duplicate / rollback
- 更新 last_lvds_sequence
- 如果 payload 过大或负载过高，触发 pause

核心逻辑：

```cpp
if (sequence > expected_sequence_) {
    invalid_sequence += sequence - expected_sequence_;
    dropped_frames += sequence - expected_sequence_;
    expected_sequence_ = sequence + 1;
    state = ChannelState::Error;
}
```

### 3.2 on_control_result

在收到 ACK/NACK 时：

- 根据 command_id 查找命令
- 标记命令状态为 Acked 或 Nacked
- 如果 NACK，则进入 Error 或 Recovering

### 3.3 on_control_command

在接收到外部控制命令时：

- 创建 command record
- 设置 command_id / issued_at_ns / deadline_ms
- 置为 Sent
- 更新 last_command_id

### 3.4 on_timeout_check

周期性检查命令是否超时：

- 若当前 state == Sent 且超过 deadline
- 则置为 Timeout
- channel 进入 Recovering

## 4. 高低水位控制

为了防止内存与 ring 过载，状态机支持高低水位机制：

- high_water：高水位阈值
- low_water：低水位阈值

### 4.1 高水位触发

当 payload_len 或负载累积超过 high_water 时：

- `flow_paused = true`
- `flow_pause_count++`
- `state = ChannelState::Paused`

### 4.2 低水位恢复

当负载回落到 low_water 以下时：

- `flow_paused = false`
- `state = ChannelState::Running`

## 5. sequence 机制

sequence 是 channel 可靠性的关键：

- 每个 LVDS frame 都有 sequence
- 状态机维护 `expected_sequence_`
- 如果接收到：
  - 大于预期：说明中间有丢包
  - 等于预期：正常
  - 小于预期：重复或乱序

这是 industrial 工况中最基本的可靠性检测能力。

## 6. ACK/NACK 机制

### 6.1 ACK

- 收到 ACK 后，command state 进入 Acked
- 对应 channel 可返回 Running

### 6.2 NACK

- 收到 NACK 后，command state 进入 Nacked
- 动态可触发 retry / reissue / error report

### 6.3 timeout

- 未在 deadline 内收到响应
- 进入 Timeout
- channel 进入 Recovering

## 7. 状态迁移逻辑

### 7.1 正常流

```text
Ready -> Running -> Running -> Running
```

### 7.2 高负载

```text
Running -> Paused -> Running
```

### 7.3 异常

```text
Running -> Error -> Recovering -> Running
```

### 7.4 命令超时

```text
Sent -> Timeout -> Recovering -> Sent
```

## 8. 线程安全

状态机使用 mutex 进行保护，避免以下问题：

- cross-thread update
- race between tx and rx
- command map corruption
- sequence inconsistency

## 9. 设计约束

状态机必须满足：

- 与 DPDK 事件循环解耦
- 只处理协议语义，不直接依赖网卡细节
- 对所有异常有明确状态变更
- 能对每个 channel 单独恢复

## 10. 当前状态

当前仓库中的状态机已包含：

- ChannelState
- CommandState
- ChannelCommand
- ChannelStatus
- ChannelStateMachine
- sequence tracking
- high-water / low-water logic
- timeout management
- pending command tracking

但还未完全完备的部分包括：

- 真正的 command 编码/解码
- ACK/NACK 的具体消息格式
- 与 rx/tx pipeline 的完整闭环
- 上层恢复策略和 fault escalation

## 11. 结论

状态机是系统可靠性的核心组件。它把“协议正确性”“数据连续性”“命令可靠性”“恢复机制”统一起来，形成一条可以持续执行的工业数据管理主线。

该状态机的设计目标，是让每个 channel 都能：

- 稳定工作
- 自动恢复
- 可观测状态
- 可排查故障
- 能与 DPDK high-performance data plane 协同工作
