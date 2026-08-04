# FEMU 对接 UBsim — FTL Core 抽取与 UBSIM 适配器

## 文件说明

| 文件 | 说明 |
|------|------|
| `femu_direct_adapter.h` | 适配器 C++ 头文件, 继承 `SimulatorModule` |
| `femu_direct_adapter.cc` | 适配器 C++ 实现 (SimulatorModule 接口 + I/O 路径) |
| `build.sh` | 构建自动化脚本, 提供一键构建功能 |
| `CMakeLists.txt` | CMake 构建配置 (适配器 + libfemu_ftl + 单元测试) |
| `libfemu_ftl/femu_ftl_api.h` | FTL Core 公开 C API |
| `libfemu_ftl/femu_ftl.c` | FTL 核心实现 (读/写/Trim/GC/映射表) |
| `libfemu_ftl/femu_ftl_types.h` | FTL 数据结构定义 |
| `libfemu_ftl/femu_ftl_timing.c` | NAND 时序推进模型 |
| `libfemu_ftl/femu_ftl_nand.c` | NAND 闪存延迟参数表 |
| `libfemu_ftl/femu_ftl_backend_dram.c` | DRAM 数据后端实现 |
| `libfemu_ftl/femu_ftl_internal.h` | FTL 内部函数声明 |
| `libfemu_ftl/test_femu_ftl.c` | FTL Core 单元测试 (6 套件) |
| `libfemu_ftl/CMakeLists.txt` | FTL Core 构建配置 |
| `femu-adapter-deadlock-analysis.md` | memstim ↔ femu 死锁根因分析与配置方案 (link_latency/sync_interval=100000) |

---

## 背景

将 FEMU 中 BBSSD (Black-Box SSD) 模式下的 FTL 核心 (`hw/femu/bbssd/ftl.c`) 抽
取为零依赖的纯 C 静态库 `libfemu_ftl.a`, 消除对 QEMU 基础设施 (g_malloc, QTAILQ,
rte_ring, QemuThread) 的依赖, 使 FTL 可作为 Ramulator2 风格的同步库被
UBSIM SimulatorModule 直接调用.

---

## 适配器设计思路

FEMU FTL 适配器的实现参照了 **Ramulator2 适配器** (`sims/ramulator2_adapter/`)
和 **UBSim 仿真架构接口文档** (`docs/simulators/ramulator2.md`,
`sims/femu_adapter/UBSim仿真架构接口文档.md`).

### 架构对比: Ramulator2 vs FEMU

```
Ramulator2 Adapter                          FEMU FTL Adapter
┌───────────────────────┐                  ┌───────────────────────┐
│ SimulatorModule       │                  │ SimulatorModule       │
│  ├─ onMessage()       │                  │  ├─ onMessage()       │
│  ├─ advanceTo()       │    相同模式       │  ├─ advanceTo()       │
│  ├─ getCurrentTime()  │ ◄───────►        │  ├─ getCurrentTime()  │
│  └─ run() loop        │                  │  └─ run() loop        │
│     while (!term)     │                  │     while (!term)     │
│       tick();         │                  │       tick();         │
├───────────────────────┤                  ├───────────────────────┤
│ Ramulator2 Core       │                  │ libfemu_ftl.a         │
│  ├─ DDR4/HBM timing   │                  │  ├─ ssd_read/write    │
│  ├─ FRFCFS scheduler  │    都是同步库     │  ├─ GC / Trim        │
│  └─ YAML config       │  ◄───────►       │  └─ FemuFtlConfig     │
└───────────────────────┘                  └───────────────────────┘
```

### SimulatorModule 接口实现对照

FEMU 适配器实现了 `SimulatorModule` 基类的全部三个纯虚函数, 并正确使用了框架
提供的所有公共方法:

| 接口 | 类别 (按接口文档) | 实现位置 | 说明 |
|------|------------------|---------|------|
| `advanceTo(localTime)` | 仿真器实现 | `femu_direct_adapter.cc:76` | 存储 `currentTs_ = nextTs`, 框架在 `tick()` 内部自动调用 |
| `onMessage(msg, port)` | 仿真器实现 | `femu_direct_adapter.cc:51` | 按消息类型分派: kReadReq → handleReadReq, kWriteReq → handleWriteReq |
| `getCurrentTime()` | 仿真器实现 | `femu_direct_adapter.cc:71` | 返回 `currentTs_` |
| `send(msg, delay, port)` | 框架提供 | 继承自基类 | 在响应完成后调用, 发回响应消息 |
| `init()` | 框架提供 | `run()` 中调用 | 从 `UBSIM_PORTS` 环境变量加载端口配置 |
| `tick()` | 框架提供 | `run()` 主循环中调用 | 轮询端口、接收消息、释放到期消息、调用 `advanceTo()` |
| `safeLocalTime()` | 框架提供 | 继承自基类 | 框架内部在 `tick()` 中自动使用, 适配器不需要显式调用 |
| `isTerminated()` | 框架提供 | 继承自基类 | 主循环退出的判断条件 |

### 适配器工作流程

```
SimulatorModule 生命周期
═══════════════════════

1. 构造 FemuDirectAdapter
   └─ 设置 FemuFtlConfig 默认值 (SSD 几何参数、延迟参数、GC 参数)
   └─ 记录 `configPath_` (如果提供了 `--config-file`)

2. run(argc, argv)
   ├─ parseArgs(): 解析命令行参数 (--config-file, --help)
   ├─ loadConfigFromYaml(): 从 YAML 文件加载/覆盖 FTL 配置 (若无 --config-file 则使用代码默认值)
   ├─ femu_ftl_init(&cfg): 初始化 FTL Core
   ├─ init(): SimulatorModule 基类, 从 UBSIM_PORTS 加载端口
   └─ while (!isTerminated()) { tick(); }

3. tick() [基类实现]
   ├─ 轮询端口, 接收外部消息
   ├─ 按 safeTime 推进本地时间 → advanceTo(localTime)
   ├─ 释放到期消息 → onMessage(msg, portIndex)
   └─ 发送 SYNC/TERMINATE

4. onMessage(msg, portIndex) [适配器实现]
   ├─ kReadReq  → handleReadReq(msg)
   │    ├─ slba = msg->address / secsz
   │    ├─ femu_ftl_read(ftl, slba, nlb, buf, &latency_ns)
   │    ├─ latency_ps = latency_ns * 1000  // ns → ps 转换
   │    ├─ doneTs = currentTs_ + latency_ps
   │    ├─ buildResponse(...) → allocateSendBuffer(doneTs, port)
   │    └─ send(respPkt, 0, 0)
   ├─ kWriteReq → handleWriteReq(msg)  [同上模式]
   └─ kWritePosted → handleWritePosted(msg)  [无响应消息]
```

### 内存系统仿真器模式

FEMU FTL 适配器属于 **内存系统仿真器**:

> 内存系统仿真器中的 SimulatorModule 接入通常比较直接:
>
> - **入口**: 在 `onMessage(msg)` 中把统一消息转换成本地内存访问请求
> - **完成回调**: 当内存系统仿真器完成该请求时, 构造响应消息并调用 `send(msg)`
> - **时间接口**: 用 `advanceTo(localTime)` 驱动内存系统向前执行, 用 `getCurrentTime()` 返回当前本地时间
>
> 内存系统侧的 Adapter 更像是"统一消息格式"和"本地内存请求格式"之间的转换器.

### 与接口文档的差异说明

| 文档描述 | 实际代码 | 原因 |
|---------|---------|------|
| `init(config)` 带参数 | `init()` 无参数, 配置从 `UBSIM_PORTS` 环境变量读取 | 框架版本演进, 实际基类为无参版本 |
| `onMessage(const Message&)` | `onMessage(UBMemMessage*, size_t)` | 实际基类使用指针 + 端口索引 |
| `send(msg, delay, targetOverride)` | `send(UBMemMessage*, delay, portIndex)` | 同上 |

适配器遵循的是代码库中 `SimulatorModule` 的实际基类定义
(`src/adapter/port/simulator_module.h`).

### 适配器中被隔离/修改的行为

与原生 FEMU BBSSD 模式相比, 适配器对 FTL Core 做了以下调整以适配 UBSIM:

1. **时间单位转换**: FTL 内部延迟使用纳秒, UBSIM 全局时间线使用皮秒.
   适配器在 `handleReadReq`/`handleWriteReq` 中做 `latency_ps = latency_ns * 1000`.
2. **同步单线程模型**: 原生 FEMU 使用 QemuThread + rte_ring 支持并发 I/O 提交;
   适配器中 FTL Core 以同步方式单线程调用, 由 UBSIM 框架负责并发.
3. **无 NVMe/PCIe 设备层**: 适配器不包含 QEMU 设备仿真, UBSIM 消息的 address
   直接被转换为 LBA (`slba = address / secsz`).
4. **无 FDP 支持**: 原生 FEMU 支持 Flexible Data Placement, 适配器中未实现.

---

## 环境依赖

### UBSIM 依赖

确保 UBSIM 项目已正确编译:

```bash
./script/build_and_test.sh
```

构建时会自动从项目根目录查找:
- 头文件: `$ROOT_PATH/src`, `$ROOT_PATH/sims/common`
- 共享库: `$ROOT_PATH/bin/libubsim.so`

## 快速开始

### 步骤 1: 一键构建

在项目根目录下执行:

```bash
bash sims/femu_adapter/build.sh
```

`build.sh` 包含以下步骤:
1. 配置 CMake (Release 模式, `-DROOT_PATH=$ROOT_PATH`)
2. 编译 `libfemu_ftl.a` 静态库
3. 编译 `test_femu_ftl` 单元测试并运行 (所有测试通过后才复制二进制)
4. 编译 `femu_direct_adapter` 并复制到 `bin/` 目录

### 步骤 2: 运行单元测试

```bash
./sims/femu_adapter/build/libfemu_ftl/test_femu_ftl
```

输出示例:
```
=== 1. Basic I/O Integrity ===
...
=== 6. Trim ===

========================================
  Tests:  794663 passed, 0 failed, 794663 assertions
  Total:  794663 checks
========================================
```

### 步骤 3: 运行集成测试

FEMU SSD 适配器提供了一个 MemStim → FTL 的完整端到端集成测试:

```bash
bash example/femu-ssd/run_integration_test.sh
```

该测试启动两个进程:
- `femu_direct_adapter` (bind 模式): 作为 ZMQ 服务端等待连接
- `memstim` (connect 模式): 发送 32 条 I/O 操作 (顺序写/读, 覆盖写, 随机偏移, 边界地址)

验证标准: memstim 退出码为 0, 且日志中出现 `all operations completed`.

### 步骤 4: 清理构建产物

```bash
bash sims/femu_adapter/build.sh clean
```

或者手动清理:

```bash
rm -rf sims/femu_adapter/build
```

---

## 配置说明

### 启动参数

FEMU 适配器支持以下参数:

| 参数 | 说明 |
|------|------|
| `--config-file <path>` | YAML 配置文件路径 (如 Ramulator2 的 `--config-file`) |
| `--help`, `-h` | 显示帮助信息 |

### 配置方式

适配器支持两级配置 (优先级: YAML 文件 > 代码默认值):

#### 1. 代码默认值 (在构造函数中设置)

```cpp
ftl_cfg_.nchs              = 8;
ftl_cfg_.luns_per_ch       = 8;
ftl_cfg_.pgs_per_blk       = 256;
ftl_cfg_.blks_per_pl       = 256;
ftl_cfg_.pg_rd_lat         = 45000;     // ns, TLC
ftl_cfg_.pg_wr_lat         = 150000;    // ns, TLC
ftl_cfg_.gc_thres_pcent    = 0.75;
ftl_cfg_.enable_gc_delay   = true;
```

#### 2. YAML 配置文件 (`--config-file`)

通过 `--config-file` 指定 YAML 配置文件, 支持三个节:

- **`geometry`**: SSD 几何参数
- **`timing`**: NAND 延迟参数
- **`ftl`**: GC 行为参数

缺失的节或键自动使用代码默认值.

```yaml
# example_config.yaml
geometry:
  num_channels: 8
  luns_per_channel: 8
  planes_per_lun: 1
  blocks_per_plane: 256
  pages_per_block: 256
  sectors_per_page: 1

timing:
  read_latency_ns: 45000
  write_latency_ns: 150000
  erase_latency_ns: 3800000

ftl:
  enable_gc_delay: true
  gc_threshold_percent: 0.75    # >1.0 自动除以 100
  gc_threshold_lines: 5
  gc_threshold_lines_high: 40
```

启动示例:

```bash
femu_direct_adapter --config-file sims/femu_adapter/example_config.yaml
```

完整的可配置项见 `femu_ftl_types.h:222-242` 的 `struct FemuFtlConfig`.

### UBSIM 端口配置

适配器通过 `UBSIM_PORTS` 环境变量接收 ZMQ 连接参数, 由 Python 配置编译器自动
生成. 手动运行时示例:

```bash
export UBSIM_PORTS="name=mem-0,\
  endpoint=ipc:///tmp/ssd.sock,\
  mode=bind,queueEntryBytes=8192,queueEntries=8192,\
  linkLatency=100000,syncInterval=100000,syncMode=required"
```

注意:
- `linkLatency` 必须 `>= syncInterval`, 否则 `UBMemPort` 初始化会报错
  (`latency must be larger or equal to sync interval`).
- **`linkLatency = syncInterval = 100000` (100μs) 是规避 memstim ↔ femu
  死锁的推荐值**, 详见下文「集成测试过程发现的问题」第 4 条. 使用
  50000 (50μs) 在 `simulate.py` 全流程场景下会因 ZMQ 高水位背压死锁.

---

## 集成测试

### 测试文件

| 文件 | 说明 |
|------|------|
| `sims/femu_adapter/libfemu_ftl/test_femu_ftl.c` | FTL Core 单元测试 (6 套件, ~560 行) |
| `example/femu-ssd/ubsim_config.json` | UBSIM 拓扑配置 (memstim → femu_direct_adapter) |
| `example/femu-ssd/memstim_input.csv` | MemStim 工作负载 trace (32 条 I/O 操作) |
| `example/femu-ssd/run_integration_test.sh` | 端到端集成测试脚本 |

### 单元测试套件

| 套件 | 测试函数 | 覆盖内容 |
|------|----------|----------|
| 1. Basic I/O Integrity | `test_single_rw` | 单 LBA 写 + 回读数据一致性 |
| | `test_read_unwritten` | 未写 LBA 读返回全零 |
| | `test_multi_lba_rw` | 连续 8 LBA 读写一致性 |
| | `test_overwrite` | 同 LBA 覆盖写, 新数据替换旧数据 |
| | `test_pattern_wr_fill` | 64 个 LBA 不同 pattern, 逐一回读校验 |
| 2. Timing Model | `test_single_latency` | 单页读延迟 = pg_rd_lat, 写延迟 = pg_wr_lat |
| | `test_multi_page_sequential` | 多页延迟逐页累加正确 |
| | `test_time_monotonic` | `current_time` 按延迟严格单调递增 |
| 3. Mapping Table | `test_mapping_update` | maptbl 写后非 UNMAPPED; rmap 正反查询一致 |
| 4. Garbage Collection | `test_gc_trigger_and_integrity` | GC 触发后 free 线数变化; 64 LBA 数据完整 |
| | `test_gc_prevent_data_loss` | 覆盖写触发 GC 后新旧数据均正确 |
| | `test_gc_line_accounting` | free/full/victim 三类线计数总和 = 总线数 |
| 5. Write Pointer | `test_write_pointer_advance` | 4 次写入 striping 到 ch 0→1→2→3 |
| 6. Trim | `test_trim` | Trim 后读回全零; maptbl 重置为 UNMAPPED |

### 集成测试场景

集成测试使用 MemStim 流量发生器向 FEMU FTL 适配器发送 32 条 I/O 操作, 涵盖六
个阶段:

1. **顺序写 (8 次)**: 1 MB 间隔, 4 KB 对齐, 全部在时间 0 发射
2. **读取验证 (8 次)**: 验证写入的数据可正确读回
3. **覆盖写 + 读 (6 次)**: 部分地址重复写入后读取, 验证新数据替换旧数据
4. **随机偏移 (6 次)**: 新的地址区间写入后读回
5. **边界地址 (4 次)**: LBA 0 (首扇区) 和 LBA ~1 GB 处 (近容量上限)

### 集成测试过程发现的问题

在首次运行集成测试时发现并修复了以下问题:

1. **ns→ps 时间单位丢失** (`femu_direct_adapter.cc:96,132`): FTL 返回的延迟是
   纳秒, 但 UBSIM 全局时间线使用皮秒. `doneTs = currentTs_ + latency_ns` 导致
   SSD 延迟比预期快了 1000 倍. 修复为 `latency_ps = latency_ns * 1000`.

2. **LogInfo format string 参数计数不匹配** (`femu_direct_adapter.cc:284`):
   `"FTL initialized: {} ch x {} lun x ... ({} secsz={})"` 有 8 个 `{}` 但只传入
   7 个参数, 触发 `fmt::format_error`. 修复为 `"FTL initialized: {} ch x {}
   lun x ... sec/secsz={}"`.

3. **linkLatency >= syncInterval**: 首次配置设 `linkLatency=10000,
   syncInterval=50000`, 触发 `UBMemPort` 初始化错误 `latency must be larger or
   equal to sync interval`. 修复为两者一致.

4. **memstim → femu 死锁 (ZMQ 高水位背压)**: 在 `simulate.py` 全流程下,
   `linkLatency=syncInterval=50000` + 32 ops trace 会导致死锁
   (memstim 卡 `zmq_ctx_term`, femu 卡 `zmq_send`). 根因是 femu 响应时间戳
   `doneTs = currentTs_ + latency` (读 45μs/写 150μs) 将 memstim 时间带飞到
   156.1ms, 而 femu 自身仅推进到 98.4ms; memstim 完成 ops 后停止接收,
   femu 追赶需发送 1154 条 SYNC (> ZMQ PAIR 默认 HWM 1000), `zmq_send`
   永久阻塞 → 死锁. 详细分析见
   `sims/femu_adapter/femu-adapter-deadlock-analysis.md`.
   **最终采用配置方案解决**: `linkLatency = syncInterval = 100000`
   (追赶周期数 577 < HWM 1000), 代码保持原版. 实验对照:

   | linkLatency / syncInterval | 追赶周期数 | 结果 |
   |---|---|---|
   | 50000 / 50000 | 1154 > 1000 | ❌ rc=124 卡死 |
   | 100000 / 100000 (当前 example 配置) | 577 < 1000 | ✅ rc=0 |
   | 200000 / 200000 | 288 < 1000 | ✅ rc=0 |

   注: `example/femu-ssd/run_integration_test.sh` 手动启动场景中
   `UBSIM_PORTS` 仍使用 50000, 实测可通过——该场景 memstim 完成 ops 后
   立即析构、脚本随后直接 kill femu, 没有框架优雅停止的窗口期, 不会
   累积满 HWM; 但为与 example 配置保持一致, 建议同步改为 100000.

---

## FTL Core 功能等价性分析

抽取后的 `libfemu_ftl` 与原始 FEMU BBSSD FTL 在 **核心 I/O 路径上功能等价**:
ssd_read/ssd_write/ssd_trim 的语义、写时复制 (redirect-on-write)、LPN→PPA 映射
表维护、反向映射表维护、写指针 striping 算法完全一致.

### 等价的部分

| 功能 | 原始 FEMU | 抽取后 | 等价? |
|------|-----------|--------|-------|
| 读写路径 | ssd_read/ssd_write | ssd_read/ssd_write | **完全一致** |
| 映射表 | maptbl[lpn] = ppa | maptbl[lpn] = ppa | **完全一致** |
| 反向映射 | rmap[pgidx] = lpn | rmap[pgidx] = lpn | **完全一致** |
| 写指针 striping | ch→lun→pg→blk | ch→lun→pg→blk | **完全一致** |
| 未映射读行为 | 跳过, 不填充 buffer | 填充零 | **改进** |
| DRAM 后端 | SsdDramBackend | ssd_dram_backend | **等价** |
| 闪存延迟参数 | NAND_SLC/MLC/TLC/QLC | 相同枚举 + 查表 | **完全一致** |
| 时序推进 | per-LUN next_lun_avail_time | 相同 | **完全一致** |
| GC 触发条件 | free_line_cnt ≤ gc_thres_lines | 相同 | **完全一致** |
| GC 数据搬迁 | 读取 valid 页 → 写入新 PPA → 擦除 | 相同 | **完全一致** |
| 数据布局 | secs_per_pg sectors/page | 相同 | **完全一致** |
| 设备容量计算 | tt_secs = nchs × ... | 相同公式 | **完全一致** |
| Trim | 清 maptbl + invalidate | 相同 | **完全一致** |

### 差异的部分

| 功能 | 原始 FEMU | 抽取后 | 原因 |
|------|-----------|--------|------|
| **Line/Superblock** | `blks_per_line = tt_luns` | `blks_per_line = 1` | 简化为单 block 粒度, superblock 语义对 UBSIM 非必需 |
| **Victim 选择** | `pqueue` (O(log n)) + IPC 阈值 | 线性扫描最小 VPC, 无 IPC 阈值 | pqueue 依赖 QEMU 动态分配; 小规模下线性扫描可接受 |
| **通道延迟** | 仅 ocssd 模式实现 `advance_channel_timestamp`, 且实现为 `return now` (TODO 死代码) | 已删除 (`femu_timing_advance_channel`, `ch_xfer_lat`, `channel_xfer_ns` 全部移除) | 原版 BBSSD 根本不回调通道时序, 配置项从未生效, 删除以避免误导 |
| **GC 延迟** | `ssd_advance_status()` | `femu_timing_advance_chip()` | 语义等价, 实现简化 |
| **FDP** | 完整 FDP 支持 | 未实现 | 后续扩展 |
| **线程安全** | QemuThread + rte_ring | 同步单线程 | 有意的 Ramulator2 风格设计 |
| **配置接口** | QEMU `-device` 参数 | `struct FemuFtlConfig` | 接口形式不同, 语义等价 |

### 测试过程中修复的问题

1. **未映射读零填充** — 原始 `ssd_read` 遇到 UNMAPPED_PPA 直接跳过, 不写 buffer.
   在 QEMU 中 buffer 由 `pci_dma_read` 预先清零; 抽取后无此保证, 故显式 memset.
2. **Line 生命周期管理** — 原始实现依赖 QTAILQ + pqueue, 提取后用
   `ftl_list_entry` 链表替代, 需要明确在 pg-wrap 点实现 line 状态机转换.
3. **`mark_page_invalid` 边界处理** — free list 中部分填充的 line 被 invalidate 时
   应留在 free list, 而非错误移至 victim list.
4. **GC 数据搬迁** — `gc_write_page` 需要实际拷贝数据、更新映射表并推进写指针.

> **结论**: 抽取后的 FTL core 在核心数据通路语义上与原始 FEMU **功能等价**,
> 差异主要体现在 line/superblock 结构和 GC 调优策略上, 这些是设计时有意为之的
> 简化取舍而非缺陷.

---

## 配置项对照

### 原始 FEMU (BBSSD 模式)

原始 FEMU 的 SSD 配置通过 QEMU device property 系统传入, 定义在
`hw/femu/femu.c:954-1035` 的 `femu_props[]` 数组中:

| 配置项 | QEMU 属性名 | 默认值 | 说明 |
|--------|-------------|--------|------|
| 扇区大小 | `secsz` | 512 | 字节 |
| 每页扇区数 | `secs_per_pg` | 8 | |
| 每块页数 | `pgs_per_blk` | 256 | |
| 每 Plane 块数 | `blks_per_pl` | 256 | |
| 每 LUN Plane 数 | `pls_per_lun` | 1 | |
| 每通道 LUN 数 | `luns_per_ch` | 8 | |
| 通道数 | `nchs` | 8 | |
| 页读延迟 | `pg_rd_lat` | 40000 | 纳秒 |
| 页写延迟 | `pg_wr_lat` | 200000 | 纳秒 |
| 块擦除延迟 | `blk_er_lat` | 2000000 | 纳秒 |
| GC 低阈值 (%) | `gc_thres_pcent` | 75 | |

### 抽取后 `libfemu_ftl` + `femu_direct_adapter`

```c
struct FemuFtlConfig {
    uint64_t capacity_bytes;   // 设备总容量 (字节)
    int secsz;                 // 扇区大小 (默认 4096)
    int secs_per_pg;           // 每页扇区数 (默认 1)
    int pgs_per_blk;           // 每块页数 (默认 256)
    int blks_per_pl;           // 每 Plane 块数 (默认 256)
    int pls_per_lun;           // 每 LUN Plane 数 (默认 1)
    int luns_per_ch;           // 每通道 LUN 数 (默认 8)
    int nchs;                  // 通道数 (默认 8)

    int pg_rd_lat;             // 页读延迟, 纳秒 (默认 45000, TLC)
    int pg_wr_lat;             // 页写延迟, 纳秒 (默认 150000, TLC)
    int blk_er_lat;            // 块擦除延迟, 纳秒 (默认 3800000, TLC)

    double gc_thres_pcent;     // GC 低阈值 (默认 0.75)
    int    gc_thres_lines;     // GC 低阈值 (绝对线数, 默认 5)
    bool   enable_gc_delay;    // 是否计入 GC 延迟 (默认 true)
};
```

> 参见 `femu_ftl_types.h:222-242` 和 `femu_direct_adapter.cc:16-37`.

### 配置接口变更说明

| 版本 | 配置方式 | 示例 |
|------|---------|------|
| 原始 FEMU (QEMU) | `-device femu-device,secs_per_pg=8,...` | QEMU device property |
| 旧版适配器 (已移除) | 环境变量 `FEMU_FTL_*` | `export FEMU_FTL_NCHS=4` |
| **当前适配器** | **YAML `--config-file`** (参照 Ramulator2) | `femu_direct_adapter --config-file cfg.yaml` |

移除了环境变量覆盖方式, 统一使用 YAML 配置文件, 与 Ramulator2 适配器的配置风格一致.

---

## 常用命令

| 命令 | 说明 |
|------|------|
| `bash sims/femu_adapter/build.sh` | 一键构建 (编译 + 单元测试 + 复制二进制) |
| `bash sims/femu_adapter/build.sh clean` | 清理构建产物 |
| `./sims/femu_adapter/build/libfemu_ftl/test_femu_ftl` | 运行 FTL Core 单元测试 |
| `bash example/femu-ssd/run_integration_test.sh` | 运行端到端集成测试 |
| `python3 simulate.py -i example/femu-ssd/ubsim_config.json` | 通过 UBSIM Python 框架运行 (需要 paramiko) |

## 参考文档

- `sims/femu_adapter/UBSim仿真架构接口文档.md` — UBSIM Adapter 架构与接口定义
- `sims/femu_adapter/femu-adapter-deadlock-analysis.md` — memstim ↔ femu 死锁根因分析与配置方案
- `docs/simulators/ramulator2.md` — Ramulator2 适配器文档 (FEMU 适配器参考模板)
- `docs/simulators/femu.md` — FEMU FTL 适配器概览文档
- `src/adapter/port/simulator_module.h` — SimulatorModule 基类定义
- `sims/femu_adapter/libfemu_ftl/femu_ftl_api.h` — FTL Core C API
