/*++
Copyright (c) 2008  Microsoft Corporation

Module Name:

    moufiltr.h

Abstract:

    This module contains the common private declarations for the mouse
    packet filter

Environment:

    kernel mode only

Notes:


Revision History:


--*/

#ifndef MOUFILTER_H
#define MOUFILTER_H

#include <ntddk.h>
#include <kbdmou.h>
#include <ntddmou.h>
#include <ntdd8042.h>
#include <wdf.h>
#include <hidclass.h>



#if DBG

#define TRAP()                      DbgBreakPoint()

#define DebugPrint(_x_) DbgPrint _x_

#else   // DBG

#define TRAP()

#define DebugPrint(_x_)

#endif

 
typedef struct _DEVICE_EXTENSION
{
 
    WDFDEVICE  hDevice;
    //
    WDFIOTARGET   IoTarget;
    WDFREQUEST    ReuseRequest;   //重复使用的 Request
    WDFMEMORY     RequestBuffer;  // 请求的Buffer
    PDEVICE_OBJECT pDeviceObject;
    PDEVICE_OBJECT MousePDO;
    PDEVICE_OBJECT NextDeviceObject;

    ULONG         ActiveCount;
    BOOLEAN       bReadReady;

    KEVENT        TimeoutWait;
    UCHAR         LastButtonState;


     //
    // Previous hook routine and context
    //                               
    PVOID UpperContext;
     
    PI8042_MOUSE_ISR UpperIsrHook;

    //
    // Write to the mouse in the context of MouFilter_IsrHook
    //
    IN PI8042_ISR_WRITE_PORT IsrWritePort;

    //
    // Context for IsrWritePort, QueueMousePacket
    //
    IN PVOID CallContext;

    //
    // Queue the current packet (ie the one passed into MouFilter_IsrHook)
    // to be reported to the class driver
    //
    IN PI8042_QUEUE_PACKET QueueMousePacket;

    //
    // The real connect data that this driver reports to
    //
    CONNECT_DATA UpperConnectData;

  
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_EXTENSION,
                                        GetDeviceContext)
 
//
// Prototypes
//
DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_DEVICE_ADD MouFilter_EvtDeviceAdd;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL MouFilter_EvtIoInternalDeviceControl;
 


VOID
MouFilter_DispatchPassThrough(
     _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target
    );

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
);

VOID
MouFilter_ServiceCallback(
    IN PDEVICE_OBJECT DeviceObject,
    IN PMOUSE_INPUT_DATA InputDataStart,
    IN PMOUSE_INPUT_DATA InputDataEnd,
    IN OUT PULONG InputDataConsumed
    );

static VOID EvtCleanupCallback(WDFOBJECT Object);

static VOID EvtDriverContextCleanup(IN WDFOBJECT Object);

VOID
MouHid_DispatchInputData(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PMOUSE_INPUT_DATA InputData);

void CompletionRoutine(
    WDFREQUEST Request,
    WDFIOTARGET Target,
    PWDF_REQUEST_COMPLETION_PARAMS Params,
    WDFCONTEXT Context);

NTSTATUS Create_reuse_request(PDEVICE_EXTENSION   deviceContext);


#pragma pack(push)
#pragma pack(1)
typedef struct _PTP_CONTACT {
    UCHAR		Confidence_TipSwitch_ContactID_Padding;
    USHORT		X;
    USHORT		Y;
} PTP_CONTACT, * PPTP_CONTACT;
#pragma pack(pop)

typedef struct _PTP_REPORT {
    UCHAR       ReportID;
    PTP_CONTACT Contacts[5];
    USHORT      ScanTime;
    UCHAR       ContactCount;
    UCHAR       IsButtonClicked;
} PTP_REPORT, * PPTP_REPORT;


//鼠标状态报告,对应的HID是上边的报告
#pragma pack(1)
typedef struct mouse_report_t
{
    BYTE    report_id;
    BYTE    button; //0 no press, 1 left, 2 right ; 3 左右同时按下，触摸板一般不会有这种事
    CHAR    dx;
    CHAR    dy;
    CHAR    v_wheel; // 垂直
    CHAR    h_wheel; // 水平
}MOUSE_REPORT, * PMOUSE_REPORT;
#pragma pack()

#endif  // MOUFILTER_H

