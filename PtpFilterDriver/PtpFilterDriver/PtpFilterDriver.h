#pragma once

#define _HIDPI_NO_FUNCTION_MACROS_
#include <ntddk.h>
#include <hidclass.h>
#include <hidpddi.h>
#include <hidpi.h>
//#define NDEBUG
//#include <debug.h>
#include <ntddmou.h>
#include <kbdmou.h>

#include <wdf.h>

#include <hidport.h>
#include <ntstrsafe.h>
#include "hidsdi.h"   // Must link in hid.lib

#include <emmintrin.h>//__m128i定义的

#include <initguid.h>  
DEFINE_GUID(GUID_DEVINTERFACE_PtpFilterDriver,
	0xf271e39d, 0xd211, 0x4a6e, 0xa4, 0x39, 0x63, 0xe5, 0x19, 0x40, 0xd2, 0x10);// 0x745a17a0, 0x74d3, 0x11d0, 0xb6, 0xfe, 0x00, 0xa0, 0xc9, 0x0f, 0x57, 0xda
// {f271e39d-d211-4a6e-a439-63e51940d210}


#define LOWORD (1) ((WORD)(1))
#define HIWORD (1) ((WORD)(((DWORD) (1) >>16) & 0xFFFF))

#define DPRINT DbgPrint
#define DPRINT1 DbgPrint

#define FUNCTION_FROM_CTL_CODE(ctrlCode) (((ULONG)((ctrlCode) & 0x3FFC)) >> 2)

////
#define VHID_HARDWARE_IDS    L"HID\\PtpFilterDriver\0\0"
#define VHID_HARDWARE_IDS_LENGTH sizeof (VHID_HARDWARE_IDS)

#define MOUHID_TAG 0x50747046u

struct _DEVICE_OBJECT DeviceObject_stru_14000A340;

typedef struct _DEVICE_CONTEXT
{
	WDFDEVICE	hDevice;
	WDFIOTARGET IoTarget;

	ULONG       RequestLength;
	BOOLEAN     RequestDataAvailableFlag;

	WDFQUEUE    ReportQueue_32;
	WDFQUEUE    ReportQueue_56;

	WDFSPINLOCK ReadLoopSpinLock;
	WDFSPINLOCK ProcessedBufferSpinLock;

	PLIST_ENTRY LIST_ENTRY_928;
	PLIST_ENTRY LIST_ENTRY_944;

	WDFREQUEST  ReuseRequest;   //重复使用的
	WDFMEMORY   OutputBuffer;
	WDFMEMORY   RequestBuffer;  // 请求的Buffer
	PVOID       ReportBuffer;  // WDFMEMORY

	SHORT       REPORT_BUFFER_SIZE;
	ULONG       ReportLength;


	PHIDP_PREPARSED_DATA PreparsedData;
	PVOID UsageListBuffer;

	HIDP_CAPS Capabilities;
	ULONG     ValueCapsLength;

	ULONG TouchPad_Physical_Length_X;
	ULONG TouchPad_Physical_Length_Y;

	float TouchPad_Physical_DotDistance_X;//坐标x轴物理点距
	float TouchPad_Physical_DotDistance_Y;//坐标y轴物理点距

	UCHAR MaxFingerCount;
	UCHAR LastFingerCount;
	UCHAR CurrentFingerCount;
	

} DEVICE_CONTEXT, * PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext)

void EvtDriverContextCleanup(IN WDFOBJECT DriverObject);
NTSTATUS PTPFilterEvtDeviceAdd(IN WDFDRIVER Driver, IN PWDFDEVICE_INIT DeviceInit);
NTSTATUS PTPFilterCreateDevice(_Inout_ PWDFDEVICE_INIT DeviceInit);

NTSTATUS PTPFilterDeviceReleaseHardware(WDFDEVICE Device,
	WDFCMRESLIST ResourcesTranslated);

NTSTATUS
PTPFilterDevicePrepareHardware(
	_In_ WDFDEVICE Device,
	_In_ WDFCMRESLIST ResourceList,
	_In_ WDFCMRESLIST ResourceListTranslated
);

void PTPFilterEvtIoDeviceControl(
	IN WDFQUEUE     Queue,
	IN WDFREQUEST   Request,
	IN size_t       OutputBufferLength,
	IN size_t       InputBufferLength,
	IN ULONG        IoControlCode);

NTSTATUS Hid_StartDevice(PDEVICE_CONTEXT pDeviceContext);

NTSTATUS PTPFilterSendReadRequest(PDEVICE_CONTEXT pDeviceContext, PWDF_REQUEST_COMPLETION_PARAMS Params);


VOID RegDebug(WCHAR* strValueName, PVOID dataValue, ULONG datasizeValue);

///// 30 length
struct STRUCT_TOUCHPAD_PACKET
{
	UINT8 PacketType;  // unknown type  =2
	UINT8 ClickOccurred;  // 按住了触摸板， 不管几个按住，都是 1
	UINT8 IsFinger;  // 触摸板有手指 1，当离开瞬间，出现 0
	UINT8 FingerDataLength;  // 手指数据总长度， 手指个数*30
	struct STRUCT_TOUCHPAD_FINGER * FingerData;  //指向手指的结构体数据
	UINT8 NumOfFingers;  //手指个数
	UINT8 ClickOccurred2;  // 同上边的ClickOccurred
	UINT8 State; 
	UINT8 Reserved[10];
};


//
struct STRUCT_TOUCHPAD_FINGER
{
	SHORT OriginalX;  //触摸时的初始坐标，按下后，这个数字不变，但是经过测试证实触摸点初始坐标OriginalX、OriginalY有时准确有时不准或者新增触摸点后会发生变化没有参考意义所以不采用该参数来追踪触摸点
	SHORT OriginalY;
	SHORT X;          //当前的手指坐标
	SHORT Y;
	SHORT HorizontalAccel;
	SHORT VerticalAccel;
	SHORT ToolMajor;  //手指接触椭圆面长度
	SHORT ToolMinor;  //手指接触椭圆面宽度
	SHORT Orientation;  //手指接触椭圆面角度方向
	SHORT TouchMajor;  //
	SHORT TouchMinor;  //
	SHORT Rsvd1;
	SHORT Rsvd2;
	SHORT Pressure;  //触摸压力
	SHORT Rsvd3;
};



SHORT Unit_TABLE[6]= { 0, 1, 2, 3, 4, 0 }; 

LONG UnitExponent_Table[24] =
{
  5,
  5,
  6,
  6,
  7,
  7,
  8,
  -8,
  9,
  -7,
  10,
  -6,
  11,
  -5,
  12,
  -4,
  13,
  -3,
  14,
  -2,
  15,
  -1,
  0,
  0
}; 



///////
#define REPORTID_MOUSE  0x02
const unsigned char MouseReportDescriptor[] = {
	///
	0x05, 0x01, // USAGE_PAGE(Generic Desktop)
	0x09, 0x02, //   USAGE(Mouse)
	0xA1, 0x01, //   COLLECTION(APPlication)
	0x09, 0x01, //   USAGE(Pointer)
		0xA1, 0x00, //     COLLECTION(Physical)
		0x85, REPORTID_MOUSE, //     ReportID(Mouse ReportID)
		0x05, 0x09, //     USAGE_PAGE(Button)
		0x19, 0x01, //     USAGE_MINIMUM(button 1)   Button 按键， 位 0 左键， 位1 右键， 位2 中键
		0x29, 0x03, //     USAGE_MAXMUM(button 3)  //0x03限制最大的鼠标按键数量
		0x15, 0x00, //     LOGICAL_MINIMUM(0)
		0x25, 0x01, //     LOGICAL_MAXIMUM(1)
		0x95, 0x03, //     REPORT_COUNT(3)  //0x03鼠标按键数量
		0x75, 0x01, //     REPORT_SIZE(1)
		0x81, 0x02, //     INPUT(Data,Var,Abs)
		0x95, 0x01, //     REPORT_COUNT(1)
		0x75, 0x05, //     REPORT_SIZE(5)  //需要补足多少个bit使得加上鼠标按键数量的3个bit位成1个字节8bit
		0x81, 0x03, //     INPUT(Data,Var, Abs)
		0x05, 0x01, //     USAGE_PAGE(Generic Desktop)
		0x09, 0x30, //     USAGE(X)       X移动
		0x09, 0x31, //     USAGE(Y)       Y移动
		0x09, 0x38, //     USAGE(Wheel)   垂直滚动
		0x15, 0x81, //     LOGICAL_MINIMUM(-127)
		0x25, 0x7F, //     LOGICAL_MAXIMUM(127)
		0x75, 0x08, //     REPORT_SIZE(8)
		0x95, 0x03, //     REPORT_COUNT(3)
		0x81, 0x06, //     INPUT(Data,Var, Rel) //X,Y,垂直滚轮三个参数， 相对值

		//下边水平滚动
		0x05, 0x0C, //     USAGE_PAGE (Consumer Devices)
		0x0A, 0x38, 0x02, // USAGE(AC Pan)
		0x15, 0x81, //       LOGICAL_MINIMUM(-127)
		0x25, 0x7F, //       LOGICAL_MAXIMUM(127)
		0x75, 0x08, //       REPORT_SIZE(8)
		0x95, 0x01, //       REPORT_COUNT(1)
		0x81, 0x06, //       INPUT(data,Var, Rel) //水平滚轮，相对值
		0xC0,       //       End Connection(PhySical)
	0xC0,       //     End Connection

};


CONST HID_DESCRIPTOR DefaultHidDescriptor = {
	0x09,   // length of HID descriptor
	0x21,   // descriptor type == HID  0x21
	0x0100, // hid spec release
	0x00,   // country code == Not Specified
	0x01,   // number of HID class descriptors
	{ 0x22,   // descriptor type 
	sizeof(MouseReportDescriptor) }  // MouseReportDescriptor  TouchpadReportDescriptor
};


///鼠标状态报告,对应的HID是上边的报告
#pragma pack(1)
struct mouse_report_t
{
	BYTE    report_id;
	BYTE    button; //0 no press, 1 left, 2 right ; 3 左右同时按下，触摸板一般不会有这种事
	CHAR    dx;
	CHAR    dy;
	CHAR    v_wheel; // 垂直
	CHAR    h_wheel; // 水平
};
#pragma pack()




