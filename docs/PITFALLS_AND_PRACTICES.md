# 项目实践与坑点速查（汇总导航）

此文为汇总导航，详细内容已拆分到以下专题文档：

- 语言层面坑点与最佳实践: [PITFALLS_LANGUAGE.md](./PITFALLS_LANGUAGE.md)
- 代码设计与接口约定: [PITFALLS_DESIGN.md](./PITFALLS_DESIGN.md)
- 架构与运行机制（epoll/事件循环）: [PITFALLS_ARCHITECTURE.md](./PITFALLS_ARCHITECTURE.md)
- 构建与工具链（CMake/clang-*）: [BUILD_AND_TOOLING.md](./BUILD_AND_TOOLING.md)
- 常用 Checklist（启动/accept/读写/关闭）: [CHECKLISTS.md](./CHECKLISTS.md)
- 故障排查（症状 → 原因 → 解决）: [TROUBLESHOOTING.md](./TROUBLESHOOTING.md)
- errno 参考速查: [ERRNO_REFERENCE.md](./ERRNO_REFERENCE.md)
- 日志系统使用: [LOGGING.md](./LOGGING.md)

> 说明：以上内容聚焦当前实现（不含 `src/old` 与 `docs/old`），适合作为复盘与查阅手册。
