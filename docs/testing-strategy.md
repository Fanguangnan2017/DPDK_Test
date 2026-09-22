# DPDK_Test 测试策略

## 1. 测试目标

本项目的测试目标，是保证：

- 代码能够稳定编译
- DPDK 初始化和端口启动正常
- 协议解析正确
- 状态机迁移符合预期
- ring 分发和 tx 流程可工作
- 关键异常能够被检测并恢复

## 2. 测试分层

### 2.1 单元测试

单元测试主要覆盖：

- `parse_frame`
- `crc32`
- `ChannelStateMachine::on_lvds_frame`
- `ChannelStateMachine::on_control_result`
- `ChannelStateMachine::on_timeout_check`

重点验证：

- 正常帧通过
- CRC 错误被拒绝
- sequence gap 可检测
- timeout 可触发状态转移

### 2.2 集成测试

集成测试覆盖：

- `PrimaryReceiver::initialize`
- `PrimaryReceiver::start`
- ring 创建与分发
- channel worker attach
- tx/rx burst 路径

重点验证：

- 端口可启动
- ring 可工作
- LVDS 数据能进入正确 channel
- control result 可回流

### 2.3 协议测试

模拟真实工业帧，验证：

- magic 正确
- command_id 提取正确
- sequence 正确
- header_crc32 校验正确
- message type 正确识别

### 2.4 性能测试

关键指标：

- burst throughput
- packet loss rate
- latency
- ring backlog
- CPU utilization

## 3. 测试用例设计

### 3.1 协议测试用例

- 正常 IPv4/工业帧测试
- 错误 magic 测试
- CRC 错误测试
- 不合法 channel id 测试
- 非法 message type 测试
- payload 超长测试

### 3.2 状态机测试用例

- 正常 sequence 递增
- sequence 跳变测试
- command timeout 测试
- NACK 测试
- high-water transition 测试
- low-water resume 测试

### 3.3 ring 测试用例

- ring empty
- ring full
- 多 producer / single consumer
- consumer 超时退出

## 4. 测试执行方式

### 4.1 本地测试

适合：

- 解析逻辑校验
- 状态机行为验证
- 功能单测

### 4.2 纯模拟测试

适合：

- DPDK 环境不可用时的逻辑验证
- 运行路径模拟
- 异常路径覆盖

### 4.3 真实设备测试

适合：

- 端口绑定验证
- DPDK PMD 兼容性验证
- burst throughput 测试
- 环境相关 bug 覆盖

## 5. 测试要求

- 所有主要分支都必须有测试覆盖
- 严重问题必须有回归测试
- 测试必须明确说明预期行为
- 错误测试必须验证失败返回和日志输出

## 6. 质量门禁

项目可以进入发布前，须满足：

- 编译通过
- 单元测试通过
- 核心集成测试通过
- 协议解析测试通过
- 关键异常场景通过
- 无严重丢包 bug

## 7. 结论

测试策略的核心，是让系统不仅“能跑”，而且在错误、网络不稳定、负载高峰或协议异常时仍然能够保持可控性和恢复能力。
