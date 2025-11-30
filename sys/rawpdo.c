
#include "kbfiltr.h"
#include "public.h"

VOID
KbFilter_EvtIoDeviceControlForRawPdo(
    IN WDFQUEUE      Queue,
    IN WDFREQUEST    Request,
    IN size_t        OutputBufferLength,
    IN size_t        InputBufferLength,
    IN ULONG         IoControlCode
    )

{
    NTSTATUS status = STATUS_SUCCESS;
    WDFDEVICE parent = WdfIoQueueGetDevice(Queue);
    PRPDO_DEVICE_DATA pdoData;
    WDF_REQUEST_FORWARD_OPTIONS forwardOptions;
        
    pdoData = PdoGetData(parent);

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    DebugPrint(("Entered KbFilter_EvtIoDeviceControlForRawPdo\n"));


    switch (IoControlCode) {
    case IOCTL_KBFILTR_GET_KEYBOARD_ATTRIBUTES:
        WDF_REQUEST_FORWARD_OPTIONS_INIT(&forwardOptions);
        status = WdfRequestForwardToParentDeviceIoQueue(Request, pdoData->ParentQueue, &forwardOptions);
        if (!NT_SUCCESS(status)) {
            WdfRequestComplete(Request, status);
        }
        break;
    default:
        WdfRequestComplete(Request, status);
        break;
    }

    return;
}

#define MAX_ID_LEN 128

NTSTATUS
KbFiltr_CreateRawPdo(
    WDFDEVICE       Device,
    ULONG           InstanceNo
    )

{   
    NTSTATUS                    status;
    PWDFDEVICE_INIT             pDeviceInit = NULL;
    PRPDO_DEVICE_DATA           pdoData = NULL;
    WDFDEVICE                   hChild = NULL;
    WDF_OBJECT_ATTRIBUTES       pdoAttributes;
    WDF_DEVICE_PNP_CAPABILITIES pnpCaps;
    WDF_IO_QUEUE_CONFIG         ioQueueConfig;
    WDFQUEUE                    queue;
    WDF_DEVICE_STATE            deviceState;
    PDEVICE_EXTENSION           devExt;
    DECLARE_CONST_UNICODE_STRING(deviceId,KBFILTR_DEVICE_ID );
    DECLARE_CONST_UNICODE_STRING(hardwareId,KBFILTR_DEVICE_ID );
    DECLARE_CONST_UNICODE_STRING(deviceLocation,L"Keyboard Filter\0" );
    DECLARE_UNICODE_STRING_SIZE(buffer, MAX_ID_LEN);

    DebugPrint(("Entered KbFiltr_CreateRawPdo\n"));


    pDeviceInit = WdfPdoInitAllocate(Device);

    if (pDeviceInit == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto Cleanup;
    }

    status = WdfPdoInitAssignRawDevice(pDeviceInit, &GUID_DEVCLASS_KEYBOARD);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    status = WdfDeviceInitAssignSDDLString(pDeviceInit,
                                           &SDDL_DEVOBJ_SYS_ALL_ADM_ALL);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    status = WdfPdoInitAssignDeviceID(pDeviceInit, &deviceId);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    if (!RtlIsNtDdiVersionAvailable(NTDDI_WINXP)) {

        status = WdfPdoInitAddHardwareID(pDeviceInit, &hardwareId);
        if (!NT_SUCCESS(status)) {
            goto Cleanup;
        }
    }

    status =  RtlUnicodeStringPrintf(&buffer, L"%02d", InstanceNo);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    status = WdfPdoInitAssignInstanceID(pDeviceInit, &buffer);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    status = RtlUnicodeStringPrintf(&buffer,L"Keyboard_Filter_%02d", InstanceNo );
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    status = WdfPdoInitAddDeviceText(pDeviceInit,
                                        &buffer,
                                        &deviceLocation,
                                        0x409
                                        );
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    WdfPdoInitSetDefaultLocale(pDeviceInit, 0x409);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&pdoAttributes, RPDO_DEVICE_DATA);


    WdfPdoInitAllowForwardingRequestToParent(pDeviceInit);

    status = WdfDeviceCreate(&pDeviceInit, &pdoAttributes, &hChild);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    pdoData = PdoGetData(hChild);

    pdoData->InstanceNo = InstanceNo;

    devExt = FilterGetData(Device);
    pdoData->ParentQueue = devExt->rawPdoQueue;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig,
                                    WdfIoQueueDispatchSequential);

    ioQueueConfig.EvtIoDeviceControl = KbFilter_EvtIoDeviceControlForRawPdo;

    status = WdfIoQueueCreate(hChild,
                                        &ioQueueConfig,
                                        WDF_NO_OBJECT_ATTRIBUTES,
                                        &queue // pointer to default queue
                                        );
    if (!NT_SUCCESS(status)) {
        DebugPrint( ("WdfIoQueueCreate failed 0x%x\n", status));
        goto Cleanup;
    }
    WDF_DEVICE_PNP_CAPABILITIES_INIT(&pnpCaps);

    pnpCaps.Removable         = WdfTrue;
    pnpCaps.SurpriseRemovalOK = WdfTrue;
    pnpCaps.NoDisplayInUI     = WdfTrue;

    pnpCaps.Address  = InstanceNo;
    pnpCaps.UINumber = InstanceNo;

    WdfDeviceSetPnpCapabilities(hChild, &pnpCaps);

    WDF_DEVICE_STATE_INIT(&deviceState);
    deviceState.DontDisplayInUI = WdfTrue;
    WdfDeviceSetDeviceState(hChild, &deviceState);

    status = WdfDeviceCreateDeviceInterface(
                 hChild,
                 &GUID_DEVINTERFACE_KBFILTER,
                 NULL
             );

    if (!NT_SUCCESS (status)) {
        DebugPrint( ("WdfDeviceCreateDeviceInterface failed 0x%x\n", status));
        goto Cleanup;
    }

 
    status = WdfFdoAddStaticChild(Device, hChild);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }
    return STATUS_SUCCESS;

Cleanup:

    DebugPrint(("KbFiltr_CreatePdo failed %x\n", status));
    if (pDeviceInit != NULL) {
        WdfDeviceInitFree(pDeviceInit);
    }

    if(hChild) {
        WdfObjectDelete(hChild);
    }

    return status;
}

