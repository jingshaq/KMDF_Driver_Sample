#include "PtpFilterDriver.h"

/////
extern "C" NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    NTSTATUS Status;
    WDF_DRIVER_CONFIG      config;
    WDF_OBJECT_ATTRIBUTES  DriverAttributes;

    WDF_OBJECT_ATTRIBUTES_INIT(&DriverAttributes);
    DriverAttributes.EvtCleanupCallback = EvtDriverContextCleanup;

    WDF_DRIVER_CONFIG_INIT(&config, PTPFilterEvtDeviceAdd);

    Status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        &DriverAttributes,
        &config,
        WDF_NO_HANDLE);

    if (!NT_SUCCESS(Status)) {
        DbgPrint("WdfDriverCreate failed %!STATUS!", Status);
    }

    RegDebug(L"DriverEntry", NULL, 0);
    return Status;
}

void EvtDriverContextCleanup(IN WDFOBJECT DriverObject)
{
    DbgPrint("EvtDriverContextCleanup");
    UNREFERENCED_PARAMETER(DriverObject);
}

NTSTATUS PTPFilterEvtDeviceAdd(IN WDFDRIVER Driver, IN PWDFDEVICE_INIT DeviceInit)
{
    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();//测试

    NTSTATUS Status = PTPFilterCreateDevice(DeviceInit);
    return Status;
}

NTSTATUS
PTPFilterDevicePrepareHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourceList,
    _In_ WDFCMRESLIST ResourceListTranslated
)
{
    UNREFERENCED_PARAMETER(ResourceList);
    UNREFERENCED_PARAMETER(ResourceListTranslated);

    PDEVICE_CONTEXT pDeviceContext = GetDeviceContext(Device);

    NTSTATUS Status = Hid_StartDevice(pDeviceContext);

    if (!NT_SUCCESS(Status)) {
        DbgPrint("PtpFilterGetDeviceProperties failed %!STATUS!", Status);
        RegDebug(L"PtpFilterGetDeviceProperties failed", NULL, Status);
        goto Exit;
    }

    //sub_140006004(
    //    v1 + 64,
    //    *(_QWORD*)(v1 + 880),
    //    (void(__fastcall*)(const char*, __int64, _QWORD, __int64))sub_140002C98);//

    WDF_OBJECT_ATTRIBUTES  RequestAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&RequestAttributes);

    RequestAttributes.ParentObject = pDeviceContext->hDevice;
    Status = WdfRequestCreate(&RequestAttributes,
        pDeviceContext->IoTarget,
        &pDeviceContext->ReuseRequest);

    if (!NT_SUCCESS(Status)) 
    {
        pDeviceContext->ReuseRequest = NULL;
        return Status;
    } 
    
    WDF_OBJECT_ATTRIBUTES_INIT(&RequestAttributes);
    RequestAttributes.ParentObject = pDeviceContext->hDevice;

    size_t REPORT_BUFFER_SIZE = pDeviceContext->REPORT_BUFFER_SIZE;
    pDeviceContext->ReportLength = REPORT_BUFFER_SIZE;

    if (!REPORT_BUFFER_SIZE)
    {
        DbgPrint("InputReportByteLength is 0");
        goto Exit;
    }

    Status = WdfMemoryCreate(
        &RequestAttributes,
        NonPagedPool,
        MOUHID_TAG,
        REPORT_BUFFER_SIZE,
        &pDeviceContext->RequestBuffer,
        &pDeviceContext->ReportBuffer);

    if (NT_SUCCESS(Status)) {
        DbgPrint("WdfMemoryCreate failed %!STATUS!", Status);
        RegDebug(L"WdfMemoryCreate failed", NULL, Status);

        pDeviceContext->RequestBuffer = NULL;
        return Status;

    }
    
    return STATUS_SUCCESS;
    
Exit:
    if ((Status & 0x80000000) != 0)
    {
        if (pDeviceContext->ReuseRequest) {
            pDeviceContext->ReuseRequest = NULL;
        }

        if (pDeviceContext->RequestBuffer) {
            pDeviceContext->RequestBuffer = NULL;
            pDeviceContext->ReportBuffer = NULL;
        }
    }

    return Status;
}

NTSTATUS PTPFilterDeviceReleaseHardware(WDFDEVICE Device,
    WDFCMRESLIST ResourcesTranslated)
{
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    NTSTATUS Status = STATUS_SUCCESS;
    PDEVICE_CONTEXT pDeviceContext = GetDeviceContext(Device);

    pDeviceContext->RequestLength = 1;

    Delete_PLIST_ENTRY(pDeviceContext->LIST_ENTRY_928);
    Delete_PLIST_ENTRY(pDeviceContext->LIST_ENTRY_944);

    if (pDeviceContext->ReuseRequest) {
        // (*(void (__fastcall **)(__int64))(qword_14000A118 + 2064))(qword_14000A110);
        WdfObjectDelete(pDeviceContext->ReuseRequest);

        pDeviceContext->ReuseRequest = NULL;
    }

    if (pDeviceContext->RequestBuffer) {
        //  (*(void (__fastcall **)(__int64))(qword_14000A118 + 1664))(qword_14000A110);
        WdfObjectDelete(pDeviceContext->RequestBuffer);

        pDeviceContext->RequestBuffer = NULL;
        pDeviceContext->ReportBuffer = NULL;
    }

    PVOID buffer = pDeviceContext->PreparsedData;
    if (buffer)
    {
        ExFreePoolWithTag(buffer, MOUHID_TAG);////0x50747046u=
        pDeviceContext->PreparsedData=NULL;
    }

    buffer = pDeviceContext->UsageListBuffer;
    if (buffer)
    {
        ExFreePoolWithTag(buffer, MOUHID_TAG);////0x50747046u=
        pDeviceContext->UsageListBuffer=NULL;
    }

    DbgPrint("[PTP] %s : ", "PTPFilterDeviceReleaseHardware");
    RegDebug(L"PTPFilterDeviceReleaseHardware", NULL, Status);

    return STATUS_SUCCESS;
}

NTSTATUS Hid_StartDevice(PDEVICE_CONTEXT pDeviceContext)
{
    NTSTATUS Status = STATUS_SUCCESS;

    WDFIOTARGET IoTarget = pDeviceContext->IoTarget;
    PHID_COLLECTION_INFORMATION NumberOfBytes = NULL;
    ULONG Size = sizeof(HID_COLLECTION_INFORMATION);
    BOOLEAN bGetCaps_x_ok = FALSE;
    BOOLEAN bGetCaps_y_ok = FALSE;

    Status = PTPFilter_WdfIoTargetFormatRequestForInternalIoctl(IOCTL_HID_GET_COLLECTION_INFORMATION, IoTarget, NULL, 0, (WDFMEMORY)NumberOfBytes, Size);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("PTPFilterCallHidClassSynchronously failed %!STATUS!", Status);
        return Status;
    }

    PVOID PreparsedData = ExAllocatePoolWithTag(NonPagedPool, (unsigned int)NumberOfBytes, MOUHID_TAG);
    pDeviceContext->PreparsedData = (PHIDP_PREPARSED_DATA)PreparsedData;

    ULONG DescriptorSize = NumberOfBytes->DescriptorSize;
    Status = PTPFilter_WdfIoTargetFormatRequestForInternalIoctl(IOCTL_HID_GET_COLLECTION_DESCRIPTOR, IoTarget, NULL, 0, (WDFMEMORY)PreparsedData, DescriptorSize);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("PTPFilterCallHidClassSynchronously failed %!STATUS!", Status);
        return Status;
    }
    
    Status = HidP_GetCaps(pDeviceContext->PreparsedData, &pDeviceContext->Capabilities);
    if (Status != HIDP_STATUS_SUCCESS)//(!NT_SUCCESS(Status))
    {
        /* failed to get capabilities */
        DbgPrint("HidP_GetCaps failed %!STATUS!", Status);
        ExFreePoolWithTag(PreparsedData, MOUHID_TAG);
        return Status;
    }

    HIDP_CAPS Caps = pDeviceContext->Capabilities;

    /* verify capabilities */
    if (!((Caps.UsagePage == HID_USAGE_PAGE_DIGITIZER && Caps.Usage == HID_USAGE_DIGITIZER_TOUCH_PAD) \
        || (Caps.UsagePage == HID_USAGE_PAGE_GENERIC && Caps.Usage == HID_USAGE_GENERIC_MOUSE)))
    {
        /* not supported */
        RegDebug(L"not supported Capabilities", NULL, Status);
        ExFreePoolWithTag(PreparsedData, MOUHID_TAG);
        return STATUS_UNSUCCESSFUL;
    }

    ULONG UsageLength = Caps.InputReportByteLength;
    if (!UsageLength)
    {
        DbgPrint("NumberInputButtonCaps is 0");
        return STATUS_UNSUCCESSFUL;
    }

    pDeviceContext->ReportLength = UsageLength;
    PVOID Buffer = ExAllocatePoolWithTag(NonPagedPool, 2 * sizeof(USAGE) * UsageLength, MOUHID_TAG);
    pDeviceContext->UsageListBuffer = Buffer;

    Status = PtpFilterGetMaxFingers(pDeviceContext);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("PtpFilterGetMaxFingers failed %!STATUS!", Status);
        return Status;
    }
    
    ULONG ValueCapsLength = Caps.NumberInputValueCaps;
    if (!ValueCapsLength)
    {
        DbgPrint("NumberInputValueCaps is 0");
        return STATUS_UNSUCCESSFUL;
    }
    pDeviceContext->ValueCapsLength = ValueCapsLength;
    PHIDP_VALUE_CAPS HidValueCaps = (PHIDP_VALUE_CAPS)ExAllocatePoolWithTag(NonPagedPool, 72 * ValueCapsLength, MOUHID_TAG);
    if (!HidValueCaps)
    {
        /* no memory */
        DbgPrint("ExAllocatePoolWithTag for value caps failed");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = HidP_GetValueCaps(HidP_Input, HidValueCaps, (PUSHORT)&ValueCapsLength, pDeviceContext->PreparsedData);
    if (!NT_SUCCESS(Status) || !ValueCapsLength) {
        DbgPrint("HidP_GetValueCaps failed %!STATUS!", Status);
        return Status;
    }

    int i = 0;
    do {
        if (!bGetCaps_x_ok && HidValueCaps[i].Range.UsageMin == HID_USAGE_GENERIC_X && HidValueCaps->UsagePage == HID_USAGE_PAGE_GENERIC) {
            ULONG Physical_Length_X = GetPhysicalValue(HidValueCaps[i].UnitsExp, HidValueCaps[i].Units, HidValueCaps[i].PhysicalMax, HidValueCaps[i].PhysicalMin);
            pDeviceContext->TouchPad_Physical_Length_X = Physical_Length_X;//触摸板x轴物理长度
            bGetCaps_x_ok = TRUE;
            pDeviceContext->TouchPad_Physical_DotDistance_X = Physical_Length_X/ (HidValueCaps[i].LogicalMax - HidValueCaps[i].LogicalMin);//触摸板x轴点距DotDistance
        }

        if (!bGetCaps_y_ok && HidValueCaps[i].Range.UsageMin == HID_USAGE_GENERIC_Y && HidValueCaps->UsagePage == HID_USAGE_PAGE_GENERIC) {
            ULONG Physical_Length_Y = GetPhysicalValue(HidValueCaps[i].UnitsExp, HidValueCaps[i].Units, HidValueCaps[i].PhysicalMax, HidValueCaps[i].PhysicalMin);
            pDeviceContext->TouchPad_Physical_Length_Y = Physical_Length_Y;//触摸板x轴物理长度
            bGetCaps_y_ok = TRUE;
            pDeviceContext->TouchPad_Physical_DotDistance_Y = Physical_Length_Y / (HidValueCaps[i].LogicalMax - HidValueCaps[i].LogicalMin);//触摸板y轴点距DotDistance
        }

        if (bGetCaps_x_ok && bGetCaps_x_ok) {
            break;
        }

        ++i;//
    } while (i != ValueCapsLength);


    ExFreePoolWithTag(HidValueCaps, MOUHID_TAG);
    DbgPrint("Hid_StartDevice");
    return STATUS_SUCCESS;

}

NTSTATUS PtpFilterGetMaxFingers(PDEVICE_CONTEXT pDeviceContext)
{
    NTSTATUS Status;

    USHORT HIDP_LINK_COLLECTION;
    ULONG LinkCollectionNodesLength[3];
    USHORT ValueCapsLength[2];
    HIDP_VALUE_CAPS ValueCaps;
    

    PHIDP_PREPARSED_DATA PreparsedData = pDeviceContext->PreparsedData;
    pDeviceContext->MaxFingerCount = 0;
    if (PreparsedData)
    {
        LinkCollectionNodesLength[0] = 0;
        Status = HidP_GetLinkCollectionNodes(0, LinkCollectionNodesLength, PreparsedData);
        if (Status == -1072627705)//0xc0110007
        {
            HIDP_LINK_COLLECTION = 1;
            ValueCapsLength[0] = 1;
            if (LinkCollectionNodesLength[0] > 1)
            {
                do
                {
                    Status = HidP_GetSpecificValueCaps(//status_v3
                        HidP_Input,
                        HID_USAGE_PAGE_DIGITIZER,
                        HIDP_LINK_COLLECTION,
                        0x51u,////USAGE_ID(Contact Identifier) //触摸点ID号
                        &ValueCaps,
                        ValueCapsLength,
                        pDeviceContext->PreparsedData);
                    if (Status == HIDP_STATUS_SUCCESS)
                    {
                        ++pDeviceContext->MaxFingerCount;
                    }

                    ++HIDP_LINK_COLLECTION;

                } while (HIDP_LINK_COLLECTION < LinkCollectionNodesLength[0]);
            }
            if (!pDeviceContext->MaxFingerCount)
            {
                Status = STATUS_BAD_DATA;//((NTSTATUS)0xC000090BL)
            }
        }
        else
        {
            DbgPrint("HidP_GetLinkCollectionNodes failed %!STATUS!", Status);
        }  
    }
    else
    {
        DbgPrint("PtpFilterGetMaxFingers failed %!STATUS!", Status);
    }

    DbgPrint("PtpFilterGetMaxFingers ok%!STATUS!", Status);
    return Status;
}


double GetPhysicalValue(ULONG UnitsExp, ULONG Units, LONG PhysicalMax, LONG PhysicalMin)
{
    double Result;//USHORT

    BYTE* pUnitExp;
    ULONG UnitSystemValue;

    USHORT Ratio;//单位制式尺成公制毫米比例 1尺=多少毫米
    USHORT ExponentValue;//指数值
    USHORT UnitValue;
    ULONG PhysicaDifference;

    char UnitExponent = UnitsExp & 0xF;//单位指数,取输入值的低4位
    Result = 0;

    UINT8 i = 0;
    pUnitExp = (BYTE*)UnitExponent_Table;//指针指向表格开头
    do
    {
        if (*pUnitExp == UnitExponent)
            break;
        ++i;
        pUnitExp += 4;//取表格内下个数据，4为单个数据长度
    } while (i < 0xB);

    ULONG UnitSystem = Units & 0xF;////表格竖列Column方向表示单位制式，行row表示单位用途种类
    if (UnitSystem < 5)
    {
        UnitSystemValue = Unit_TABLE[UnitSystem] - 1;
        if (UnitSystemValue)
        {
            if (UnitSystemValue != 2)
                return Result;
            Ratio = 2540;  //英制1英尺=2540毫米
        }
        else
        {
            Ratio = 1000; //公制1米=1000毫米
        }
        if (i < 0xB)
        {
            ExponentValue = UnitExponent_Table[2 * i + 1];
            UnitValue = ExponentTransfor(ExponentValue);
            PhysicaDifference = PhysicalMax - PhysicalMin;
            if (ExponentValue >= 0)
            {
                Result = UnitValue * PhysicaDifference * Ratio;
            }
            else if (UnitValue)
            {
                Result = PhysicaDifference * (unsigned int)Ratio / UnitValue;
            }
        }
    }
    return Result;
}

double ExponentTransfor(SHORT Value)
{
    int v1; // edx
    DOUBLE result; // rax
    bool v3; // zf

    v1 = 10;
    result = 1;
    v3 = Value == 0;
    if (Value < 0)
    {
        Value = -Value;
        v3 = Value == 0;
    }
    if (!v3)
    {
        do
        {
            if ((Value & 1) != 0)
                result = (unsigned int)(v1 * result);
            v1 *= v1;
            Value >>= 1;
        } while (Value);
    }
    return result;
}

NTSTATUS PTPFilter_WdfIoTargetFormatRequestForInternalIoctl(ULONG IoControlCode, WDFIOTARGET IoTarget, WDFMEMORY InputBuffer, ULONG InputBuffer_size, WDFMEMORY OutputBuffer, ULONG OutputBuffer_size)
{
    PWDFMEMORY_OFFSET InputBufferOffset, OutputBufferOffset;

    if (InputBuffer) {
        InputBufferOffset->BufferOffset = (size_t)InputBuffer;
        InputBufferOffset->BufferLength = InputBuffer_size;

    }

    if (OutputBuffer) {
        memset(OutputBuffer, 0, OutputBuffer_size);
        OutputBufferOffset->BufferOffset = (size_t)OutputBuffer;
        OutputBufferOffset->BufferLength = OutputBuffer_size;

    }

    return WdfIoTargetFormatRequestForInternalIoctl(IoTarget, NULL, IoControlCode, InputBuffer, InputBufferOffset, OutputBuffer, OutputBufferOffset);
}

void Delete_PLIST_ENTRY(PLIST_ENTRY ListHead)
{
    PLIST_ENTRY v2; // rax

    while (ListHead->Flink != ListHead)
    {
        v2 = RemoveHeadList(ListHead);
        FreePool_PLIST_ENTRY(v2);
    }
}

void FreePool_PLIST_ENTRY(PLIST_ENTRY a1)
{
    void* v2; // rcx

    v2 = (void*)a1[3];//v2 = (void*)a1[3];
    if (v2)
    {
        ExFreePoolWithTag(v2, MOUHID_TAG);
        a1[3] = 0i64;
    }
    ExFreePoolWithTag(a1, MOUHID_TAG);
}

NTSTATUS PTPFilterCreateDevice(_Inout_ PWDFDEVICE_INIT DeviceInit)
{
    NTSTATUS Status;
    WDFDEVICE hDevice;
    PDEVICE_CONTEXT pDeviceContext;
    WDF_OBJECT_ATTRIBUTES DeviceAttributes;
    WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;

    WDFQUEUE             queue;
    WDF_IO_QUEUE_CONFIG queueConfig;

    WdfFdoInitSetFilter(DeviceInit);
    //WdfDeviceInitSetPowerPageable(DeviceInit);
    //WdfPdoInitAllowForwardingRequestToParent(DeviceInit);

    WdfDeviceInitSetIoType(DeviceInit, WdfDeviceIoUndefined);

    //设置电源回调函数
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);

    //设备处于工作（供电D0状态）或者非工作状态
    pnpPowerCallbacks.EvtDevicePrepareHardware = PTPFilterDevicePrepareHardware;
    pnpPowerCallbacks.EvtDeviceReleaseHardware = PTPFilterDeviceReleaseHardware;


    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&DeviceAttributes, DEVICE_CONTEXT);

    Status = WdfDeviceCreate(&DeviceInit, &DeviceAttributes, &hDevice);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("WdfDeviceCreate failed %!STATUS!", Status);
        RegDebug(L"WdfDeviceCreate failed", NULL, Status);
        return Status;
    }

    pDeviceContext = GetDeviceContext(hDevice);

    pDeviceContext->hDevice = hDevice;
    pDeviceContext->IoTarget = WdfDeviceGetIoTarget(hDevice);

    pDeviceContext->RequestDataAvailableFlag = FALSE;
    pDeviceContext->RequestLength = 0;

    WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchParallel);

    queueConfig.EvtIoDeviceControl = PTPFilterEvtIoDeviceControl;
    //queueConfig.EvtIoInternalDeviceControl = PTPFilterEvtIoDeviceControl;
    queueConfig.EvtIoRead = PTPFilterEvtIoRead;

    Status = WdfIoQueueCreate(
        hDevice,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &queue);

    if (!NT_SUCCESS(Status)) {
        DbgPrint("WdfIoQueueCreate failed %!STATUS!", Status);
        RegDebug(L"WdfIoQueueCreate failed", NULL, Status);
        return Status;
    }

    WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchManual);

    Status = WdfIoQueueCreate(
        hDevice,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &pDeviceContext->ReportQueue_32);

    if (!NT_SUCCESS(Status)) {
        DbgPrint("WdfIoQueueCreate failed %!STATUS!", Status);
        RegDebug(L"WdfIoQueueCreate ReportQueue_32 failed", NULL, Status);
        return Status;
    }

    WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchManual);
    Status = WdfIoQueueCreate(
        hDevice,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &pDeviceContext->ReportQueue_56);

    if (!NT_SUCCESS(Status)) {
        DbgPrint("WdfIoQueueCreate failed %!STATUS!", Status);
        RegDebug(L"WdfIoQueueCreate ReportQueue_56 failed", NULL, Status);
        return Status;
    }

   // sub_140001038(pDeviceContext);//测试什么功能

    Status = WdfSpinLockCreate(NULL, &pDeviceContext->ReadLoopSpinLock);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("WdfSpinLockCreate for ReadLoopSpinLock failed %!STATUS!", Status);
        RegDebug(L"WdfSpinLockCreate ReadLoopSpinLock failed", NULL, Status);
        return Status;
    }

    Status = WdfSpinLockCreate(NULL, &pDeviceContext->ProcessedBufferSpinLock);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("WdfSpinLockCreate for ProcessedBufferSpinLock failed %!STATUS!", Status);
        RegDebug(L"WdfSpinLockCreate ProcessedBufferSpinLock failed", NULL, Status);
        return Status;
    }

    //Status = WdfDeviceCreateDeviceInterface(
    //    hDevice,
    //    &GUID_DEVINTERFACE_PtpFilterDriver,
    //    NULL // ReferenceString
    //);

    //if (NT_SUCCESS(Status))
    //{
    //    DbgPrint("WdfDeviceCreateDeviceInterface failed %!STATUS!", Status);
    //    RegDebug(L"WdfDeviceCreateDeviceInterface failed", NULL, Status);
    //    return Status;
    //}

    //BOOLEAN IsInterfaceEnabled = TRUE;
    //WdfDeviceSetDeviceInterfaceState(
    //    hDevice,
    //    &GUID_DEVINTERFACE_PtpFilterDriver,
    //    NULL, // ReferenceString
    //    IsInterfaceEnabled);

    RegDebug(L"PTPFilterCreateDevice ok", NULL, 0);
    return STATUS_SUCCESS;
}

void PTPFilterEvtIoRead(_In_ WDFQUEUE Queue, _In_ WDFREQUEST Request, _In_ size_t Length)
{
    WDFDEVICE Device = WdfIoQueueGetDevice(Queue);
    PDEVICE_CONTEXT pDeviceContext = GetDeviceContext(Device);
    BOOLEAN bRequestForwardFlag = FALSE;
    EVT_WDF_REQUEST_COMPLETION_ROUTINE PTPFilter_EvtRequestIoctlCompletionRoutine;

    WdfSpinLockAcquire(pDeviceContext->ReadLoopSpinLock);

    NTSTATUS Status = PTPFilterCheckAndStartReadLoop(pDeviceContext, Request, PTPFilter_EvtRequestIoctlCompletionRoutine);
    if (NT_SUCCESS(Status)) {

        Status = PTPFilterProcessCurrentRequest(pDeviceContext, Request, &bRequestForwardFlag);
        if (!NT_SUCCESS(Status)) {
            DbgPrint("PTPFilterProcessCurrentRequest failed %!STATUS!", Status);
            RegDebug(L"PTPFilterProcessCurrentRequest failed", NULL, Status);
        }
    }
    else {
        DbgPrint("PTPFilterCheckAndStartReadLoop failed %!STATUS!", Status);
        RegDebug(L"PTPFilterCheckAndStartReadLoop failed", NULL, Status);
    }

    DbgPrint("[PTP] %s : ", "PTPFilterEvtIoRead");
    RegDebug(L"PTPFilterEvtIoRead", NULL, Status);

    WdfSpinLockRelease(pDeviceContext->ReadLoopSpinLock);

    if (!bRequestForwardFlag) {
        WdfRequestComplete(Request, Status);
    }
}

NTSTATUS PTPFilterRewritePosition(PDEVICE_CONTEXT pDeviceContext,PLIST_ENTRY *PList_Entry)
{
    NTSTATUS Status;

    CHAR* Report_v21;
    ULONG UsageValue[4];
    USHORT HIDP_LINK_COLLECTION;
    int v28;
    USHORT i;
    __m128i v29[29];//    _OWORD v29[29];
    LONG v25;

    HIDP_LINK_COLLECTION = 1;
    while (1)
    {
        Status = PTPFilterReadContactId(UsageValue, HIDP_LINK_COLLECTION,  Report_v21, pDeviceContext);
        if (!NT_SUCCESS(Status)) {
            return Status;
        }

        for (i = 0; i < v28; ++i)////v28没有初始化？？？
        {
            if (LOWORD(v29[3 * i]) == UsageValue[0])
                break;
        }
        if (i == v28)
        {
            DbgPrint("Finger with contactId %d not found in Fingers", UsageValue[0]);
            return -1073741811;// STATUS_INVALID_PARAMETER         ((NTSTATUS)0xC000000DL)
        }
        v25 = 3i64 * i;
        long double PhysicalX_value = DWORD2(v29[3 * i + 2]);
        float TouchPad_Physical_DotDistance_X = pDeviceContext->TouchPad_Physical_DotDistance_X;

        Status = PTPFilterSetUsageValuePosX(PhysicalX_value / TouchPad_Physical_DotDistance_X, HIDP_LINK_COLLECTION, Report_v21, pDeviceContext);//UsageValue=PhysicalX_value/TouchPad_Physical_DotDistance_X逻辑坐标x=物理坐标x/点距
        if (Status != HIDP_STATUS_SUCCESS) {
            return Status;
        }

        long double PhysicalX_value = HIDWORD(v29[v25 + 2]);
        float TouchPad_Physical_DotDistance_Y = pDeviceContext->TouchPad_Physical_DotDistance_Y;
        Status = PTPFilterSetUsageValuePosY(PhysicalY_value / TouchPad_Physical_DotDistance_Y, HIDP_LINK_COLLECTION, Report_v21, pDeviceContext);//UsageValue=PhysicalY_value/TouchPad_Physical_DotDistance_Y逻辑坐标y=物理坐标y/点距

        if (Status != HIDP_STATUS_SUCCESS) {
            return Status;
        }
           
        if (HIDP_LINK_COLLECTION++ >= *((_DWORD*)v2 + 4))
            goto LABEL_14;
    }

    DbgPrint("[PTP] %s : ", "PTPFilterRewritePosition");
    return STATUS_SUCCESS;
}


NTSTATUS PTPFilterReadContactId(PULONG UsageValue, USHORT HIDP_LINK_COLLECTION, CHAR* Report, PDEVICE_CONTEXT pDeviceContext)
{
    NTSTATUS Status;
    *UsageValue = 0;

    Status = HidP_GetUsageValue(
        HidP_Input,
        HID_USAGE_PAGE_DIGITIZER,
        HIDP_LINK_COLLECTION,
        0x51u,//USAGE ID(Contact Identifier) //触摸点ID号
        UsageValue,//返回值Contact Identifier
        pDeviceContext->PreparsedData,
        Report,
        pDeviceContext->ReportLength);

    if (!NT_SUCCESS(Status)) {
        DbgPrint("HidP_GetUsageValue for HID_USAGE_CONTACTID failed with status - %!STATUS!", Status);
    }

    DbgPrint("[PTP] %s : ", "PTPFilterReadContactId");
    return Status;
}

NTSTATUS PTPFilterReadContactTX(PULONG UsageValue, USHORT HIDP_LINK_COLLECTION, CHAR* Report, PDEVICE_CONTEXT pDeviceContext)//未被调用？？
{
    NTSTATUS Status;

    *UsageValue = 0;
    Status = HidP_GetUsageValue(
        HidP_Input,
        HID_USAGE_PAGE_GENERIC,
        HIDP_LINK_COLLECTION,
        HID_USAGE_GENERIC_X,//USAGE(X)       X移动
        UsageValue,////获得X移动返回值
        pDeviceContext->PreparsedData,
        Report,
        pDeviceContext->ReportLength);

    if (!NT_SUCCESS(Status)) {
        DbgPrint("HidP_GetUsageValue for HID_USAGE_GENERIC_X failed with status - %!STATUS!", Status);
    }

    DbgPrint("[PTP] %s : ", "PTPFilterReadContactTX");
    return Status;
}

NTSTATUS PTPFilterReadContactTY(PULONG UsageValue, USHORT HIDP_LINK_COLLECTION, CHAR *Report, PDEVICE_CONTEXT pDeviceContext)//未被调用？？
{
    NTSTATUS Status;
    *UsageValue = 0;

    Status = HidP_GetUsageValue(
        HidP_Input,
        HID_USAGE_PAGE_GENERIC,
        HIDP_LINK_COLLECTION,
        HID_USAGE_GENERIC_Y,//USAGE(Y)       Y移动
        UsageValue,//获得Y移动返回值
        pDeviceContext->PreparsedData,
        Report,
        pDeviceContext->ReportLength);

    if (!NT_SUCCESS(Status)) {
        DbgPrint("HidP_GetUsageValue for HID_USAGE_GENERIC_Y failed with status - %!STATUS!", Status);
    }

    DbgPrint("[PTP] %s : ", "PTPFilterReadContactTY");
    return Status;
}


NTSTATUS PTPFilterSetUsageValuePosX(ULONG UsageValue, USHORT HIDP_LINK_COLLECTION, CHAR *Report, PDEVICE_CONTEXT pDeviceContext)
{
    return HidP_SetUsageValue(
        HidP_Input,
        HID_USAGE_PAGE_GENERIC,
        HIDP_LINK_COLLECTION,
        HID_USAGE_GENERIC_X,
        UsageValue,
        pDeviceContext->PreparsedData,
        Report,
        pDeviceContext->ReportLength);
}


NTSTATUS PTPFilterSetUsageValuePosY(ULONG UsageValue, USHORT HIDP_LINK_COLLECTION, CHAR *Report, PDEVICE_CONTEXT pDeviceContext)
{
    return HidP_SetUsageValue(
        HidP_Input,
        HID_USAGE_PAGE_GENERIC,
        HIDP_LINK_COLLECTION,
        HID_USAGE_GENERIC_Y,
        UsageValue,
        pDeviceContext->PreparsedData,
        Report,
        pDeviceContext->ReportLength);
}


NTSTATUS PTPFilterCheckAndStartReadLoop(PDEVICE_CONTEXT pDeviceContext, WDFREQUEST Request, EVT_WDF_REQUEST_COMPLETION_ROUTINE PTPFilter_EvtRequestIoctlCompletionRoutine)
{
    NTSTATUS Status = STATUS_SUCCESS;
    if (!pDeviceContext->RequestDataAvailableFlag && !pDeviceContext->RequestLength)
    {
        pDeviceContext->RequestDataAvailableFlag = TRUE; 

        DbgPrint("Read Loop Started");

        PIRP prp = WdfRequestWdmGetIrp(Request);

        PHID_XFER_PACKET pHidPacket = (PHID_XFER_PACKET)WdfRequestWdmGetIrp(Request)->UserBuffer; 

        PLIST_ENTRY p1 = (PLIST_ENTRY)pHidPacket->reportBuffer;

        pDeviceContext->member_24 = p1->Blink;

        Status = PTPFilterSendReadRequest(pDeviceContext, PTPFilter_EvtRequestIoctlCompletionRoutine);
        if (!NT_SUCCESS(Status)) {

            DbgPrint("PTPFilterSendReadRequest failed %!STATUS!", Status);

            pDeviceContext->RequestDataAvailableFlag = FALSE;
        }
    }

    return Status;
}


NTSTATUS PTPFilterSendReadRequest(PDEVICE_CONTEXT pDeviceContext, EVT_WDF_REQUEST_COMPLETION_ROUTINE PTPFilter_EvtRequestIoctlCompletionRoutine)
{
    WDFREQUEST Request = pDeviceContext->ReuseRequest;

    /////重新初始化
    WDF_REQUEST_REUSE_PARAMS reuseParams;

    WDF_REQUEST_REUSE_PARAMS_INIT(
        &reuseParams,
        WDF_REQUEST_REUSE_NO_FLAGS,
        STATUS_SUCCESS
    );

    NTSTATUS Status = WdfRequestReuse(Request, &reuseParams);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("WdfRequestReuse failed %!STATUS!", Status);
        return Status;
    }

    //测试pDeviceContext->OutputBuffer = pDeviceContext->RequestBuffer;
    Status = WdfIoTargetFormatRequestForRead(pDeviceContext->IoTarget, Request, pDeviceContext->OutputBuffer, 0, 0);//pDeviceContext->OutputBuffer未初始化？并且pDeviceContext->RequestBuffer也未被调用处理，

    if (!NT_SUCCESS(Status)) {
        DbgPrint("WdfIoTargetFormatRequestForRead failed %!STATUS!", Status);
        return Status;
    }

    WdfRequestSetCompletionRoutine(Request, PTPFilter_EvtRequestIoctlCompletionRoutine, pDeviceContext);

    PIRP prp = WdfRequestWdmGetIrp(Request);

    PHID_XFER_PACKET pHidPacket = (PHID_XFER_PACKET)WdfRequestWdmGetIrp(Request)->UserBuffer;

    PLIST_ENTRY p1 = (PLIST_ENTRY)pHidPacket->reportBuffer;

    //p1->Flink = pDeviceContext->member_3;

    BOOLEAN ret = WdfRequestSend(Request, pDeviceContext->IoTarget, NULL);

    if (!ret) {
        Status = WdfRequestGetStatus(pDeviceContext->ReuseRequest);

        DbgPrint("WdfRequestSend failed %!STATUS!", Status);
        return Status;
    }

    DbgPrint("[PTP] %s : ", "PTPFilterSendReadRequest");
    return Status;
}

void PTPFilter_EvtRequestIoctlCompletionRoutine(WDFREQUEST Request, WDFIOTARGET Target, PWDF_REQUEST_COMPLETION_PARAMS Params, WDFCONTEXT Context)
{
    PDEVICE_CONTEXT pDeviceContext =(PDEVICE_CONTEXT) Context;
    BOOLEAN bRequestStopFlag = TRUE;////Pending_v16=TRUE;??

    NTSTATUS Status = WdfRequestGetStatus(Request);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("Read request failed with status - %!STATUS!", Status);

        PTPFilterProcessReadReport(pDeviceContext, Status, &bRequestStopFlag);////Pending_v16;??
        if (!bRequestStopFlag) {
            return;
        }
    }

    Status = PTPFilterHandleDeviceData(pDeviceContext);//此处获取输入报告数据并处理
    if (!NT_SUCCESS(Status)) {
        DbgPrint("PTPFilterHandleDeviceData failed with status - %!STATUS!", Status);
    }

    Status = PTPFilterSendReadRequest(pDeviceContext, PTPFilter_EvtRequestIoctlCompletionRoutine);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("PTPFilterSendReadRequest failed unexpectedly %!STATUS!", Status);
        pDeviceContext->RequestDataAvailableFlag = FALSE;
    }

    DbgPrint("[PTP] %s : ", "PTPFilterOnDeviceDataAvailable");
}

NTSTATUS PTPFilterHandleDeviceData(PDEVICE_CONTEXT pDeviceContext)//处理设备数据
{
    NTSTATUS Status = STATUS_SUCCESS;
    WDFSPINLOCK ProcessedBufferSpinLock = pDeviceContext->ProcessedBufferSpinLock;

    ULONG CurrentFingersCountValue = 0;

    BOOLEAN boolFlag_FingerNotChanged =FALSE;

    WdfSpinLockAcquire(ProcessedBufferSpinLock);
    GettingButtonUsage(pDeviceContext);

    Status = PTPFilterGetFingersCount(pDeviceContext, &CurrentFingersCountValue, &boolFlag_FingerNotChanged);
    if (!NT_SUCCESS(Status)) {
    
        DbgPrint("PTPFilterGetFingersCount failed with status - %!STATUS!", Status);

        //此处已经获取到了手指输入报告后进行数据保存
        Status =PTPFilterBufferStoreReport(pDeviceContext->LIST_ENTRY_944, pDeviceContext->REPORT_BUFFER_SIZE, pDeviceContext->ReportBuffer,pDeviceContext->ReportLength, CurrentFingersCountValue);
        if (NT_SUCCESS(Status)) {

            sub_140001058((PLIST_ENTRY)(a1 + 928), (_LIST_ENTRY*)(a1 + 944), &v14);//pDeviceContext_a1->LIST_ENTRY_928, pDeviceContext_a1->LIST_ENTRY_944
            LODWORD(v10) = v14;

            DbgPrint("PTPFilterHandleDeviceData - No fingers in report. PTPFilterBufferProcessData, rawDataProcessed: %d",v14);

            Status =PTPFilterProcessPendingRequest(pDeviceContext);
            if (NT_SUCCESS(Status)) {
                goto Exit;
            }

            DbgPrint("PTPFilterProcessPendingRequest failed with status - %!STATUS!", Status);
            goto Exit;
        }

        DbgPrint("PTPFilterBufferStoreReport failed with status - %!STATUS!", Status);
        goto Exit;
    }

    Status = PTPFilterBufferStoreReport(pDeviceContext->LIST_ENTRY_928, pDeviceContext->REPORT_BUFFER_SIZE, pDeviceContext->ReportBuffer, pDeviceContext->ReportLength, CurrentFingersCountValue);
    if (!NT_SUCCESS(Status)) {

        DbgPrint("PTPFilterBufferStoreReport failed with status - %!STATUS!", Status);
        goto Exit;
    }
    if (boolFlag_FingerNotChanged)
    {
        Status = PTPFilterProcessInputFrame(pDeviceContext);
        if (!NT_SUCCESS(Status)) {
            DbgPrint("PTPFilterProcessInputFrame failed with status - %!STATUS!", Status);
            pDeviceContext->LastFingerCount = 0;
            pDeviceContext->CurrentFingerCount = 0;
        }
    }
Exit:
    DbgPrint("PTPFilterHandleDeviceData with status - %!STATUS!", Status);
    
    WdfSpinLockRelease (pDeviceContext->ProcessedBufferSpinLock);
    return Status;

}

NTSTATUS PTPFilterProcessPendingRequest(PDEVICE_CONTEXT pDeviceContext)
{    
    WDFREQUEST OutRequest;

    NTSTATUS Status = WdfIoQueueRetrieveNextRequest(pDeviceContext->ReportQueue_56, &OutRequest);
    if (NT_SUCCESS(Status)) {
        PLIST_ENTRY v3 = RemoveHeadList(pDeviceContext->LIST_ENTRY_944);
        Status = PTPFilterCompleteReadRequest (OutRequest, v3[1].Blink,  pDeviceContext->ReportLength);

        FreePool_PLIST_ENTRY(v3);

        if (!NT_SUCCESS(Status)) {
            DbgPrint("PTPFilterCompleteReadRequest failed with status - %!STATUS!", Status);
            return Status;
        }
    }

    DbgPrint("[PTP] %s : ", "PTPFilterProcessPendingRequest");
    return Status;
}


NTSTATUS PTPFilterBufferStoreReport(LIST_ENTRY List_Entry, SHORT REPORT_BUFFER_SIZE, PCHAR ReportBuffer, ULONG ReportLength, UCHAR CurrentFingersCountValue)
{
    NTSTATUS Status = STATUS_SUCCESS;
    STRUCT_TOUCHPAD_PACKET*  Struct_tp_packet = (STRUCT_TOUCHPAD_PACKET*)ExAllocatePoolWithTag(NonPagedPool, sizeof(STRUCT_TOUCHPAD_PACKET), MOUHID_TAG);//30 length手指数据长度

    if (Struct_tp_packet) {

        STRUCT_TOUCHPAD_FINGER* Struct_tp_finger_data = (STRUCT_TOUCHPAD_FINGER*)ExAllocatePoolWithTag(NonPagedPool, REPORT_BUFFER_SIZE, MOUHID_TAG); //REPORT_BUFFER_SIZE = 手指个数 * sizeof(STRUCT_TOUCHPAD_FINGER)
        Struct_tp_packet-> 3   = Struct_tp_finger_data;

        if (Struct_tp_finger_data) {
            memset(Struct_tp_finger_data, 0, REPORT_BUFFER_SIZE);

            if (REPORT_BUFFER_SIZE >= ReportLength) {
                memmove(Struct_tp_finger_data->FingerData_3, ReportBuffer, ReportLength);
            }

            Struct_tp_packet->NumOfFingers_4 = CurrentFingersCountValue;

            LIST_ENTRY v12 = List_Entry-> 8;
            Struct_tp_packet = LIST_ENTRY;
            Struct_tp_packet->1 =v12;
            if (*v12 != List_Entry) {
                RtlFailFast(FAST_FAIL_STACK_COOKIE_CHECK_FAILURE);//__fastfail(FAST_FAIL_STACK_COOKIE_CHECK_FAILURE);//RtlFailFast(_In_ ULONG Code)
            }

            *v12 = Struct_tp_packet;
            List_Entry-> 8 = Struct_tp_packet;
        }
        else
        {
            DbgPrint("ExAllocatePoolWithTag for input report failed");

            Status = STATUS_MEMORY_NOT_ALLOCATED; //     ((NTSTATUS)0xC00000A0L)
            ExFreePoolWithTag(Struct_tp_finger_data, MOUHID_TAG);///0x50747046u=MOUHID_TAG
        }
    }
    else
    {
        DbgPrint("ExAllocatePoolWithTag for list entry failed");
        Status = STATUS_MEMORY_NOT_ALLOCATED;//     ((NTSTATUS)0xC00000A0L)
    }

    DbgPrint("[PTP] %s : ", "PTPFilterBufferStoreReport");
    return Status;
}

void PTPFilterProcessReadReport(PDEVICE_CONTEXT pDeviceContext, NTSTATUS Status_In, BOOLEAN *bRequestStopFlag)
{

    WDFREQUEST OutRequest;

    if (pDeviceContext->RequestLength)//
    {
       pDeviceContext->RequestDataAvailableFlag = FALSE;
        *bRequestStopFlag = FALSE;////Pending_a3=FALSE;??
    }
    else
    {
        WdfSpinLockAcquire(pDeviceContext->ReadLoopSpinLock);
        NTSTATUS Status = WdfIoQueueRetrieveNextRequest(pDeviceContext->ReportQueue_56, &OutRequest);
        if (NT_SUCCESS(Status)) {

            *bRequestStopFlag = TRUE;//Pending_a3=TRUE;??
            WdfSpinLockRelease (pDeviceContext->ReadLoopSpinLock);
            WdfRequestComplete(OutRequest, Status_In);//??
        }
        else
        {
            pDeviceContext->RequestDataAvailableFlag = FALSE;
            *bRequestStopFlag = FALSE;////Pending_a3=FALSE;??
            WdfSpinLockRelease (pDeviceContext->ReadLoopSpinLock);
        }
    }

}

NTSTATUS PTPFilterGetFingersCount(PDEVICE_CONTEXT pDeviceContext, ULONG *CurrentFingersCountValue, BOOLEAN *boolFlag_FingerNotChanged)
{
    NTSTATUS Status;

    *boolFlag_FingerNotChanged = FALSE;
    Status = HidP_GetUsageValue(
        HidP_Input,
        HID_USAGE_PAGE_DIGITIZER,
        HIDP_LINK_COLLECTION_UNSPECIFIED,
        0x54u,//USAGE ID (Contact count)  =54 //Contact Count	Total number of contacts to be reported in a given report.
        CurrentFingersCountValue,
        pDeviceContext->PreparsedData,
        (PCHAR)pDeviceContext->ReportBuffer,
        pDeviceContext->ReportLength);

    if (!NT_SUCCESS(Status)) {

        DbgPrint("Could not obtain actual count from OEM HID report - %!STATUS!", Status);
        return Status;
    }

    if (pDeviceContext->LastFingerCount){
        if (*CurrentFingersCountValue)
        {
            DbgPrint("Expected report with 0 actual count in OEM HID report");
        LABEL_6:
            DbgPrint("\n");
            return STATUS_BAD_DATA;//((NTSTATUS)0xC000090BL)
        }
    }
    else{
        if (!*CurrentFingersCountValue){

            DbgPrint("Non-zero actual count expected in OEM HID report");
            return STATUS_BAD_DATA;//((NTSTATUS)0xC000090BL)
        }
        
        pDeviceContext->CurrentFingerCount = *CurrentFingersCountValue;
    }
    
    UCHAR FingerChangeCount = pDeviceContext->CurrentFingerCount- pDeviceContext->LastFingerCount;
    if (pDeviceContext->MaxFingerCount < FingerChangeCount) {
        FingerChangeCount = pDeviceContext->MaxFingerCount;
    }
    
    *CurrentFingersCountValue = FingerChangeCount;
    pDeviceContext->LastFingerCount += FingerChangeCount ;
    if ( pDeviceContext->LastFingerCount == pDeviceContext->CurrentFingerCount ) 
        *boolFlag_FingerNotChanged = TRUE;

    return Status;
}


void PTPFilterEvtIoDeviceControl(
    IN WDFQUEUE     Queue,
    IN WDFREQUEST   Request,
    IN size_t       OutputBufferLength,
    IN size_t       InputBufferLength,
    IN ULONG        IoControlCode)
{
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(OutputBufferLength);

    NTSTATUS Status = STATUS_SUCCESS;
    WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    PDEVICE_CONTEXT pDeviceContext = GetDeviceContext(device);

    UNREFERENCED_PARAMETER(pDeviceContext);

    //__m128i* Outresult_buffer_v26;
    //size_t Outresult_bufferSize_v25;//ULONG
    //__int32 v19; // ecx
    //char v20; // al
    //unsigned __int32 v21; // ebx
    //__int64 v22; // [rsp+20h] [rbp-60h]
    //__int64 v23; // [rsp+28h] [rbp-58h]
    //__int64 v24; // [rsp+28h] [rbp-58h]
    //__m128i v27; // [rsp+60h] [rbp-20h]

    ULONG IoControlCode_v13 = IOCTL_HID_GET_DEVICE_DESCRIPTOR - 16317;   //测试

    RegDebug(L"STATUS_NOT_SUPPORTED IoControlCode_v13-4", NULL, IoControlCode_v13-4);
    RegDebug(L"STATUS_NOT_SUPPORTED FUNCTION_FROM_CTL_CODE IoControlCode_v13-4", NULL, FUNCTION_FROM_CTL_CODE(IoControlCode_v13-4));

    RegDebug(L"STATUS_NOT_SUPPORTED IoControlCode_v13-8", NULL, IoControlCode_v13 - 8);
    RegDebug(L"STATUS_NOT_SUPPORTED FUNCTION_FROM_CTL_CODE IoControlCode_v13-8", NULL, FUNCTION_FROM_CTL_CODE(IoControlCode_v13 - 8));

    switch (IoControlCode)
    {
    case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
    {

        RegDebug(L"IOCTL_HID_GET_DEVICE_DESCRIPTOR", NULL, IoControlCode);
        /////
        WDFMEMORY memory;
        Status = WdfRequestRetrieveOutputMemory(Request, &memory);
        if (!NT_SUCCESS(Status))break;

        Status = WdfMemoryCopyFromBuffer(memory, 0, (PVOID)&DefaultHidDescriptor, DefaultHidDescriptor.bLength);
        if (!NT_SUCCESS(Status))break;
        ////
        WdfRequestSetInformation(Request, DefaultHidDescriptor.bLength);

    }
    break;

    case IOCTL_HID_GET_REPORT_DESCRIPTOR:
    {
        RegDebug(L"IOCTL_HID_GET_REPORT_DESCRIPTOR", NULL, IoControlCode);
        WDFMEMORY memory;
        Status = WdfRequestRetrieveOutputMemory(Request, &memory);
        if (!NT_SUCCESS(Status)) {
            break;
        }

        LONG outlen = DefaultHidDescriptor.DescriptorList[0].wReportLength;
        Status = WdfMemoryCopyFromBuffer(memory,
            0,
            (PVOID)MouseReportDescriptor,  //TouchpadReportDescriptor  MouseReportDescriptor
            outlen);
        if (!NT_SUCCESS(Status)) {
            break;
        }
        ////
        WdfRequestSetInformation(Request, outlen);

    }
    break;

    case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
    {
        RegDebug(L"IOCTL_HID_GET_DEVICE_ATTRIBUTES", NULL, IoControlCode);
        PHID_DEVICE_ATTRIBUTES   attr = NULL;
        Status = WdfRequestRetrieveOutputBuffer(Request,
            sizeof(HID_DEVICE_ATTRIBUTES),
            (PVOID*)&attr,
            NULL);
        if (!NT_SUCCESS(Status)) {
            break;
        }
        ////
        RtlZeroMemory(attr, sizeof(HID_DEVICE_ATTRIBUTES));
        attr->Size = sizeof(HID_DEVICE_ATTRIBUTES);

        attr->VendorID = 0x05AC;   //
        attr->ProductID = 0x0277;  //
        attr->VersionNumber = 0x0101;
        ///
        WdfRequestSetInformation(Request, sizeof(HID_DEVICE_ATTRIBUTES));
    }
    break;

    case IOCTL_HID_SET_FEATURE:
    {
        ////
        RegDebug(L"IOCTL_HID_SET_FEATURE", NULL, IoControlCode);
    }
    break;

    case IOCTL_HID_READ_REPORT:

    break;

    case IOCTL_HID_GET_FEATURE://测试代码
    {
        RegDebug(L"IOCTL_HID_GET_FEATURE", NULL, IoControlCode);
    }
    break;

    case IOCTL_HID_GET_STRING:RegDebug(L"IOCTL_HID_GET_STRING", NULL, IoControlCode); break;
    case IOCTL_UMDF_HID_SET_OUTPUT_REPORT:RegDebug(L"IOCTL_UMDF_HID_SET_OUTPUT_REPORT", NULL, IoControlCode); break;
    case IOCTL_UMDF_HID_GET_INPUT_REPORT:RegDebug(L"IOCTL_UMDF_HID_GET_INPUT_REPORT", NULL, IoControlCode); break;
    case IOCTL_HID_ACTIVATE_DEVICE:RegDebug(L"IOCTL_HID_ACTIVATE_DEVICE", NULL, IoControlCode); break;
    case IOCTL_HID_DEACTIVATE_DEVICE:RegDebug(L"IOCTL_HID_DEACTIVATE_DEVICE", NULL, IoControlCode); break;
    case IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST:RegDebug(L"IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST", NULL, IoControlCode); break;

    case IOCTL_UMDF_GET_PHYSICAL_DESCRIPTOR:RegDebug(L"IOCTL_UMDF_GET_PHYSICAL_DESCRIPTOR", NULL, IoControlCode); break;
    case IOCTL_UMDF_HID_GET_FEATURE:RegDebug(L"IOCTL_UMDF_HID_GET_FEATURE", NULL, IoControlCode); break;
    case IOCTL_UMDF_HID_SET_FEATURE:RegDebug(L"IOCTL_UMDF_HID_SET_FEATURE", NULL, IoControlCode); break;


    case IOCTL_HID_GET_DRIVER_CONFIG:RegDebug(L"IOCTL_HID_GET_DRIVER_CONFIG", NULL, IoControlCode); break;
    case IOCTL_HID_SET_DRIVER_CONFIG:RegDebug(L"IOCTL_HID_SET_DRIVER_CONFIG", NULL, IoControlCode); break;
    case IOCTL_HID_GET_POLL_FREQUENCY_MSEC:RegDebug(L"IOCTL_HID_GET_POLL_FREQUENCY_MSEC", NULL, IoControlCode); break;
    case IOCTL_HID_SET_POLL_FREQUENCY_MSEC:RegDebug(L"IOCTL_HID_SET_POLL_FREQUENCY_MSEC", NULL, IoControlCode); break;
    case IOCTL_GET_NUM_DEVICE_INPUT_BUFFERS:RegDebug(L"IOCTL_GET_NUM_DEVICE_INPUT_BUFFERS", NULL, IoControlCode); break;
    case IOCTL_SET_NUM_DEVICE_INPUT_BUFFERS:RegDebug(L"IOCTL_SET_NUM_DEVICE_INPUT_BUFFERS", NULL, IoControlCode); break;
    case IOCTL_HID_GET_COLLECTION_INFORMATION:RegDebug(L"IOCTL_HID_GET_COLLECTION_INFORMATION", NULL, IoControlCode); break;
    case IOCTL_HID_ENABLE_WAKE_ON_SX:RegDebug(L"IOCTL_HID_ENABLE_WAKE_ON_SX", NULL, IoControlCode); break;
    case IOCTL_HID_SET_S0_IDLE_TIMEOUT:RegDebug(L"IOCTL_HID_SET_S0_IDLE_TIMEOUT", NULL, IoControlCode); break;
    case IOCTL_HID_GET_COLLECTION_DESCRIPTOR:RegDebug(L"IOCTL_HID_GET_COLLECTION_DESCRIPTOR", NULL, IoControlCode); break;
    case IOCTL_HID_FLUSH_QUEUE:RegDebug(L"IOCTL_HID_FLUSH_QUEUE", NULL, IoControlCode); break;
    case IOCTL_GET_PHYSICAL_DESCRIPTOR:RegDebug(L"IOCTL_GET_PHYSICAL_DESCRIPTOR", NULL, IoControlCode); break;
    case IOCTL_HID_GET_HARDWARE_ID:RegDebug(L"IOCTL_HID_GET_HARDWARE_ID", NULL, IoControlCode); break;
    case IOCTL_HID_SET_OUTPUT_REPORT:RegDebug(L"IOCTL_HID_SET_OUTPUT_REPORT", NULL, IoControlCode); break;
    case IOCTL_HID_GET_INPUT_REPORT:RegDebug(L"IOCTL_HID_GET_INPUT_REPORT", NULL, IoControlCode); break;
    case IOCTL_HID_GET_OUTPUT_REPORT:RegDebug(L"IOCTL_HID_GET_OUTPUT_REPORT", NULL, IoControlCode); break;
    case IOCTL_HID_GET_MANUFACTURER_STRING:RegDebug(L"IOCTL_HID_GET_MANUFACTURER_STRING", NULL, IoControlCode); break;
    case IOCTL_HID_GET_PRODUCT_STRING:RegDebug(L"IOCTL_HID_GET_PRODUCT_STRING", NULL, IoControlCode); break;
    case IOCTL_HID_GET_SERIALNUMBER_STRING:RegDebug(L"IOCTL_HID_GET_SERIALNUMBER_STRING", NULL, IoControlCode); break;
    case IOCTL_HID_GET_INDEXED_STRING:RegDebug(L"IOCTL_HID_GET_INDEXED_STRING", NULL, IoControlCode); break;
    case IOCTL_HID_GET_MS_GENRE_DESCRIPTOR:RegDebug(L"IOCTL_HID_GET_MS_GENRE_DESCRIPTOR", NULL, IoControlCode); break;
    case IOCTL_HID_ENABLE_SECURE_READ:RegDebug(L"IOCTL_HID_ENABLE_SECURE_READ", NULL, IoControlCode); break;
    case IOCTL_HID_DISABLE_SECURE_READ:RegDebug(L"IOCTL_HID_DISABLE_SECURE_READ", NULL, IoControlCode); break;
    case IOCTL_HID_DEVICERESET_NOTIFICATION:////status = STATUS_NOT_SUPPORTED; break;//测试代码确定TouchpadReportDescriptor会执行到这，MouseReportDescriptor则不会
    default:
        RegDebug(L"STATUS_NOT_SUPPORTED IoControlCode", NULL, IoControlCode);
        RegDebug(L"STATUS_NOT_SUPPORTED FUNCTION_FROM_CTL_CODE", NULL, FUNCTION_FROM_CTL_CODE(IoControlCode));

        Status = STATUS_NOT_SUPPORTED;
    }

    /////complete irp
    WdfRequestComplete(Request, Status);

    if (!IoControlCode_v13) {
        
        //Status = WdfRequestRetrieveInputBuffer(Request, InputBufferLength, (PVOID *)&Outresult_buffer_v26, &Outresult_bufferSize_v25);
        /*if (Status >= 0 && Outresult_bufferSize_v25 == 20) {
            v19 = Outresult_buffer_v26[1].m128i_i32[0];
            v20 = _mm_cvtsi128_si32(*Outresult_buffer_v26);
            v27 = *Outresult_buffer_v26;
            *(_BYTE*)v12 = v20;
            if (v20)
            {
                *(_QWORD*)(v12 + 80) = *(__int64*)((char*)v27.m128i_i64 + 4);
                *(_DWORD*)(v12 + 88) = v27.m128i_i32[3] * v27.m128i_i32[3];
                *(_DWORD*)(v12 + 92) = v19 * v19;
            }
            v21 = v27.m128i_i32[0];
            LODWORD(v23) = v27.m128i_i32[0];
            DbgPrint("START_STOP_FILTERING PARAMS, state: %d", v21);
            DbgPrint("\n");
        }*/
    }
}

NTSTATUS  PTPFilterProcessCurrentRequest(PDEVICE_CONTEXT pDeviceContext, WDFREQUEST Request, BOOLEAN *bRequestForwardFlag)
{
    NTSTATUS Status = STATUS_SUCCESS;
    *bRequestForwardFlag = FALSE;

    WdfSpinLockAcquire(pDeviceContext->ProcessedBufferSpinLock);

    PLIST_ENTRY v7 = pDeviceContext->LIST_ENTRY_944;

    if (v7->Flink != v7)
    {
        PLIST_ENTRY v8 = RemoveHeadList(v7);
        WdfSpinLockRelease (pDeviceContext->ProcessedBufferSpinLock);

        Status = PTPFilterCompleteReadRequest(Request, v8[1].Blink, pDeviceContext->ReportLength);
        FreePool_PLIST_ENTRY(v8);

        if (!NT_SUCCESS(Status))
        {
            DbgPrint("WdfRequestForwardToIoQueue failed %!STATUS!", Status);;
            return Status;
        }

        *bRequestForwardFlag = TRUE;
        return Status;
    }
    
    WdfSpinLockRelease (pDeviceContext->ProcessedBufferSpinLock);

    if (!*bRequestForwardFlag)
    {
        Status = WdfRequestForwardToIoQueue(Request, pDeviceContext->ReportQueue_56);
        if (!NT_SUCCESS(Status))
        {
            DbgPrint("WdfRequestForwardToIoQueue failed %!STATUS!", Status);
            return Status;
        }

        *bRequestForwardFlag = TRUE;
    }

    return Status;
}

NTSTATUS PTPFilterCompleteReadRequest(WDFREQUEST Request, PVOID SourceBuffer, ULONG_PTR Information_size)
{
    PVOID OutBuffer;
    size_t Size;

    NTSTATUS Status = WdfRequestRetrieveOutputBuffer(Request, Information_size, &OutBuffer, &Size);
    if (NT_SUCCESS(Status))
    {
        memset(OutBuffer, 0, Size);
        if (Size >= Information_size) {
            memmove(OutBuffer, SourceBuffer, Information_size);//OutBuffer未被调用？？？ 并且pDeviceContext->RequestBuffer也未被调用处理，

            //测试pDeviceContext->RequestBuffer = OutBuffer;
        }
            
        WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, Information_size);
    }
    else
    {
        DbgPrint("WdfRequestRetrieveOutputBuffer failed with status - %!STATUS!", Status);
    }

    return Status;

}

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
