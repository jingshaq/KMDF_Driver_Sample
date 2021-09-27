# MouHidInputHook

MouHidInputHook enables users to filter, modify, and inject mouse input data packets into the input data stream of HID USB mouse devices without modifying the mouse device stacks.

The [MouHid Hook Manager](./MouHidInputHook/mouhid_hook_manager.cpp) emulates the hook strategy used by the [Moufiltr](https://github.com/microsoft/Windows-driver-samples/tree/master/input/moufiltr) driver by hooking the [CONNECT_DATA](https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/content/kbdmou/ns-kbdmou-_connect_data "CONNECT_DATA structure") object inside each MouHid device object. The MouHid driver uses the **ClassService** field of a CONNECT_DATA object to transfer mouse input data packets to mouse class data queues. By emulating this strategy, we have access to each data packet generated by the mouse device without needing to install a mouse filter driver. This emulation technique has the following benefits:

1. We can safely unhook mouse device stacks and unload the MouHidInputHook driver without needing to unload the hooked device stacks. A standard filter driver for a PnP device can only be unloaded after all of the device objects attached to it are destroyed.

2. The technique is PatchGuard safe.

3. The technique is relatively stealthy from the perspective of a kernel anti-cheat because:

    1. The field offset of the CONNECT_DATA object inside a MouHid device object is not defined in a public header.

    2. We do not modify the HID USB mouse device stack(s) by attaching a filter device object.

    A kernel anti-cheat must resort to heuristics or other potentially unsafe vectors in order to detect the presence of this technique. This is generally impractical because an anti-cheat driver must be reliable, i.e., avoid using undocumented information, in order to support the player base of the protected video game.

    This project uses a heuristic to resolve the CONNECT_DATA field offset during driver initialization (described below).

The MouHid Hook Manager supports PnP events by registering a PnP notification callback for mouse device interface changes. This callback is invoked each time a MouClass device object is added to or removed from the system.

## MouHid Monitor

The [MouHid Monitor](./MouHidInputHook/mouhid_monitor.cpp) is an example hook callback which logs the content of each data packet generated by a HID USB mouse. Users can utilize this feature to debug how mouse actions, e.g., moving the mouse or clicking a mouse button, are represented as a sequence of one or more data packets.

For example, we can determine the sequence of data packets required to emulate a left mouse button click action by loading the driver, running the client, and manually clicking the left mouse button. The following items are log output excerpts after performing this action on a local machine and in a virtual machine:

    The column data elements refer to CONNECT_DATA and MOUSE_INPUT_DATA fields:

        SC: Class service callback invoked to transfer the packet.  CONNECT_DATA.ClassService
        DO: Device object which contains the target data queue.     CONNECT_DATA.ClassDeviceObject
        ID: Target device unit id, e.g., '\Device\PointerClassX'.   MOUSE_INPUT_DATA.UnitId
        IF: Indicator flags.                                        MOUSE_INPUT_DATA.Flags
        BF: Transition state of the mouse buttons.                  MOUSE_INPUT_DATA.ButtonFlags
        BD: Wheel data for 'SCROLL' action packets.                 MOUSE_INPUT_DATA.ButtonData
        RB: Raw button state.                                       MOUSE_INPUT_DATA.RawButtons
        EX: Device-specific data.                                   MOUSE_INPUT_DATA.ExtraInformation
        LX: Signed relative or absolute motion in X direction.      MOUSE_INPUT_DATA.LastX
        LY: Signed relative or absolute motion in Y direction.      MOUSE_INPUT_DATA.LastY

    ================================= OUTPUT ==================================

    Platform:       Windows 7 SP1 x64
    Environment:    Local machine
    Mouse:          Logitech G303
    Action:         Hold mouse in the air, click left mouse button, put mouse on surface, move the mouse

        Packet 63: SC=FFFFF880040A8858 DO=FFFFFA800FDE78B0 ID=1 IF=0x000 BF=0x000 BD=0 RB=0 EX=0 LX=0 LY=-1
    D ->Packet 64: SC=FFFFF880040A8858 DO=FFFFFA800FDE78B0 ID=1 IF=0x000 BF=0x001 BD=0 RB=0 EX=0 LX=0 LY=0
    U ->Packet 65: SC=FFFFF880040A8858 DO=FFFFFA800FDE78B0 ID=1 IF=0x000 BF=0x002 BD=0 RB=0 EX=0 LX=0 LY=0
        Packet 66: SC=FFFFF880040A8858 DO=FFFFFA800FDE78B0 ID=1 IF=0x000 BF=0x000 BD=0 RB=0 EX=0 LX=0 LY=1
        Packet 67: SC=FFFFF880040A8858 DO=FFFFFA800FDE78B0 ID=1 IF=0x000 BF=0x000 BD=0 RB=0 EX=0 LX=0 LY=1

    Platform:       Windows 7 SP1 x64
    Environment:    VMware virtual machine, vmusbmouse.sys mouse filter driver active
    Mouse:          Logitech G303
    Action:         Hold mouse in the air, click left mouse button, put mouse on surface, move the mouse

        Packet 75: SC=FFFFF880025630C0 DO=FFFFFA80040F6A40 ID=1 IF=0x003 BF=0x000 BD=0 RB=0 EX=0 LX=12580 LY=25436
    D ->Packet 76: SC=FFFFF88003FE5858 DO=FFFFFA8004106CF0 ID=2 IF=0x000 BF=0x001 BD=0 RB=0 EX=0 LX=0 LY=0
        Packet 77: SC=FFFFF880025630C0 DO=FFFFFA80040F6A40 ID=1 IF=0x003 BF=0x000 BD=0 RB=0 EX=0 LX=12580 LY=25436
    U ->Packet 78: SC=FFFFF88003FE5858 DO=FFFFFA8004106CF0 ID=2 IF=0x000 BF=0x002 BD=0 RB=0 EX=0 LX=0 LY=0
        Packet 79: SC=FFFFF880025630C0 DO=FFFFFA80040F6A40 ID=1 IF=0x003 BF=0x000 BD=0 RB=0 EX=0 LX=12580 LY=25436
        Packet 80: SC=FFFFF880025630C0 DO=FFFFFA80040F6A40 ID=1 IF=0x003 BF=0x000 BD=0 RB=0 EX=0 LX=12528 LY=25436
        Packet 81: SC=FFFFF880025630C0 DO=FFFFFA80040F6A40 ID=1 IF=0x003 BF=0x000 BD=0 RB=0 EX=0 LX=12474 LY=25544
        Packet 82: SC=FFFFF880025630C0 DO=FFFFFA80040F6A40 ID=1 IF=0x003 BF=0x000 BD=0 RB=0 EX=0 LX=12422 LY=25544

    Note: The above output was modified to increase readability.

The data packet for the left mouse button down action is indicated by **D ->**, and the data packet for the left mouse button up action is indicated by **U ->**.

We can infer the following rules for these environments:

1. The local machine environment uses **MOUSE_MOVE_RELATIVE** for mouse movement actions, and the virtual machine environment uses **MOUSE_MOVE_ABSOLUTE**.

2. The local machine environment contains one HID USB mouse device stack, and the virtual machine environment contains two HID USB mouse device stacks. The virtual machine environment uses different (ClassDeviceObject, ClassService) pairs for button action packets and movement action packets. Each pair is represented by a CONNECT_DATA object inside a MouHid device object. Therefore, there must be two MouHid device objects and two device stacks.

3. The virtual machine environment contains multiple unit ids. This implies that there are two named mouse class device objects: **\Device\PointerClass1** and **\Device\PointerClass2**. There must be two class data queues: one for button action packets and one for movement action packets. Therefore, a data packet should never contain both button data and movement data. Conversely, data packets in the local machine environment can contain both data types because there is one class data queue.

4. The virtual machine contains a third party filter driver, vmusbmouse, which generates a movement data packet between a button-down packet and its corresponding button-up packet. This may be related to the mouse smoothing feature of VMware Tools.

### MouClassInputInjection

The [MouClassInputInjection](https://github.com/changeofpace/MouClassInputInjection) project is an application of the knowledge acquired from using the **MouHid Monitor**. This project uses a MouHidInputHook hook callback to dynamically resolve the packet data rules for the HID USB mouse device stacks on the host machine. These rules are used to synthesize and inject valid data packets into the input data stream.

## Projects

### MouHidInputHook

The core driver project which implements the hook interface.

### MouHidMonitor

A command line **MouHidInputHook** client which enables the MouHid Monitor.

## Input Processing Internals

### Input Class Drivers

The input class drivers, kbdclass.sys and mouclass.sys, allow hardware-independent operation of input devices by enforcing a non-standard communication protocol between device objects in an input device stack. This protocol divides the device stack into two substacks: the hardware-independent upper stack and the hardware-dependent lower stack. The lower stack transfers input data from a physical device to the upper stack via the [class service callback](https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/content/kbdmou/nc-kbdmou-pservice_callback_routine "PSERVICE_CALLBACK_ROUTINE callback function"). The class service callback ensures that the upper stack always receives input data in a normalized format.

### Class Service Callback

The class service callback for an input device stack is established by the input class driver's **AddDevice** routine. This routine performs the following actions:

1. Creates an upper-level class filter device object.
2. Attaches the new device object to the input device stack.
3. Initializes a [CONNECT_DATA](https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/content/kbdmou/ns-kbdmou-_connect_data "CONNECT_DATA structure") object for this class device object.
4. Sends the connect data object inside a connect-device request down the device stack.

The device object whose driver completes this request is the top-level device object in the lower substack. This driver stores the connect data object inside the device extension of the corresponding device object.

### Class Data Queue

A class device object contains a circular buffer of input data packets in its device extension. This buffer, referred to as the class data queue, is effectively a producer/consumer queue.

The lower substack produces data packets by invoking the class service callback when new input data is available. The class service callback copies the input data from the input buffer maintained by the lower substack device to the class data queue.

The win32k subsystem consumes data packets by reading the input device via nt!ZwReadFile. ZwReadFile issues an irp to the top of the input device stack which is ultimately handled by the IRP_MJ_READ handler of the input class driver. This handler copies data packets from the class data queue to the irp's system buffer.

### Input Processing

The following diagram depicts the input processing system for a HID USB mouse device on Windows 7 SP1 x64.

<p align="center">
    <img src="Image/mouse_input_processing_internals.png" />
</p>

#### Upper Substack

1. The upper substack read cycle begins in win32k!StartDeviceRead. This routine issues a read request for a mouse device by invoking nt!ZwReadFile:

    ```C++
    ZwReadFile(
        MouseDeviceHandle,  // Handle to the mouse device to read packets from
                            //  (referred to as the 'target mouse device')
        NULL,
        win32k!InputApc,    // Apc routine executed when the read is completed
        MouseDeviceInfo,    // Pointer to the DEVICEINFO object for the target
                            //  mouse device (see win32k!gpDeviceInfoList)
        IoStatusBlock,
        Buffer,             // MOUSE_INPUT_DATA buffer inside MouseDeviceInfo
        Length,
        &win32k!gZero,
        0);
    ```

2. nt!ZwReadFile sends an IRP_MJ_READ irp to the top of the target mouse device stack.

3. The irp is ultimately processed by the MouClass IRP_MJ_READ handler, mouclass!MouseClassRead. This routine validates the irp then invokes mouclass!MouseClassHandleRead.

    If the class data queue of the target mouse device object contains new input data packets then mouclass!MouseClassHandleRead invokes mouclass!MouseClassReadCopyData. This routine copies the new data packets from the class data queue to the irp's system buffer.

    If the class data queue does not contain new input data packets then:

    1. The irp is appended to a linked list in the device extension of the target mouse device object (referred to as the pending irp list).

    2. STATUS_PENDING is returned to nt!NtReadFile.

4. The win32k!InputApc routine is invoked when the irp is completed. This routine invokes win32k!ProcessMouseInput via a function pointer in the DEVICE_TEMPLATE object for the mouse device type in the DEVICE_TEMPLATE array, win32k!aDeviceTemplate. win32k!ProcessMouseInput applies movement data from the data packets to the user desktop and queues each packet to win32k!gMouseEventQueue. The raw input thread processes this queue inside win32k!RawInputThread. Finally, win32k!InputApc restarts the read cycle by invoking win32k!StartDeviceRead.

#### Lower Substack

1. The lower substack read cycle begins in mouhid!MouHid_StartRead. This routine initializes (reuses) an IRP_MJ_READ irp, sets the completion routine to mouhid!MouHid_ReadComplete, and then sends it to the next lower device object in the device stack, a HidUsb device object, via nt!IofCallDriver. The irp is routed to the IRP_MJ_READ handler defined in the HidUsb driver object, HIDCLASS!HidpMajorHandler.

2. HIDCLASS!HidpMajorHandler invokes HIDCLASS!HidpIrpMajorRead. The irp routing and processing beyond this point is outside the scope of this analysis.

3. USBPORT!USBPORT_Core_iCompleteDoneTransfer invokes nt!IopfCompleteRequest to complete the irp after new input data is read from the physical device. The irp's completion routine, mouhid!MouHid_ReadComplete, is invoked. This routine converts the input data from its hardware-dependent format, HID report, to the hardware-independent format, [MOUSE_INPUT_DATA](https://docs.microsoft.com/en-us/windows/win32/api/ntddmou/ns-ntddmou-mouse_input_data "MOUSE_INPUT_DATA structure") packet. The converted packets are stored in the device extension of the MouHid device object associated with the completed irp.

4. mouhid!MouHid_ReadComplete invokes the [mouse class service callback](https://docs.microsoft.com/en-us/previous-versions/ff542394%28v%3dvs.85%29 "MouseClassServiceCallback"), mouclass!MouseClassServiceCallback, via the ClassService field of the CONNECT_DATA object in the device extension of the MouHid device object associated with the completed irp:

    ```C++
    PMOUHID_DEVICE_EXTENSION DeviceExtension = MouHidDeviceObject->DeviceExtension;
    PCONNECT_DATA ConnectData = &DeviceExtension->ConnectData;
    ULONG InputDataConsumed = 0;

    ((MOUSE_SERVICE_CALLBACK_ROUTINE)ConnectData->ClassService)(
        ConnectData->ClassDeviceObject,
        DeviceExtension->InputDataStart,
        DeviceExtension->InputDataEnd,
        &InputDataConsumed);
    ```

    mouclass!MouseClassServiceCallback uses data packets from the **InputDataStart** buffer to complete each irp in the pending irp list of the class device object. The remaining input data packets in the InputDataStart buffer are copied to the class data queue of the class device object. Finally, each serviced irp is completed via nt!IofCompleteRequest. This action is directly connected to item [4] in the **Upper Substack** description above.

5. mouhid!MouHid_ReadComplete restarts the read cycle by invoking mouhid!MouHid_StartRead. The **Hook Point** indicates where the MouHid Hook Manager installs the class service hook.

## Connect Data Heuristic

The [MouHid](./MouHidInputHook/mouhid.cpp) module uses a heuristic to dynamically resolve the **CONNECT_DATA** field inside the MouHid device extension. This heuristic is based on the MouClass initialiation protocol so it may be applicable to other mouse device types. i.e., This heuristic can potentially be used for any mouse device stack which uses the MouClass driver. The following is a summary of the heuristic:

1. Obtain a list of all the MouHid device objects.

2. For each MouHid device object:

    1. Get the device object attached to the MouHid device object.

    2. Get the address range of every executable image section in the driver of
        the attached device object.

    3. Search the device extension of the MouHid device object for a valid
        CONNECT_DATA object by interpreting each pointer-aligned address as a
        CONNECT_DATA candidate. A candidate is valid if it meets the following
        criteria:

        1. The **ClassDeviceObject** field matches the attached device object.

        2. The **ClassService** field points to an address contained in one of
            the executable image sections from (ii).

## Log Output

Use **DebugView** (or **DbgView**) from Sysinternals to read the driver's log output.

## Notes

* The MouHidInputHook project was developed for Windows 7 SP1 x64. Support for other platforms is unknown.
* The MouHidInputHook hook technique is PatchGuard safe.