# DPDK_Test 运维手册

## 1. 适用场景

本手册适用于 DPDK 版本、网卡绑定、hugepage 配置、程序启动和故障定位。

## 2. 环境准备

### 2.1 hugepage

执行：

```bash
sudo ./scripts/setup_hugepages.sh
```

### 2.2 网卡绑定

执行：

```bash
sudo ./scripts/bind_nics.sh
```

注意：

- 需要根据真实 PCI 号替换脚本中的地址
- 需确保对应网卡驱动支持 DPDK

## 3. 构建项目

```bash
mkdir -p build
cd build
cmake -DDPDK_ROOT="/path/to/dpdk" ..
make -j$(nproc)
```

## 4. 启动方式

### 4.1 主程序

```bash
./rx_primary
```

### 4.2 worker

```bash
./channel_worker 0
./channel_worker 1
./channel_worker 11
```

## 5. 运行检查

启动后，建议查看：

- 日志输出
- total_rx_frames
- total_dropped
- total_crc_errors
- per-channel 状态

如果状态不正常，应检查：

- hugepage 是否正常
- NIC 是否绑定正确
- ring 是否创建成功
- DPDK EAL 是否初始化成功

## 6. 故障排查

### 6.1 编译失败

优先检查：

- DPDK 路径是否正确
- `rte_eal.h` 是否存在
- `librte_*` 库是否正确匹配版本

### 6.2 启动失败

检查：

- hugepage 可用性
- 网卡是否被其他驱动占用
- 端口是否支持当前 PMD

### 6.3 丢包异常

检查：

- sequence 是否断裂
- `dropped_frames` 是否持续增长
- 是否进入 Paused 或 Recovering 状态
- ring 是否被填满

### 6.4 WAL 异常

检查：

- `/data/industrial_rx/wal` 是否可写
- 磁盘空间是否充足
- 文件权限是否正确

## 7. 日志建议

建议记录以下日志：

- EAL 初始化日志
- port setup 日志
- ring 创建日志
- tx/rx burst 统计
- timeout / recover / paused 状态切换
- WAL 写失败日志

## 8. 生产部署建议

- 在生产环境中尽量固定 DPDK 版本
- 使用脚本统一绑定网卡与 hugepage
- 将状态输出接到监控系统
- 对 WAL 做周期性归档
- 对 core dump 和 error 日志做保留

## 9. 结论

本运维手册的核心目的是保证系统能稳定地：

- 启动
- 运行
- 观察状态
- 诊断故障
- 恢复和维护

工程上，运维能力与可靠性设计同等重要。