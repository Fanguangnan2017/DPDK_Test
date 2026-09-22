# DPDK_Test 项目文档索引

## 1. 文档总览

本项目文档集按“需求 -> 设计 -> 实施 -> 规范 -> 运维”顺序组织，便于从高层理解到细节落地。

## 2. 文档目录

### 2.1 需求与目标

- `docs/requirements.md`  
  项目需求梳理，说明系统的业务目标、技术目标、可靠性目标和验收诉求。

- `docs/system-design.md`  
  总体系统设计，解释架构层次、模块职责和运行时流转。

### 2.2 状态与协议设计

- `docs/state-machine-design.md`  
  每个 channel 的状态机设计，涵盖 sequence、ACK/NACK、timeout、pause/resume。

- `docs/architecture.md`  
  架构说明，给出模块分层、运行链路和扩展思路。

### 2.3 实施与验收

- `docs/implementation-plan.md`  
  四阶段实施计划，说明开发推进顺序。

- `docs/acceptance-criteria.md`  
  工程验收标准，用于评估是否达到可交付状态。

### 2.4 规范与流程

- `docs/coding-standards.md`  
  命名、目录、线程安全、DPDK 编码规范。

- `docs/testing-strategy.md`  
  单元测试、集成测试、协议测试和性能测试策略。

- `docs/release-process.md`  
  发布前检查、发布步骤、回滚策略和复盘要求。

- `docs/architecture-checklist.md`  
  架构评审清单，用于确保设计一致性、可维护性和稳定性。

### 2.5 运行和运维

- `docs/operation-guide.md`  
  环境准备、构建、启动、日志和故障排查。

## 3. 阅读顺序建议

推荐阅读顺序：

1. `docs/requirements.md`
2. `docs/system-design.md`
3. `docs/state-machine-design.md`
4. `docs/implementation-plan.md`
5. `docs/operation-guide.md`
6. `docs/coding-standards.md`
7. `docs/testing-strategy.md`
8. `docs/release-process.md`

## 4. 文档使用建议

- 新成员入门：先看 `requirements` + `system-design`
- 代码评审：看 `coding-standards` + `architecture-checklist`
- 生产部署：看 `operation-guide` + `release-process`
- 故障排查：看 `state-machine-design` + `operation-guide`

## 5. 当前项目状态说明

当前仓库已形成模块化的工程骨架，重点涵盖：

- DPDK 网卡初始化
- mbuf / ring / channel 架构
- IndustrialHeader 与协议解析
- ChannelStateMachine
- WAL / metrics / run-time stats
- 工程文档和规范说明

尚处于工程骨架 + 设计完善阶段，正式工业环境适配仍需结合真实网卡、真实 DPDK 版本和实际协议定义逐步收口。

## 6. 结论

这个文档集的目标，是把项目从“零散代码 + 经验描述”收敛成“可理解、可维护、可交付”的工程说明体系。
