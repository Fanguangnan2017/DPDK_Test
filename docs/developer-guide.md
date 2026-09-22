# DPDK_Test 开发者手册

## 1. 项目定位

本项目属于工业网络接收框架，基于 DPDK 实现高性能数据接收和控制消息管理。它的设计目标是：

- 高吞吐接收
- 多 channel 分发
- 协议校验和可靠性控制
- 监控与审计
- 可恢复的状态管理

## 2. 开发入口

建议开发者阅读顺序：

1. `docs/requirements.md`
2. `docs/system-design.md`
3. `docs/state-machine-design.md`
4. `docs/implementation-plan.md`
5. `docs/coding-standards.md`

## 3. 代码主线

关键代码入口：

- `CMakeLists.txt`：工程构建配置
- `include/protocol.hpp`：协议定义
- `include/channel_state_machine.hpp`：状态机接口
- `src/protocol.cpp`：协议解析实现
- `src/primary.cpp`：PrimaryReceiver 实现
- `src/worker.cpp`：ChannelWorker 实现
- `scripts/setup_hugepages.sh`：hugepage 设置
- `scripts/bind_nics.sh`：NIC 绑定脚本

## 4. 开发约束

### 4.1 重要约束

- DPDK 环境必须提前准备好
- NIC 必须绑定到合适的驱动
- `hugepage` 必须具备
- 协议字段必须保持一致，避免无序修改
- 状态机更新必须受控，不能直接绕过

### 4.2 线程约束

- ring 操作需要保证正确生命周期
- mbuf 必须在成功分配后负责释放
- 共享状态必须通过锁或原子保护

## 5. 典型开发流程

### 5.1 新功能开发

1. 阅读需求和设计文档
2. 确认模块边界
3. 实现对应功能
4. 添加/更新测试
5. 更新相关 docs
6. 运行编译和集成验证

### 5.2 故障定位

1. 先看 `operation-guide.md`
2. 再看 `state-machine-design.md`
3. 检查 channel 状态和 metrics
4. 观察 WAL 和日志
5. 进行回归验证

## 6. 发布前要求

提交前必须确认：

- 编译通过
- 测试通过
- 协议行为符合设计
- state machine 状态切换正常
- no silent failures
- 有足够日志和统计

## 7. 维护建议

- 对 DPDK 版本进行固定管理
- 对 NIC 绑定脚本做版本化维护
- 对状态机代码做回归测试
- 每次发布保留文档和变更说明

## 8. 结论

开发者手册的目标，是帮助团队在项目演进过程中始终保持：

- 架构一致
- 实现稳定
- 文档完整
- 运维可控

从而把工程推进到真正可交付、可维护、可演进的状态。
