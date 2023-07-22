# proj227-Lightweight-resource-isolation-technology-based-on-eBPF

## 题目 设计实现基于eBPF的轻量级资源隔离技术

## 学校：华中科技大学

## 队名：nopnop 队长：王与峥 队员：王与峥，朱晔，姜昊哲 指导教师：樊浩，邵志远

## 项目简介

我们设计实现了Esolation，这是一款serverless框架下，基于eBPF和wasm的轻量级、用户可定义的CPU、network、IO、Memory资源隔离机制和内存共享机制，
目标为降低serverless中函数运行容器的初始化开销，提高高并发下函数沙箱的启动速度。



## 赛题目标

· 目标一：实验测试现有cgroup、namespace机制的并发性能 <br />
· 目标二：基于eBPF实现对disk io、network io资源的约束 <br />
· 目标三：基于eBPF实现对CPU、Memory资源的约束



## 项目进度

· 测试了单独启动 cgroup 的平均创建时间，测试了使用 crun 创建 docker 容器在有无cgroup 的情形下的平均创建时间<br />
· 对 disk 和 network 的 io 路径进行了深入分析，形成了对 disk 和 network 的 eBPF 隔离方案<br />
· 已实现 CPU 资源按比例约束、容器绑定单个 CPU、优化运行过程中约束 CPU 资源的开销，实现内存资源隔离，实验验证了 eBPF 共享内存性能远优于 Linux shm



## 项目贡献

我们基于wasm容器作为serverless沙箱搭建平台，利用eBPF技术，实现了名为 Esolation 的一种轻量级的、无侵入的、平台可定义的、满足无感知计算平台隔离需求的资源隔离机制。
Esolation利用wasm实例独立的线性内存，保证了内存的天然隔离，并显著降低了平台容器启动的时间开销；引入了新的eBPF程序类型与辅助函数，在Linux的调度器中设置了一系列覆盖
容器生命周期的eBPF挂载点以动态执行隔离策略，通过BPF程序中轻量的数据结构设计，在不改变原有调度逻辑的情况下，于内核调度器中完成了对cgroup机制的对标与优化；提出了利用
eBPF进行网络虫洞加速与穿越overlay网络的初步设计；验证了利用eBPF可以完成高效的容器内存共享以减少容器通信开销的方法；设计了利用eBPF在通用块设备层实现按比例分享和IO
带宽限制的方案。



## 代码文件说明

我们的开发的代码以及修改的Linux源码请访问Github链接：https://github.com/LancelottJ/proj227-nopnop-code-repository<br />
完整的Linux内核源码在： https://github.com/Andrew12138-w/linux
