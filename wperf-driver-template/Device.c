// BSD 3-Clause License
//
// Copyright (c) 2024, Arm Limited
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "driver.h"
#if defined ENABLE_TRACING
#include "device.tmh"
#endif

EVT_WDF_DEVICE_SELF_MANAGED_IO_INIT WindowsPerf_tEvtDeviceSelfManagedIoStart;
EVT_WDF_DEVICE_SELF_MANAGED_IO_SUSPEND WindowsPerf_tEvtDeviceSelfManagedIoSuspend;

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, WperfDriver_TCreateDevice)
#endif


static void FileCreate(
    WDFDEVICE Device,
    WDFREQUEST Request,
    WDFFILEOBJECT FileObject
)
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(FileObject);
    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_TRACE_LEVEL, "====>FileCreate\n"));

    WdfRequestComplete(Request, STATUS_SUCCESS);
    return;
}

static void FileClose(
    WDFFILEOBJECT FileObject
)
{
    UNREFERENCED_PARAMETER(FileObject);
    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_TRACE_LEVEL, "<====FileCose\n"));
}



/// <summary>
/// Worker routine called to create a device and its software resources.
/// </summary>
/// <param name="DeviceInit">Pointer to an opaque init structure. Memory
/// for this structure will be freed by the framework when the
/// WdfDeviceCreate succeeds. So don't access the structure after that
/// point.</param>
/// <returns>NTSTATUS</returns>
NTSTATUS
WperfDriver_TCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    PDEVICE_EXTENSION devExt;
    WDF_PNPPOWER_EVENT_CALLBACKS    pnpPowerCallbacks;
    WDFDEVICE device;
    NTSTATUS status;
    WDF_FILEOBJECT_CONFIG FileObjectConfig;

    PAGED_CODE();

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);

    //
    // Register pnp/power callbacks so that we can start and stop the timer as the device
    // gets started and stopped.
    //
    pnpPowerCallbacks.EvtDeviceSelfManagedIoInit = WindowsPerf_tEvtDeviceSelfManagedIoStart;
    pnpPowerCallbacks.EvtDeviceSelfManagedIoSuspend = WindowsPerf_tEvtDeviceSelfManagedIoSuspend;

    //
    // Function used for both Init and Restart Callbacks
    //
#pragma warning(suppress: 28024)
    pnpPowerCallbacks.EvtDeviceSelfManagedIoRestart = WindowsPerf_tEvtDeviceSelfManagedIoStart;

    WdfDeviceInitSetPowerPageable(DeviceInit);

    //
    // Register the PnP and power callbacks. Power policy related callbacks will be registered
    // later in SotwareInit.
    //
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

    // Register for file object creation, we dont need callbacks to file open and close etc
    WDF_FILEOBJECT_CONFIG_INIT(&FileObjectConfig, FileCreate, FileClose, WDF_NO_EVENT_CALLBACK);

    WdfDeviceInitSetFileObjectConfig(DeviceInit, &FileObjectConfig, WDF_NO_OBJECT_ATTRIBUTES);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_EXTENSION);

    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);

    if (!NT_SUCCESS(status))
    {
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "WdfDeviceCreate failed 0x%X %S", status, DbgStatusStr(status)));
        return status;
    }

    // Get a pointer to the device context structure that we just associated
    // with the device object.
    devExt = GetDeviceGetContext(device);
    devExt->PrivateDeviceData = 0;


    // Create a device interface so that applications can find and talk
    // to us.
    status = WdfDeviceCreateDeviceInterface(
        device,
        &GUID_DEVINTERFACE_WINDOWSPERF,
        NULL // ReferenceString
        );

    if (!NT_SUCCESS(status)) 
    {
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "WdfDeviceCreateDeviceInterface failed 0x%X %S", status, DbgStatusStr(status)));
        return status;
    }

    status = WperfDriver_TIOInitialize(device, devExt);
    if (!NT_SUCCESS(status))
    {
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "WperfDriver_TIOInitialize failed 0x%X %S", status, DbgStatusStr(status)));
        return status;
    }




    devExt->current_status.file_object = 0;
    devExt->current_status.ioctl = 0;
    devExt->current_status.status = STS_IDLE;
    KeInitializeSpinLock(&devExt->current_status.sts_lock);

    return status;
}

/// <summary>
/// This event is called by the Framework when the device is started
/// or restarted after a suspend operation.
///
/// This function is not marked pageable because this function is in the
/// device power up path.When a function is marked pagable and the code
/// section is paged out, it will generate a page fault which could impact
/// the fast resume behavior because the client driver will have to wait
/// until the system drivers can service this page fau
/// </summary>
/// <param name="Device">Handle to a framework device object.</param>
/// <returns>NTSTATUS - Failures will result in the device stack being torn down.</returns>
NTSTATUS
WindowsPerf_tEvtDeviceSelfManagedIoStart(
    IN  WDFDEVICE Device
)
{
    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "--> WindowsPerf_tEvtDeviceSelfManagedIoInit\n"));

    //
    // Restart the queue and the periodic timer. We stopped them before going
    // into low power state.
    //
    WdfIoQueueStart(WdfDeviceGetDefaultQueue(Device));

    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "<-- WindowsPerf_tEvtDeviceSelfManagedIoInit\n"));

    return STATUS_SUCCESS;
}

/// <summary>
/// This event is called by the Framework when the device is stopped
/// for resource rebalance or suspended when the system is entering
///  Sx state.
/// </summary>
/// <param name="Device">Handle to a framework device object.</param>
/// <returns>NTSTATUS - The driver is not allowed to fail this function.  If it does, the
///  device stack will be torn down.</returns>
NTSTATUS
WindowsPerf_tEvtDeviceSelfManagedIoSuspend(
    IN  WDFDEVICE Device
)
{
    PAGED_CODE();

    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "--> WindowsPerf_tEvtDeviceSelfManagedIoSuspend\n"));

    //
    // Before we stop the timer we should make sure there are no outstanding
    // i/o. We need to do that because framework cannot suspend the device
    // if there are requests owned by the driver. There are two ways to solve
    // this issue: 1) We can wait for the outstanding I/O to be complete by the
    // periodic timer 2) Register EvtIoStop callback on the queue and acknowledge
    // the request to inform the framework that it's okay to suspend the device
    // with outstanding I/O. In this sample we will use the 1st approach
    // because it's pretty easy to do. We will restart the queue when the
    // device is restarted.
    //
    WdfIoQueueStopSynchronously(WdfDeviceGetDefaultQueue(Device));

    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "<-- WindowsPerf_tEvtDeviceSelfManagedIoSuspend\n"));

    return STATUS_SUCCESS;
}
