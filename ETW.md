# WindowsPerf support for Event Tracing for Windows (ETW)

[[_TOC_]]

# Event Tracing for Windows (ETW)

[Event Tracing for Windows (ETW)](https://learn.microsoft.com/en-us/windows-hardware/drivers/devtest/event-tracing-for-windows--etw-) provides a mechanism to trace and log events that are raised by user-mode applications and kernel-mode drivers. ETW is implemented in the Windows operating system and provides developers a fast, reliable, and versatile set of event tracing features.

# WindowsPerf ETW architecture



## Windows Performance Analyzer

[Windows Performance Analyzer](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/windows-performance-analyzer) is a tool that creates graphs and data tables of Event Tracing for Windows (ETW) events that are recorded by Windows Performance Recorder (WPR), Xperf and WindowsPerf via [WPA-plugin-etl](https://gitlab.com/Linaro/WindowsPerf/wpa-plugin-etl).

# WPA-plugin-etl

The [WPA-plugin-etl](https://gitlab.com/Linaro/WindowsPerf/wpa-plugin-etl is a dedicated plugin developed for the Windows Performance Analyzer (WPA). Its primary function is to interpret and present event traces that have been injected by the WindowsPerf ETW (Event Tracing for Windows). These events can be injected through two main sources: the [wperf](https://gitlab.com/Linaro/WindowsPerf/windowsperf/-/tree/main/wperf?ref_type=heads) application and the [wperf-driver](https://gitlab.com/Linaro/WindowsPerf/windowsperf/-/tree/main/wperf-driver?ref_type=heads). The `wperf` application is a user-mode application, while the `wperf-driver` is a Windows Kernel Driver. 

Together, they provide a comprehensive view of system performance and behavior, making the `WPA-plugin-etl` a valuable tool for system analysis and debugging. This plugin enhances the capabilities of WPA, allowing users to delve deeper into the Arm core and uncore PMU performance characteristics of their Windows on Arm systems. It’s an essential tool for anyone looking to optimize system performance or troubleshoot issues.
