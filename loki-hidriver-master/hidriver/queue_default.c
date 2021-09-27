#include "queue_default.h"
#include "queue_manual.h"
#include "device.h"
#include "memory.h"

ULONG runtimes = 0;

_Use_decl_annotations_
NTSTATUS
QueueDefaultCreate(
    _In_ WDFDEVICE device,
    _Out_ WDFQUEUE *queueOut)
{
    NTSTATUS                  status       = STATUS_SUCCESS;
    WDFQUEUE                  queue        = NULL;
    PQUEUE_DEFAULT_CONTEXT    context      = NULL;
    WDF_IO_QUEUE_CONFIG       config;
    WDF_OBJECT_ATTRIBUTES     attributes;

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&config, WdfIoQueueDispatchSequential);
    config.EvtIoInternalDeviceControl = QueueDefaultEvtIoDeviceControl;

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, QUEUE_DEFAULT_CONTEXT);

    status = WdfIoQueueCreate(device, &config, &attributes, &queue);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    context = QueueDefaultGetContext(queue);
    context->device      = device;
    context->queue       = queue;
    *queueOut            = queue;

    return status;
}

_Use_decl_annotations_
VOID
QueueDefaultEvtIoDeviceControl(
    _In_ WDFQUEUE   queue,
    _In_ WDFREQUEST request,
    _In_ size_t     outputBufferLength,
    _In_ size_t     inputBufferLength,
    _In_ ULONG      ioControlCode)
{
    UNREFERENCED_PARAMETER(outputBufferLength);
    UNREFERENCED_PARAMETER(inputBufferLength);

    NTSTATUS                status               = STATUS_SUCCESS;
    PDEVICE_CONTEXT         deviceContext        = DeviceGetContext(QueueDefaultGetContext(queue)->device);
    runtimes++;

    switch (ioControlCode) {
    case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
        RegDebug(L"QueueDefaultEvtIoDeviceControl IOCTL_HID_GET_DEVICE_DESCRIPTOR", NULL, runtimes);
        status = CopyToRequestBuffer(
            request,
            deviceContext->hidDescriptor,
            deviceContext->hidDescriptor->bLength
        );
        WdfRequestComplete(request, status);
        break;
    case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
        RegDebug(L"QueueDefaultEvtIoDeviceControl IOCTL_HID_GET_DEVICE_ATTRIBUTES", NULL, runtimes);
        status = CopyToRequestBuffer(
            request,
            &deviceContext->hidDeviceAttributes,
            sizeof(HID_DEVICE_ATTRIBUTES)
        );
        WdfRequestComplete(request, status);
        break;
    case IOCTL_HID_GET_REPORT_DESCRIPTOR:
        RegDebug(L"QueueDefaultEvtIoDeviceControl IOCTL_HID_GET_REPORT_DESCRIPTOR", NULL, runtimes);
        status = CopyToRequestBuffer(
            request,
            deviceContext->hidReportDescriptor,
            deviceContext->hidDescriptor->DescriptorList[0].wReportLength
        );
        WdfRequestComplete(request, status);
        break;
    case IOCTL_HID_READ_REPORT:
        RegDebug(L"QueueDefaultEvtIoDeviceControl IOCTL_HID_READ_REPORT", NULL, runtimes);
    case IOCTL_HID_GET_INPUT_REPORT:
        RegDebug(L"QueueDefaultEvtIoDeviceControl IOCTL_HID_GET_INPUT_REPORT", NULL, runtimes);
        status = WdfRequestForwardToIoQueue(
            request,
            deviceContext->queueManual
        );
        if (!NT_SUCCESS(status)) {
            WdfRequestComplete(request, status);
        }
        break;
    case IOCTL_HID_WRITE_REPORT:
        RegDebug(L"QueueDefaultEvtIoDeviceControl IOCTL_HID_WRITE_REPORT", NULL, runtimes);
    case IOCTL_HID_SET_OUTPUT_REPORT:
        RegDebug(L"QueueDefaultEvtIoDeviceControl IOCTL_HID_SET_OUTPUT_REPORT", NULL, runtimes);
        status = QueueManualSendReport(
            request,
            deviceContext
        );
        WdfRequestComplete(request, status);
        break;
    default:
        RegDebug(L"QueueDefaultEvtIoDeviceControl STATUS_NOT_IMPLEMENTED", NULL, runtimes);
        status = STATUS_NOT_IMPLEMENTED;
        WdfRequestComplete(request, status);
        break;
    }
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