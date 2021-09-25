/*--         
Copyright (c) 2008  Microsoft Corporation

Module Name:

    moufiltr.c

Abstract:

Environment:

    Kernel mode only- Framework Version 

Notes:


--*/

#include "moufiltr.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, MouFilter_EvtDeviceAdd)
#pragma alloc_text (PAGE, MouFilter_EvtIoInternalDeviceControl)
#endif

#pragma warning(push)
#pragma warning(disable:4055) // type case from PVOID to PSERVICE_CALLBACK_ROUTINE
#pragma warning(disable:4152) // function/data pointer conversion in expression

#define IOCTL_INTERNAL_MOUSE_REPORT CTL_CODE(FILE_DEVICE_MOUSE,0x800,METHOD_BUFFERED,FILE_ANY_ACCESS)//自定义ioctl
#define REPORT_BUFFER_SIZE   16
ULONG runtimes_io = 0;

//
//float TouchPad_ReportInterval;//定义触摸板报告间隔时间
//ULONG tick_count = 0; ///
//
//LARGE_INTEGER last_ticktime; //上次报告计时
//LARGE_INTEGER current_ticktime;//当前报告计时
//LARGE_INTEGER ticktime_Interval;//报告间隔时间


/////////////////

VOID RegDebug(WCHAR* strValueName, PVOID dataValue, ULONG datasizeValue)//RegDebug(L"Run debug here",pBuffer,pBufferSize);//RegDebug(L"Run debug here",NULL,0x12345678);
{
    //初始化注册表项
    UNICODE_STRING stringKey;
    RtlInitUnicodeString(&stringKey, L"\\Registry\\Machine\\Software\\RegDebug");

    //初始化OBJECT_ATTRIBUTES结构
    OBJECT_ATTRIBUTES  ObjectAttributes;
    InitializeObjectAttributes(&ObjectAttributes, &stringKey, OBJ_CASE_INSENSITIVE, NULL, NULL);

    //创建注册表项
    HANDLE hKey;
    ULONG Des;
    NTSTATUS status = ZwCreateKey(&hKey, KEY_ALL_ACCESS, &ObjectAttributes, 0, NULL, REG_OPTION_NON_VOLATILE, &Des);
    if (NT_SUCCESS(status))
    {
        if (Des == REG_CREATED_NEW_KEY)
        {
            KdPrint(("新建注册表项！\n"));
        }
        else
        {
            KdPrint(("要创建的注册表项已经存在！\n"));
        }
    }
    else {
        return;
    }

    //初始化valueName
    UNICODE_STRING valueName;
    RtlInitUnicodeString(&valueName, strValueName);

    if (dataValue == NULL) {
        //设置REG_DWORD键值
        status = ZwSetValueKey(hKey, &valueName, 0, REG_DWORD, &datasizeValue, 4);
        if (!NT_SUCCESS(status))
        {
            KdPrint(("设置REG_DWORD键值失败！\n"));
        }
    }
    else {
        //设置REG_BINARY键值
        status = ZwSetValueKey(hKey, &valueName, 0, REG_BINARY, dataValue, datasizeValue);
        if (!NT_SUCCESS(status))
        {
            KdPrint(("设置REG_BINARY键值失败！\n"));
        }
    }
    ZwFlushKey(hKey);
    //关闭注册表句柄
    ZwClose(hKey);
}


NTSTATUS
DriverEntry (
    IN  PDRIVER_OBJECT  DriverObject,
    IN  PUNICODE_STRING RegistryPath
    )
/*++
Routine Description:

     Installable driver initialization entry point.
    This entry point is called directly by the I/O system.

--*/
{
    WDF_DRIVER_CONFIG               config;
    NTSTATUS                                status;

    RegDebug(L"DriverEntry start", NULL, 0);
    
    // Initialize driver config to control the attributes that
    // are global to the driver. Note that framework by default
    // provides a driver unload routine. If you create any resources
    // in the DriverEntry and want to be cleaned in driver unload,
    // you can override that by manually setting the EvtDriverUnload in the
    // config structure. In general xxx_CONFIG_INIT macros are provided to
    // initialize most commonly used members.

    WDF_DRIVER_CONFIG_INIT(
        &config,
        MouFilter_EvtDeviceAdd
    );

    //
    WDF_OBJECT_ATTRIBUTES  attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = EvtDriverContextCleanup;


    //
    // Create a framework driver object to represent our driver.
    //
    status = WdfDriverCreate(DriverObject,
                            RegistryPath,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            &config,
                            WDF_NO_HANDLE); // hDriver optional
    if (!NT_SUCCESS(status)) {
        DebugPrint( ("WdfDriverCreate failed with status 0x%x\n", status));
        RegDebug(L"WdfDriverCreate failed with status", NULL, status);
    }


    RegDebug(L"DriverEntry ok", NULL, status);
    return status; 
}


//设备卸载,在这里释放EvtDeviceAdd分配的资源, WDF资源不用释放，WDF框架会自动回收
static VOID EvtCleanupCallback(WDFOBJECT Object)
{
    RegDebug(L"EvtCleanupCallback ok", NULL, 0);

    PDEVICE_EXTENSION   deviceContext = GetDeviceContext((WDFDEVICE)Object);
    UNREFERENCED_PARAMETER(deviceContext);

}

static VOID EvtDriverContextCleanup(IN WDFOBJECT Object)
{
    RegDebug(L"EvtDriverContextCleanup ok", NULL, 0);

    UNREFERENCED_PARAMETER(Object);
}

NTSTATUS
MouFilter_EvtDeviceAdd(
    IN WDFDRIVER        Driver,
    IN PWDFDEVICE_INIT  DeviceInit
    )
/*++
Routine Description:

    EvtDeviceAdd is called by the framework in response to AddDevice
    call from the PnP manager. Here you can query the device properties
    using WdfFdoInitWdmGetPhysicalDevice/IoGetDeviceProperty and based
    on that, decide to create a filter device object and attach to the
    function stack.

    If you are not interested in filtering this particular instance of the
    device, you can just return STATUS_SUCCESS without creating a framework
    device.

Arguments:

    Driver - Handle to a framework driver object created in DriverEntry

    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.

Return Value:

    NTSTATUS

--*/   
{
    WDF_OBJECT_ATTRIBUTES   deviceAttributes;
    NTSTATUS                            status;
    WDFDEVICE                          hDevice;
    WDF_IO_QUEUE_CONFIG        ioQueueConfig;
    
    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    // Tell the framework that you are filter driver. Framework
    // takes care of inherting all the device flags & characterstics
    // from the lower device you are attaching to.
    //
    WdfFdoInitSetFilter(DeviceInit);

    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_MOUSE);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes,DEVICE_EXTENSION);
    deviceAttributes.EvtCleanupCallback = EvtCleanupCallback; //设备删除

    
    //
    // Create a framework device object.  This call will in turn create
    // a WDM deviceobject, attach to the lower stack and set the
    // appropriate flags and attributes.
    //
    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &hDevice);
    if (!NT_SUCCESS(status)) {
        RegDebug(L"WdfDeviceCreate failed with status code", NULL, status);
        return status;
    }

    //
    UNICODE_STRING MouseFilterObjectNameStr;
    RtlInitUnicodeString(&MouseFilterObjectNameStr, L"\\Device\\MouseFilter");
    status = WdfDeviceCreateSymbolicLink(hDevice, &MouseFilterObjectNameStr);
    if (!NT_SUCCESS(status)) {
        RegDebug(L"WdfDeviceCreateSymbolicLink failed", NULL, status);
        return status;
    }

    //
    // Configure the default queue to be Parallel. Do not use sequential queue
    // if this driver is going to be filtering PS2 ports because it can lead to
    // deadlock. The PS2 port driver sends a request to the top of the stack when it
    // receives an ioctl request and waits for it to be completed. If you use a
    // a sequential queue, this request will be stuck in the queue because of the 
    // outstanding ioctl request sent earlier to the port driver.
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig,
                             WdfIoQueueDispatchParallel);

    //
    // Framework by default creates non-power managed queues for
    // filter drivers.
    //
    ioQueueConfig.EvtIoInternalDeviceControl = MouFilter_EvtIoInternalDeviceControl;

    status = WdfIoQueueCreate(hDevice,
                            &ioQueueConfig,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            WDF_NO_HANDLE // pointer to default queue
                            );
    if (!NT_SUCCESS(status)) {
        RegDebug(L"WdfIoQueueCreate failed", NULL, status);
        return status;
    }


    //init context
    PDEVICE_EXTENSION   deviceContext = GetDeviceContext(hDevice);
    deviceContext->pDeviceObject = WdfDeviceWdmGetDeviceObject(hDevice);
    deviceContext->IoTarget = WdfDeviceGetIoTarget(hDevice);
    deviceContext->MousePDO = WdfIoTargetWdmGetTargetPhysicalDevice(deviceContext->IoTarget);
    deviceContext->NextDeviceObject = deviceContext->pDeviceObject->NextDevice;

    deviceContext->ReuseRequest = NULL;
    deviceContext->RequestBuffer = NULL;
    deviceContext->ActiveCount = 0;
    deviceContext->bReadReady = FALSE;
    deviceContext->LastButtonState = 0;
    RegDebug(L"deviceContext->pDeviceObject=", deviceContext->pDeviceObject->DriverObject->DriverName.Buffer, deviceContext->pDeviceObject->DriverObject->DriverName.Length);
    RegDebug(L"deviceContext->MousePDO=", deviceContext->MousePDO->DriverObject->DriverName.Buffer, deviceContext->MousePDO->DriverObject->DriverName.Length);
    if (deviceContext->NextDeviceObject) {
        RegDebug(L"deviceContext->NextDeviceObject=", deviceContext->NextDeviceObject->DriverObject->DriverName.Buffer, deviceContext->NextDeviceObject->DriverObject->DriverName.Length);
    }
    if (deviceContext->pDeviceObject->AttachedDevice) {
        RegDebug(L"deviceContext->pDeviceObject->AttachedDevice=", deviceContext->pDeviceObject->AttachedDevice->DriverObject->DriverName.Buffer, deviceContext->pDeviceObject->AttachedDevice->DriverObject->DriverName.Length);
    }

    //KeInitializeEvent(&deviceContext->TimeoutWait, NotificationEvent, FALSE);

    status = Create_reuse_request(deviceContext);
    if (!NT_SUCCESS(status)) {
        RegDebug(L"create_reuse_request failed", NULL, status);
        return status;
    }

    RegDebug(L"MouFilter_EvtDeviceAdd ok", NULL, status);
    return status;
}



VOID
MouFilter_DispatchPassThrough(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target
    )
/*++
Routine Description:

    Passes a request on to the lower driver.


--*/
{
    //
    // Pass the IRP to the target
    //
 
    WDF_REQUEST_SEND_OPTIONS options;
    BOOLEAN ret;
    NTSTATUS status = STATUS_SUCCESS;

    //
    // We are not interested in post processing the IRP so 
    // fire and forget.
    //
    WDF_REQUEST_SEND_OPTIONS_INIT(&options,
                                  WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

    ret = WdfRequestSend(Request, Target, &options);

    if (ret == FALSE) {
        status = WdfRequestGetStatus (Request);
        DebugPrint( ("WdfRequestSend failed: 0x%x\n", status));
        WdfRequestComplete(Request, status);
    }

    return;
}           

VOID
MouFilter_EvtIoInternalDeviceControl(
    IN WDFQUEUE      Queue,
    IN WDFREQUEST    Request,
    IN size_t        OutputBufferLength,
    IN size_t        InputBufferLength,
    IN ULONG         IoControlCode
    )
/*++

Routine Description:

    This routine is the dispatch routine for internal device control requests.
    There are two specific control codes that are of interest:
    
    IOCTL_INTERNAL_MOUSE_CONNECT:
        Store the old context and function pointer and replace it with our own.
        This makes life much simpler than intercepting IRPs sent by the RIT and
        modifying them on the way back up.
                                      
    IOCTL_INTERNAL_I8042_HOOK_MOUSE:
        Add in the necessary function pointers and context values so that we can
        alter how the ps/2 mouse is initialized.
                                            
    NOTE:  Handling IOCTL_INTERNAL_I8042_HOOK_MOUSE is *NOT* necessary if 
           all you want to do is filter MOUSE_INPUT_DATAs.  You can remove
           the handling code and all related device extension fields and 
           functions to conserve space.
                                         

--*/
{
    
   
    
    PDEVICE_EXTENSION           devExt;
    PCONNECT_DATA               connectData;
    PINTERNAL_I8042_HOOK_MOUSE  hookMouse;
    NTSTATUS                   status = STATUS_SUCCESS;
    WDFDEVICE                 hDevice;
    size_t                           length; 

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    PAGED_CODE();

    hDevice = WdfIoQueueGetDevice(Queue);
    devExt = GetDeviceContext(hDevice);

    runtimes_io++;

    switch (IoControlCode) {

    //
    // Connect a mouse class device driver to the port driver.
    //
    case IOCTL_INTERNAL_MOUSE_CONNECT:

        RegDebug(L"IOCTL_INTERNAL_MOUSE_CONNECT", NULL, runtimes_io);

        //
        // Only allow one connection.
        //
        if (devExt->UpperConnectData.ClassService != NULL) {
            status = STATUS_SHARING_VIOLATION;
            RegDebug(L"STATUS_SHARING_VIOLATION", NULL, status);
            break;
        }
        
        //
        // Copy the connection parameters to the device extension.
        //
         status = WdfRequestRetrieveInputBuffer(Request,
                            sizeof(CONNECT_DATA),
                            &connectData,//connectData = IoStack->Parameters.DeviceIoControl.Type3InputBuffer;
                            &length);
        if(!NT_SUCCESS(status)){
            RegDebug(L"WdfRequestRetrieveInputBuffer failed", NULL, status);
            break;
        }

        
        devExt->UpperConnectData = *connectData;

        //
        // Hook into the report chain.  Everytime a mouse packet is reported to
        // the system, MouFilter_ServiceCallback will be called
        //
        //connectData->ClassDeviceObject = WdfDeviceWdmGetDeviceObject(hDevice);
       // connectData->ClassService = MouFilter_ServiceCallback;

        RegDebug(L"IOCTL_INTERNAL_MOUSE_CONNECT ok", NULL, status);
        break;

    //
    // Disconnect a mouse class device driver from the port driver.
    //
    case IOCTL_INTERNAL_MOUSE_DISCONNECT:
        RegDebug(L"IOCTL_INTERNAL_MOUSE_DISCONNECT", NULL, status);
        //
        // Clear the connection parameters in the device extension.
        //
        // devExt->UpperConnectData.ClassDeviceObject = NULL;
        // devExt->UpperConnectData.ClassService = NULL;

        status = STATUS_NOT_IMPLEMENTED;
        break;

    //
    // Attach this driver to the initialization and byte processing of the 
    // i8042 (ie PS/2) mouse.  This is only necessary if you want to do PS/2
    // specific functions, otherwise hooking the CONNECT_DATA is sufficient
    //
    case IOCTL_INTERNAL_I8042_HOOK_MOUSE:   
        RegDebug(L"IOCTL_INTERNAL_I8042_HOOK_MOUSE", NULL, runtimes_io);
          DebugPrint(("hook mouse received!\n"));
        
        // Get the input buffer from the request
        // (Parameters.DeviceIoControl.Type3InputBuffer)
        //
        status = WdfRequestRetrieveInputBuffer(Request,
                            sizeof(INTERNAL_I8042_HOOK_MOUSE),
                            &hookMouse,
                            &length);
        if(!NT_SUCCESS(status)){
            DebugPrint(("WdfRequestRetrieveInputBuffer failed %x\n", status));
            RegDebug(L"IOCTL_INTERNAL_I8042_HOOK_MOUSE failed", NULL, status);
            break;
        }
      
        //
        // Set isr routine and context and record any values from above this driver
        //
        devExt->UpperContext = hookMouse->Context;
        hookMouse->Context = (PVOID) devExt;

        if (hookMouse->IsrRoutine) {
            devExt->UpperIsrHook = hookMouse->IsrRoutine;
        }
        hookMouse->IsrRoutine = (PI8042_MOUSE_ISR) MouFilter_IsrHook;

        //
        // Store all of the other functions we might need in the future
        //
        devExt->IsrWritePort = hookMouse->IsrWritePort;
        devExt->CallContext = hookMouse->CallContext;
        devExt->QueueMousePacket = hookMouse->QueueMousePacket;

        RegDebug(L"IOCTL_INTERNAL_I8042_HOOK_MOUSE ok", NULL, status);
        status = STATUS_SUCCESS;
        break;

    //
    // Might want to capture this in the future.  For now, then pass it down
    // the stack.  These queries must be successful for the RIT to communicate
    // with the mouse.
    //
    case IOCTL_MOUSE_QUERY_ATTRIBUTES: {//0xf0000
        RegDebug(L"IOCTL_MOUSE_QUERY_ATTRIBUTES", NULL, runtimes_io);

        PIO_STACK_LOCATION IoStack;
        PMOUSE_ATTRIBUTES Attributes;

        /* get current stack location */
        PIRP pIrp = WdfRequestWdmGetIrp(Request);
        IoStack = IoGetCurrentIrpStackLocation(pIrp);

        /* verify output buffer length */
        if (IoStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(MOUSE_ATTRIBUTES))
        {
            /* invalid request */
            RegDebug(L"IOCTL_MOUSE_QUERY_ATTRIBUTES Buffer too small", NULL, runtimes_io);
            pIrp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
            IoCompleteRequest(pIrp, IO_NO_INCREMENT);
            return;
        }

        /* get output buffer */
        Attributes = pIrp->AssociatedIrp.SystemBuffer;

        /* type of mouse */
        Attributes->MouseIdentifier = WHEELMOUSE_HID_HARDWARE;// WHEELMOUSE_HID_HARDWARE;//MOUSE_HID_HARDWARE

        /* number of buttons */
        Attributes->NumberOfButtons = 3;// devExt->UsageListLength;

        /* sample rate not used for usb */
        Attributes->SampleRate = 0;

        /* queue length */
        Attributes->InputDataQueueLength = 2;

        /* complete request */
        pIrp->IoStatus.Information = sizeof(MOUSE_ATTRIBUTES);
        pIrp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(pIrp, IO_NO_INCREMENT);

        if (!devExt->bReadReady) {
            devExt->bReadReady = TRUE;

            /*tick_count = KeQueryTimeIncrement(); ///
            KeQueryTickCount(&last_ticktime);*/

            //KeSetEvent(&devExt->TimeoutWait, 0, 0);
           /* BOOLEAN ret = WdfRequestSend(devExt->ReuseRequest, devExt->IoTarget, NULL);
            if (ret == FALSE) {
                status = WdfRequestGetStatus(Request);
                RegDebug(L"QueueDefaultEvtIoDeviceControl WdfRequestSend failed", NULL, status);
                WdfRequestComplete(Request, status);
            }          */
        }

        return;
        
    }
    case IOCTL_INTERNAL_MOUSE_REPORT:
    {
        size_t bufferlength = 0;
        PMOUSE_REPORT pMouseReport;
        status = WdfRequestRetrieveInputBuffer(Request, sizeof(MOUSE_REPORT), &pMouseReport, &bufferlength);
        if (!NT_SUCCESS(status)) {
            RegDebug(L"WdfRequestRetrieveInputBuffer IOCTL_INTERNAL_MOUSE_REPORT failed", NULL, status);
            break;
        }
        RegDebug(L"IOCTL_INTERNAL_MOUSE_REPORT pMouseReport=", pMouseReport, sizeof(MOUSE_REPORT));

        MOUSE_INPUT_DATA MouseInputData;

        /* init input data */
        RtlZeroMemory(&MouseInputData, sizeof(MOUSE_INPUT_DATA));

        UCHAR CurrentButtonState = pMouseReport->button & 0x07;
        BOOLEAN LButtonState = CurrentButtonState & 0x01;
        BOOLEAN RButtonState = CurrentButtonState & 0x02;
        BOOLEAN MButtonState = CurrentButtonState & 0x04;

        BOOLEAN lastLButtonState = devExt->LastButtonState & 0x01;
        BOOLEAN lastRButtonState = devExt->LastButtonState & 0x02;
        BOOLEAN lastMButtonState = devExt->LastButtonState & 0x04;

        USHORT ButtonFlags = 0;

        if (CurrentButtonState != devExt->LastButtonState) {
            if (LButtonState!= lastLButtonState) {
                if (LButtonState) {
                    ButtonFlags |= MOUSE_LEFT_BUTTON_DOWN;
                }
                else {
                    ButtonFlags |= MOUSE_LEFT_BUTTON_UP;
                }
            }

            if (RButtonState != lastRButtonState) {
                if (RButtonState) {
                    ButtonFlags |= MOUSE_RIGHT_BUTTON_DOWN;
                }
                else {
                    ButtonFlags |= MOUSE_RIGHT_BUTTON_UP;
                }
            }

            if (MButtonState != lastMButtonState) {
                if (MButtonState) {
                    ButtonFlags |= MOUSE_MIDDLE_BUTTON_DOWN;
                }
                else {
                    ButtonFlags |= MOUSE_MIDDLE_BUTTON_UP;
                }
            }

            /* init input data */
            MouseInputData.ButtonFlags = ButtonFlags;
            MouseInputData.Flags = MOUSE_MOVE_RELATIVE;//MOUSE_MOVE_RELATIVE | MOUSE_MOVE_NOCOALESCE
            MouseInputData.LastX = pMouseReport->dx;
            MouseInputData.LastY = pMouseReport->dy;


            /* dispatch mouse action */
            //MouHid_DispatchInputData(devExt, &MouseInputData);
            devExt->LastButtonState = CurrentButtonState;

            RegDebug(L"SendPtpReport ok", NULL, status);

        }
        
        RegDebug(L"IOCTL_INTERNAL_MOUSE_REPORT ok", NULL, status);

        WdfRequestComplete(Request, status);
        return;
    }
    break;

    case IOCTL_INTERNAL_MOUSE_ENABLE:
        RegDebug(L"IOCTL_INTERNAL_MOUSE_ENABLE", NULL, runtimes_io);
        status = STATUS_NOT_SUPPORTED;
        break;

    case IOCTL_INTERNAL_MOUSE_DISABLE:
        RegDebug(L"IOCTL_INTERNAL_MOUSE_DISABLE", NULL, runtimes_io); 
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
        
    default:
        RegDebug(L"IOCTL unkown", NULL, IoControlCode);
        status = STATUS_NOT_SUPPORTED;

    }

    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(Request, status);
        return ;
    }

    MouFilter_DispatchPassThrough(Request,WdfDeviceGetIoTarget(hDevice));
}


BOOLEAN
MouFilter_IsrHook (
    PVOID         DeviceExtension, 
    PMOUSE_INPUT_DATA       CurrentInput, 
    POUTPUT_PACKET          CurrentOutput,
    UCHAR                   StatusByte,
    PUCHAR                  DataByte,
    PBOOLEAN                ContinueProcessing,
    PMOUSE_STATE            MouseState,
    PMOUSE_RESET_SUBSTATE   ResetSubState
)
/*++

Remarks:
    i8042prt specific code, if you are writing a packet only filter driver, you
    can remove this function

Arguments:

    DeviceExtension - Our context passed during IOCTL_INTERNAL_I8042_HOOK_MOUSE
    
    CurrentInput - Current input packet being formulated by processing all the
                    interrupts

    CurrentOutput - Current list of bytes being written to the mouse or the
                    i8042 port.
                    
    StatusByte    - Byte read from I/O port 60 when the interrupt occurred                                            
    
    DataByte      - Byte read from I/O port 64 when the interrupt occurred. 
                    This value can be modified and i8042prt will use this value
                    if ContinueProcessing is TRUE

    ContinueProcessing - If TRUE, i8042prt will proceed with normal processing of
                         the interrupt.  If FALSE, i8042prt will return from the
                         interrupt after this function returns.  Also, if FALSE,
                         it is this functions responsibilityt to report the input
                         packet via the function provided in the hook IOCTL or via
                         queueing a DPC within this driver and calling the
                         service callback function acquired from the connect IOCTL
                                             
Return Value:

    Status is returned.

  --+*/
{
    PDEVICE_EXTENSION   devExt;
    BOOLEAN             retVal = TRUE;

    devExt = DeviceExtension;
    
    if (devExt->UpperIsrHook) {
        retVal = (*devExt->UpperIsrHook) (devExt->UpperContext,
                            CurrentInput,
                            CurrentOutput,
                            StatusByte,
                            DataByte,
                            ContinueProcessing,
                            MouseState,
                            ResetSubState
            );

        if (!retVal || !(*ContinueProcessing)) {
            return retVal;
        }
    }

    RegDebug(L"MouFilter_IsrHook ok", NULL, 0);
    *ContinueProcessing = TRUE;
    return retVal;
}

    

VOID
MouFilter_ServiceCallback(
    IN PDEVICE_OBJECT DeviceObject,
    IN PMOUSE_INPUT_DATA InputDataStart,
    IN PMOUSE_INPUT_DATA InputDataEnd,
    IN OUT PULONG InputDataConsumed
    )
/*++

Routine Description:

    Called when there are mouse packets to report to the RIT.  You can do 
    anything you like to the packets.  For instance:
    
    o Drop a packet altogether
    o Mutate the contents of a packet 
    o Insert packets into the stream 
                    
Arguments:

    DeviceObject - Context passed during the connect IOCTL
    
    InputDataStart - First packet to be reported
    
    InputDataEnd - One past the last packet to be reported.  Total number of
                   packets is equal to InputDataEnd - InputDataStart
    
    InputDataConsumed - Set to the total number of packets consumed by the RIT
                        (via the function pointer we replaced in the connect
                        IOCTL)

Return Value:

    Status is returned.

--*/
{
    PDEVICE_EXTENSION   devExt;
    WDFDEVICE   hDevice;

    hDevice = WdfWdmDeviceGetWdfDeviceHandle(DeviceObject);

    devExt = GetDeviceContext(hDevice);
    RegDebug(L"MouFilter_ServiceCallback ok", NULL, devExt->ActiveCount);
    //
    // UpperConnectData must be called at DISPATCH
    //
    (*(PSERVICE_CALLBACK_ROUTINE) devExt->UpperConnectData.ClassService)(
        devExt->UpperConnectData.ClassDeviceObject,
        InputDataStart,
        InputDataEnd,
        InputDataConsumed
        );
} 

#pragma warning(pop)


VOID
MouHid_DispatchInputData(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PMOUSE_INPUT_DATA InputData)
{
    RegDebug(L"MouHid_DispatchInputData start", NULL, runtimes_io);
    KIRQL OldIrql;
    ULONG InputDataConsumed = 0;

    if (!DeviceExtension->UpperConnectData.ClassService)
        return;

    /* sanity check */
    ASSERT(DeviceExtension->UpperConnectData.ClassService);
    ASSERT(DeviceExtension->UpperConnectData.ClassDeviceObject);

    /* raise irql */
    KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

    /* dispatch input data */
    (*(PSERVICE_CALLBACK_ROUTINE)DeviceExtension->UpperConnectData.ClassService)(DeviceExtension->UpperConnectData.ClassDeviceObject, InputData, InputData + 1, &InputDataConsumed);

    /* lower irql to previous level */
    KeLowerIrql(OldIrql);
    RegDebug(L"MouHid_DispatchInputData ok", NULL, 0);
}



void CompletionRoutine(
    WDFREQUEST Request,
    WDFIOTARGET Target,
    PWDF_REQUEST_COMPLETION_PARAMS Params,
    WDFCONTEXT Context)
{
    PDEVICE_EXTENSION ext = (PDEVICE_EXTENSION)Context;
    NTSTATUS status = WdfRequestGetStatus(Request);
    BOOLEAN bReadOK = FALSE;
    ++ext->ActiveCount; /// !!!

    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Params);
    RegDebug(L"CompletionRoutine ActiveCount", NULL, ext->ActiveCount);


    ////计算报告频率和时间间隔
    //KeQueryTickCount(&current_ticktime);
    //ticktime_Interval.QuadPart = (current_ticktime.QuadPart - last_ticktime.QuadPart) * tick_count / 10000;//单位ms毫秒
    //TouchPad_ReportInterval = (float)ticktime_Interval.LowPart;//触摸板报告间隔时间ms
    //last_ticktime = current_ticktime;

    
    PTP_REPORT ptpReport;
    /////
    if (NT_SUCCESS(status)) { // success
        ////
        LONG retlen = (LONG)WdfRequestGetInformation(Request);
        RegDebug(L"CompletionRoutine retlen=", NULL, retlen);
        PUCHAR data = (PUCHAR)WdfMemoryGetBuffer(ext->RequestBuffer, NULL);

        ptpReport = *(PPTP_REPORT)data;

        RegDebug(L"CompletionRoutine ptpReport=", &ptpReport, sizeof(PTP_REPORT));
        /////////
        bReadOK = TRUE;
    }
    else {
        while (ext->ActiveCount < 100) {
            MOUSE_INPUT_DATA MouseInputData;

            /* init input data */
            RtlZeroMemory(&MouseInputData, sizeof(MOUSE_INPUT_DATA));

            /* init input data */
            MouseInputData.ButtonFlags = 0;
            MouseInputData.Flags = MOUSE_MOVE_RELATIVE;
            MouseInputData.LastX = 1;
            MouseInputData.LastY = 1;

            /* dispatch mouse action */
            MouHid_DispatchInputData(ext, &MouseInputData);

            LARGE_INTEGER DueTime;
            DueTime.QuadPart = -10000 * 200;//定时200ms

            //sleep,waiting (TimeoutWait) singaled
            status = KeDelayExecutionThread(KernelMode, TRUE, &DueTime);
            if (!NT_SUCCESS(status)) {
                RegDebug(L"KeDelayExecutionThread failed", NULL, status);
            }
        }
        
    }



    MOUSE_INPUT_DATA MouseInputData;

    /* init input data */
    RtlZeroMemory(&MouseInputData, sizeof(MOUSE_INPUT_DATA));

    /* init input data */
    MouseInputData.ButtonFlags = 0;
    MouseInputData.Flags = MOUSE_MOVE_RELATIVE;
    MouseInputData.LastX = 1;
    MouseInputData.LastY = 1;

    /* dispatch mouse action */
    MouHid_DispatchInputData(ext, &MouseInputData);

    RegDebug(L"CompletionRoutine ok", NULL, status);

    /////重新初始化
    WDF_REQUEST_REUSE_PARAMS reuseParams;

    WDF_REQUEST_REUSE_PARAMS_INIT(
        &reuseParams,
        WDF_REQUEST_REUSE_NO_FLAGS,
        STATUS_NOT_SUPPORTED//STATUS_NOT_SUPPORTED//STATUS_SUCCESS
    );

    status = WdfRequestReuse(Request, &reuseParams);//在完成函数里，函数返回总是成功
    if (!NT_SUCCESS(status)) {
        RegDebug(L"WdfRequestReuse failed", NULL, status);
    }

    RegDebug(L"WdfRequestReuse ok", NULL, ext->ActiveCount);
    status = WdfIoTargetFormatRequestForInternalIoctl(ext->IoTarget, ext->ReuseRequest,//Request//ext->ReuseRequest
        IOCTL_HID_GET_INPUT_REPORT,//IOCTL_HID_GET_INPUT_REPORT//IOCTL_HID_READ_REPORT
        ext->RequestBuffer, NULL, ext->RequestBuffer, NULL); //因为参数没变，函数返回总是成功 ，查看MSDN

    if (!NT_SUCCESS(status)) {
        RegDebug(L"WdfIoTargetFormatRequestForInternalIoctl failed", NULL, status);
    }

    WdfRequestSetCompletionRoutine(ext->ReuseRequest, CompletionRoutine, ext); //完成函数//Request//ext->ReuseRequest


    LARGE_INTEGER DueTime;
    DueTime.QuadPart = -10000 * 200;//定时200ms

    //sleep,waiting (TimeoutWait) singaled
    status = KeDelayExecutionThread(KernelMode, TRUE, &DueTime);
    if (!NT_SUCCESS(status)) {
        RegDebug(L"KeDelayExecutionThread failed", NULL, status);
    }

    //status = KeWaitForSingleObject(&ext->TimeoutWait,Executive, KernelMode, FALSE, &DueTime);
    //if (!NT_SUCCESS(status)) {
    //    RegDebug(L"KeWaitForSingleObject failed", NULL, status);
    //}
    ////KeSetEvent(&ext->TimeoutWait, 0, 0);

    RegDebug(L"timerout ok", NULL, status);

    //立即投递下一个请求
    WDF_REQUEST_SEND_OPTIONS options;
    WDF_REQUEST_SEND_OPTIONS_INIT(&options,
        WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);//WDF_REQUEST_SEND_OPTION_TIMEOUT  //WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET

    //将I/O请求发送到下层设备前，做相应的数据处理//不设置完成例程时，下面这句可有可无
    //WdfRequestFormatRequestUsingCurrentType(Request);
    if (bReadOK) {
        bReadOK = WdfRequestSend(Request, ext->IoTarget, NULL);//NULL//&options
        if (bReadOK==FALSE) {
            status = WdfRequestGetStatus(Request);
            RegDebug(L"WdfRequestSend failed", NULL, status);
            WdfRequestComplete(Request, status);
        }
    }
   


    RegDebug(L"CompletionRoutine end", NULL, status);
}


////
NTSTATUS Create_reuse_request(PDEVICE_EXTENSION   deviceContext)
{
    NTSTATUS status = STATUS_SUCCESS;

    ///create IOCTL_HID_READ_REPORT Request
    WDF_OBJECT_ATTRIBUTES      RequestAttributes = { 0 };

    WDF_OBJECT_ATTRIBUTES_INIT(&RequestAttributes);
    RequestAttributes.ParentObject = deviceContext->hDevice; //设置父对象，父对象删除时候，自动删除request
    status = WdfRequestCreate(&RequestAttributes,
        deviceContext->IoTarget,
        &deviceContext->ReuseRequest);
    if (!NT_SUCCESS(status)) {
        RegDebug(L"WdfRequestCreate err", NULL, status);
        return status;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&RequestAttributes);
    RequestAttributes.ParentObject = deviceContext->hDevice; //设置父对象，父对象删除时候，自动删除
    status = WdfMemoryCreate(&RequestAttributes, NonPagedPool, 'Fxsd', REPORT_BUFFER_SIZE, &deviceContext->RequestBuffer, NULL);
    if (!NT_SUCCESS(status)) {
        WdfObjectDelete(deviceContext->ReuseRequest);
        deviceContext->ReuseRequest = NULL;
        RegDebug(L"WdfMemoryCreate err", NULL, status);

        return status;
    }

    status = WdfIoTargetFormatRequestForInternalIoctl(deviceContext->IoTarget, deviceContext->ReuseRequest,
        IOCTL_HID_GET_INPUT_REPORT,//IOCTL_HID_GET_INPUT_REPORT
        deviceContext->RequestBuffer, NULL, deviceContext->RequestBuffer, NULL); //设置IOCTL请求

    if (!NT_SUCCESS(status)) {
        WdfObjectDelete(deviceContext->ReuseRequest); deviceContext->ReuseRequest = 0;
        WdfObjectDelete(deviceContext->RequestBuffer); deviceContext->RequestBuffer = NULL;
        RegDebug(L"WdfIoTargetFormatRequestForInternalIoctl err", NULL, status);
        return status;
    }

    WdfRequestSetCompletionRoutine(deviceContext->ReuseRequest, CompletionRoutine, deviceContext); //完成函数

    RegDebug(L"Create_reuse_request ok", NULL, status);
    return status;

}