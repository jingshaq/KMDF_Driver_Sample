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

#include <emmintrin.h>//__m128i�����

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

	WDFREQUEST  ReuseRequest;   //�ظ�ʹ�õ�
	WDFMEMORY   OutputBuffer;
	WDFMEMORY   RequestBuffer;  // �����Buffer
	PVOID       ReportBuffer;  // WDFMEMORY

	SHORT       REPORT_BUFFER_SIZE;
	ULONG       ReportLength;


	PHIDP_PREPARSED_DATA PreparsedData;
	PVOID UsageListBuffer;

	HIDP_CAPS Capabilities;
	ULONG     ValueCapsLength;

	ULONG TouchPad_Physical_Length_X;
	ULONG TouchPad_Physical_Length_Y;

	float TouchPad_Physical_DotDistance_X;//����x��������
	float TouchPad_Physical_DotDistance_Y;//����y��������

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
	UINT8 ClickOccurred;  // ��ס�˴����壬 ���ܼ�����ס������ 1
	UINT8 IsFinger;  // ����������ָ 1�����뿪˲�䣬���� 0
	UINT8 FingerDataLength;  // ��ָ�����ܳ��ȣ� ��ָ����*30
	struct STRUCT_TOUCHPAD_FINGER * FingerData;  //ָ����ָ�Ľṹ������
	UINT8 NumOfFingers;  //��ָ����
	UINT8 ClickOccurred2;  // ͬ�ϱߵ�ClickOccurred
	UINT8 State; 
	UINT8 Reserved[10];
};


//
struct STRUCT_TOUCHPAD_FINGER
{
	SHORT OriginalX;  //����ʱ�ĳ�ʼ���꣬���º�������ֲ��䣬���Ǿ�������֤ʵ�������ʼ����OriginalX��OriginalY��ʱ׼ȷ��ʱ��׼���������������ᷢ���仯û�вο��������Բ����øò�����׷�ٴ�����
	SHORT OriginalY;
	SHORT X;          //��ǰ����ָ����
	SHORT Y;
	SHORT HorizontalAccel;
	SHORT VerticalAccel;
	SHORT ToolMajor;  //��ָ�Ӵ���Բ�泤��
	SHORT ToolMinor;  //��ָ�Ӵ���Բ����
	SHORT Orientation;  //��ָ�Ӵ���Բ��Ƕȷ���
	SHORT TouchMajor;  //
	SHORT TouchMinor;  //
	SHORT Rsvd1;
	SHORT Rsvd2;
	SHORT Pressure;  //����ѹ��
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
		0x19, 0x01, //     USAGE_MINIMUM(button 1)   Button ������ λ 0 ����� λ1 �Ҽ��� λ2 �м�
		0x29, 0x03, //     USAGE_MAXMUM(button 3)  //0x03����������갴������
		0x15, 0x00, //     LOGICAL_MINIMUM(0)
		0x25, 0x01, //     LOGICAL_MAXIMUM(1)
		0x95, 0x03, //     REPORT_COUNT(3)  //0x03��갴������
		0x75, 0x01, //     REPORT_SIZE(1)
		0x81, 0x02, //     INPUT(Data,Var,Abs)
		0x95, 0x01, //     REPORT_COUNT(1)
		0x75, 0x05, //     REPORT_SIZE(5)  //��Ҫ������ٸ�bitʹ�ü�����갴��������3��bitλ��1���ֽ�8bit
		0x81, 0x03, //     INPUT(Data,Var, Abs)
		0x05, 0x01, //     USAGE_PAGE(Generic Desktop)
		0x09, 0x30, //     USAGE(X)       X�ƶ�
		0x09, 0x31, //     USAGE(Y)       Y�ƶ�
		0x09, 0x38, //     USAGE(Wheel)   ��ֱ����
		0x15, 0x81, //     LOGICAL_MINIMUM(-127)
		0x25, 0x7F, //     LOGICAL_MAXIMUM(127)
		0x75, 0x08, //     REPORT_SIZE(8)
		0x95, 0x03, //     REPORT_COUNT(3)
		0x81, 0x06, //     INPUT(Data,Var, Rel) //X,Y,��ֱ�������������� ���ֵ

		//�±�ˮƽ����
		0x05, 0x0C, //     USAGE_PAGE (Consumer Devices)
		0x0A, 0x38, 0x02, // USAGE(AC Pan)
		0x15, 0x81, //       LOGICAL_MINIMUM(-127)
		0x25, 0x7F, //       LOGICAL_MAXIMUM(127)
		0x75, 0x08, //       REPORT_SIZE(8)
		0x95, 0x01, //       REPORT_COUNT(1)
		0x81, 0x06, //       INPUT(data,Var, Rel) //ˮƽ���֣����ֵ
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


///���״̬����,��Ӧ��HID���ϱߵı���
#pragma pack(1)
struct mouse_report_t
{
	BYTE    report_id;
	BYTE    button; //0 no press, 1 left, 2 right ; 3 ����ͬʱ���£�������һ�㲻����������
	CHAR    dx;
	CHAR    dy;
	CHAR    v_wheel; // ��ֱ
	CHAR    h_wheel; // ˮƽ
};
#pragma pack()




