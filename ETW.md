# WindowsPerf support for Event Tracing for Windows (ETW)

[[_TOC_]]

# Introduction

## Event Tracing for Windows (ETW)

[Event Tracing for Windows (ETW)](https://learn.microsoft.com/en-us/windows-hardware/drivers/devtest/event-tracing-for-windows--etw-) provides a mechanism to trace and log events that are raised by user-mode applications and kernel-mode drivers. ETW is implemented in the Windows operating system and provides developers a fast, reliable, and versatile set of event tracing features.

Microsoft Event Tracing for Windows (ETW) is a high-speed tracing facility built into the Windows operating system. It provides a mechanism to trace and log events raised by both user-mode applications and kernel-mode driver. Using a buffering and logging mechanism implemented in the operating system kernel, ETW offers developers a fast, reliable, and versatile set of event tracing features. It can be used for system and application diagnosis, troubleshooting, and performance monitoring.

The architecture of Event Tracing for Windows (ETW) is designed to be robust, dynamic, and lightweight. It involves `event providers`, which are software components instrumented for ETW to report critical errors and other important events. These providers register with ETW and raise corresponding events when they encounter an error condition or other important execution state. The events are initially written to an ETW Session, which can deliver the event data live to a consumer or log it for later processing and analysis. An ETW Controller starts and stops ETW Sessions and dynamically enables providers.

WindowsPerf raises corresponding PMU events from `wperf` application and `wperf-driver`. These can be recorded (and saved) with [Windows Performance Recorder (WPR)](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/windows-performance-recorder)  and analyzed using [Windows Performance Analyzer](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/windows-performance-analyzer) with installed `WPA-plugin-etl`.

### Windows Performance Analyzer

[Windows Performance Analyzer](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/windows-performance-analyzer) is a tool that creates graphs and data tables of Event Tracing for Windows (ETW) events that are recorded by Windows Performance Recorder (WPR), Xperf and WindowsPerf via [WPA-plugin-etl](https://gitlab.com/Linaro/WindowsPerf/wpa-plugin-etl).

## WindowsPerf ETW architecture

## WPA-plugin-etl

The [WPA-plugin-etl](https://gitlab.com/Linaro/WindowsPerf/wpa-plugin-etl) is a dedicated plugin developed for the Windows Performance Analyzer (WPA). Its primary function is to interpret and present event traces that have been injected by the WindowsPerf ETW (Event Tracing for Windows). These events can be injected through two main sources: the [wperf](https://gitlab.com/Linaro/WindowsPerf/windowsperf/-/tree/main/wperf?ref_type=heads) application and the [wperf-driver](https://gitlab.com/Linaro/WindowsPerf/windowsperf/-/tree/main/wperf-driver?ref_type=heads). The `wperf` application is a user-mode application, while the `wperf-driver` is a Windows Kernel Driver.

Together, they provide a comprehensive view of system performance and behavior, making the `WPA-plugin-etl` a valuable tool for system analysis and debugging. This plugin enhances the capabilities of WPA, allowing users to delve deeper into the Arm core and uncore PMU performance characteristics of their Windows on Arm systems. It’s an essential tool for anyone looking to optimize system performance or troubleshoot issues.

# HOWTO use WindowsPerf with ETW

## Dependencies

These are the tools and resources required to configure usage of WindowsPerf with ETW:
- [wevtutil](https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/wevtutil) enables you to retrieve information about event logs and publishers. You can also use this command to install and uninstall event manifests, to run queries, and to export, archive, and clear logs.

Note: The `WevtUtil.exe` tool is included in `%windir%\System32` directory. You can check its presence with below command:

```
> %windir%\System32\WevtUtil.exe /?
```

- WindowsPerf Kernel Driver ETW event manifest file [wperf-driver-etw-manifest.xml](https://gitlab.com/Linaro/WindowsPerf/windowsperf/-/blob/main/wperf-driver/wperf-driver-etw-manifest.xml?ref_type=heads). This file is part of [wperf-driver](https://gitlab.com/Linaro/WindowsPerf/windowsperf/-/tree/main/wperf-driver?ref_type=heads) project.

## Configuration instructions

1. Download locally [wperf-driver-etw-manifest.xml](https://gitlab.com/Linaro/WindowsPerf/windowsperf/-/blob/main/wperf-driver/wperf-driver-etw-manifest.xml?ref_type=heads) manifest file.

**Note**: Make sure the XML file corresponds to the `wperf-driver` version you are using on your system.

2. Run Command Prompt as Admin with the `Run` command window. Press `Windows`+`R` to open the `Run` window. Type `cmd` into the box and then press `Ctrl`+`Shift`+`Enter` to run the command as an Administrator.

3. Change directory to where you've downloaded the `wperf-driver-etw-manifest.xml` manifest file. For example if you've downloaded the file to `Downloads` directory, simply:

**Note**: By default, Chrome, Firefox and Microsoft Edge download files to the `Downloads` folder located at `%USERPROFILE%\Downloads`.

```
> cd %USERPROFILE%\Downloads
```

4. Uninstall the manifest with `wevtutil.exe um` command in case you already have a manifest installed.

```
> %windir%\System32\wevtutil.exe um .\wperf-driver-etw-manifest.xml
```

Note: Command `wevtutil.exe {um | uninstall-manifest} <Manifest>` uninstalls all publishers and logs from a manifest.

5. Install the manifest `wperf-driver-etw-manifest.xml` with the `wevtutil` tool using the below command.

```
> %windir%\System32\wevtutil.exe im .\wperf-driver-etw-manifest.xml
```

Note: Command `{im | install-manifest} <Manifest>` installs event publishers and logs from a manifest.
