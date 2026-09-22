# DPDK_Test 编码规范

## 1. 总体原则

本项目代码遵循以下原则：

- 清晰优先于炫技
- 明确模块边界
- 可靠性优先于极限性能
- 线程安全优先于随意共享状态
- 结果可观测优先于“看起来能跑”

## 2. 命名规范

### 2.1 文件命名

- 采用 `snake_case` 命名
- 例如：
  - `channel_state_machine.cpp`
  - `receiver_config.hpp`
  - `dpdk_helpers.hpp`

### 2.2 类/结构体命名

- 使用 `PascalCase`
- 例如：
  - `ChannelStateMachine`
  - `PrimaryReceiver`
  - `ChannelWorker`

### 2.3 函数命名

- 使用 `snake_case`
- 例如：
  - `initialize`
  - `parse_frame`
  - `enqueue_control_command`

### 2.4 常量命名

- 使用 `k` 前缀
- 例如：
  - `kChannelCount`
  - `kMaxPayloadLength`
  - `kIndustrialMagic`

## 3. 目录结构规范

```text
include/       公开头文件
src/           实现文件
apps/          可执行程序入口
scripts/        启动脚本
docs/          设计与规范文档
```

## 4. 头文件规范

- 头文件尽量只声明接口，不写具体逻辑
- 使用 `#pragma once`
- 尽量避免全局可变状态
- 公共结构应放在 `include/` 目录

## 5. 实现规范

### 5.1 线程安全

- 共享状态必须通过锁或原子保护
- 外部线程不得直接修改状态机内部对象
- 状态机和 ring 必须保持最小访问接口

### 5.2 错误返回

- 函数返回值必须显式反映成功/失败
- 对失败场景必须输出日志或统计信息
- 不允许“静默失败”

### 5.3 日志规范

- 日志必须包含：
  - 事件类型
  - channel id（如适用）
  - 关键参数
  - 错误原因
- 禁止无上下文的大量日志刷屏

## 6. DPDK 编码要求

- 任何 mbuf / ring / port / queue 访问必须保证有效性检查
- 在调用 `rte_eth_rx_burst` / `rte_eth_tx_burst` 前必须确认队列可用
- 分配失败必须处理后续清理
- 必须保证 `rte_pktmbuf_free` 对所有失败路径生效

## 7. 状态机编码要求

- 状态迁移必须有明确条件
- 命令必须带 command_id
- timeout 检查必须周期性执行
- sequence 检查必须在处理帧入口处执行

## 8. 代码审查要求

提交前至少检查：

- 是否有未处理错误路径
- 是否有内存泄漏
- 是否有未释放 mbuf
- 是否有未加锁共享变量
- 是否有状态机状态不一致

## 9. 提交要求

- 提交说明必须写清楚改动目的
- 如果是协议改动，必须说明字段影响
- 如果是状态机改动，必须说明状态迁移
- 新增功能应配套文档更新

## 10. 结论

编码规范的目的，是让项目保持：

- 可维护
- 可审计
- 可扩展
- 可恢复
- 可靠运行

在 DPDK 工程中，规范性尤其重要，因为很小的错误可能会放大为稳定性问题。
