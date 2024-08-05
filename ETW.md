# WindowsPerf support for Event Tracing for Windows (ETW)

[[_TOC_]]

# Event Tracing for Windows (ETW)

[Event Tracing for Windows (ETW)](https://learn.microsoft.com/en-us/windows-hardware/drivers/devtest/event-tracing-for-windows--etw-) provides a mechanism to trace and log events that are raised by user-mode applications and kernel-mode drivers. ETW is implemented in the Windows operating system and provides developers a fast, reliable, and versatile set of event tracing features.

Microsoft Event Tracing for Windows (ETW) is a high-speed tracing facility built into the Windows operating system. It provides a mechanism to trace and log events raised by both user-mode applications and kernel-mode driver. Using a buffering and logging mechanism implemented in the operating system kernel, ETW offers developers a fast, reliable, and versatile set of event tracing features. It can be used for system and application diagnosis, troubleshooting, and performance monitoring.

The architecture of Event Tracing for Windows (ETW) is designed to be robust, dynamic, and lightweight. It involves `event providers`, which are software components instrumented for ETW to report critical errors and other important events. These providers register with ETW and raise corresponding events when they encounter an error condition or other important execution state. The events are initially written to an ETW Session, which can deliver the event data live to a consumer or log it for later processing and analysis. An ETW Controller starts and stops ETW Sessions and dynamically enables providers.

WindowsPerf raises corresponding PMU events from `wperf` application and `wperf-driver`. These can be recorded (and saved) with [Windows Performance Recorder (WPR)](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/windows-performance-recorder)  and analyzed using [Windows Performance Analyzer](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/windows-performance-analyzer) with installed `WPA-plugin-etl`.

## Windows Performance Analyzer

[Windows Performance Analyzer](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/windows-performance-analyzer) is a tool that creates graphs and data tables of Event Tracing for Windows (ETW) events that are recorded by Windows Performance Recorder (WPR), Xperf and WindowsPerf via [WPA-plugin-etl](https://gitlab.com/Linaro/WindowsPerf/wpa-plugin-etl).

# WindowsPerf ETW architecture

# WPA-plugin-etl

The [WPA-plugin-etl](https://gitlab.com/Linaro/WindowsPerf/wpa-plugin-etl is a dedicated plugin developed for the Windows Performance Analyzer (WPA). Its primary function is to interpret and present event traces that have been injected by the WindowsPerf ETW (Event Tracing for Windows). These events can be injected through two main sources: the [wperf](https://gitlab.com/Linaro/WindowsPerf/windowsperf/-/tree/main/wperf?ref_type=heads) application and the [wperf-driver](https://gitlab.com/Linaro/WindowsPerf/windowsperf/-/tree/main/wperf-driver?ref_type=heads). The `wperf` application is a user-mode application, while the `wperf-driver` is a Windows Kernel Driver. 

Together, they provide a comprehensive view of system performance and behavior, making the `WPA-plugin-etl` a valuable tool for system analysis and debugging. This plugin enhances the capabilities of WPA, allowing users to delve deeper into the Arm core and uncore PMU performance characteristics of their Windows on Arm systems. It’s an essential tool for anyone looking to optimize system performance or troubleshoot issues.
