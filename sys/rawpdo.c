
#include "kbfiltr.h"
#include "public.h"
#include <wdf.h>
#include <wdfobject.h>


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
    // WDFDEVICE parent - это Raw PDO, которое получило запрос.
    WDFDEVICE hChild = WdfIoQueueGetDevice(Queue);
    PRPDO_DEVICE_DATA pdoData = PdoGetData(hChild);
    // Получаем FDO (родительский фильтр), который хранит DEVICE_EXTENSION.
    WDFDEVICE hFDO = WdfIoQueueGetDevice(pdoData->ParentQueue);

    // Получаем контексты устройств

    PDEVICE_EXTENSION devExt = FilterGetData(hFDO); // <-- КЛЮЧЕВОЕ ИСПРАВЛЕНИЕ: Инициализация devExt

    WDF_REQUEST_FORWARD_OPTIONS forwardOptions;
    size_t bytesTransferred = 0;

    // Дублирование объявления 'status' удалено.
    // Исходный 'parent' переименован в 'hChild' для ясности.

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    DebugPrint(("Entered KbFilter_EvtIoDeviceControlForRawPdo\n",0));


    switch (IoControlCode) {
    case IOCTL_KBFILTR_SET_REMAPPED_KEYS:
    {
        NTSTATUS remapStatus = STATUS_SUCCESS;
        PKEY_REMAP_CONFIG newRemapConfig = NULL;

        // 1. ПРОВЕРКА РАЗМЕРА
        if (InputBufferLength < sizeof(KEY_REMAP_CONFIG)) {
            remapStatus = STATUS_BUFFER_TOO_SMALL;
            DebugPrint(("KBFILTR IOCTL: ERROR: Remap buffer too small! Status 0x%x\n", remapStatus));
            break;
        }

        // 2. ПОЛУЧЕНИЕ БУФЕРА
        remapStatus = WdfRequestRetrieveInputBuffer(Request, sizeof(KEY_REMAP_CONFIG), (PVOID*)&newRemapConfig, NULL);
        if (!NT_SUCCESS(remapStatus)) {
            DebugPrint(("KBFILTR IOCTL: ERROR: RetrieveInputBuffer for remap failed! Status 0x%x\n", remapStatus));
            break;
        }

        // 3. ВАЛИДАЦИЯ COUNT
        if (newRemapConfig->Count > MAX_REMAPPED_KEYS) {
            remapStatus = STATUS_INVALID_PARAMETER;
            DebugPrint(("KBFILTR IOCTL: ERROR: Invalid parameter (Remap Count too high)! Status 0x%x\n", remapStatus));
            break;
        }

        // 4. КОПИРОВАНИЕ ДАННЫХ В КОНТЕКСТ ДРАЙВЕРА С БЛОКИРОВКОЙ
        WdfSpinLockAcquire(devExt->ConfigLock);
        RtlCopyMemory(&devExt->RemapConfig, newRemapConfig, sizeof(KEY_REMAP_CONFIG));

        devExt->RemappingEnabled = (newRemapConfig->Count > 0);

        WdfSpinLockRelease(devExt->ConfigLock);

        DebugPrint(("KBFILTR IOCTL: Successfully updated remap keys. Final Count: %lu\n", devExt->RemapConfig.Count));

        status = remapStatus; // Используем общую переменную status
        bytesTransferred = 0;
    }
    break;
    case IOCTL_KBFILTR_SET_BLOCKED_KEYS:
    {
        DebugPrint(("KBFILTR IOCTL: ** DEBUG POINT 2: Inside Case **\n", 0));
        PBLOCKED_KEYS_CONFIG newConfig = NULL;


        // 1. ПРОВЕРКА РАЗМЕРА
        DebugPrint(("KBFILTR IOCTL: InputBufferLength: %zu, Expected size: %zu\n", InputBufferLength, sizeof(BLOCKED_KEYS_CONFIG)));
        if (InputBufferLength < sizeof(BLOCKED_KEYS_CONFIG)) {
            status = STATUS_BUFFER_TOO_SMALL;
            DebugPrint(("KBFILTR IOCTL: ERROR: Buffer too small! Status 0x%x\n", status));
            break;
        }

        // 2. ПОЛУЧЕНИЕ БУФЕРА
        status = WdfRequestRetrieveInputBuffer(Request, sizeof(BLOCKED_KEYS_CONFIG), (PVOID*)&newConfig, NULL);
        if (!NT_SUCCESS(status)) {
            DebugPrint(("KBFILTR IOCTL: ERROR: RetrieveInputBuffer failed! Status 0x%x\n", status));
            break;
        }

        // 3. ДИАГНОСТИКА И ВАЛИДАЦИЯ COUNT
        DebugPrint(("KBFILTR IOCTL: ** DEBUG POINT 3: Got Buffer. Count: %lu **\n", newConfig->Count));
        DebugPrint(("KBFILTR IOCTL: [DEBUG-ALIGN] Received raw Count value: %lu\n", newConfig->Count));

        DebugPrint(("KBFILTR IOCTL: Received Count: %lu, Max Allowed: %d\n", newConfig->Count, MAX_BLOCKED_KEYS));
        if (newConfig->Count > MAX_BLOCKED_KEYS) {
            status = STATUS_INVALID_PARAMETER;
            DebugPrint(("KBFILTR IOCTL: ERROR: Invalid parameter (Count too high)! Status 0x%x\n", status));
            break;
        }

        // 4. КОПИРОВАНИЕ ДАННЫХ В КОНТЕКСТ ДРАЙВЕРА С БЛОКИРОВКОЙ
        WdfSpinLockAcquire(devExt->ConfigLock);
        RtlCopyMemory(&devExt->BlockedKeys, newConfig, sizeof(BLOCKED_KEYS_CONFIG));
        if (newConfig->OperationFlag == 0) {
            // Флаг = 0: ОТКЛЮЧИТЬ блокировку
            devExt->BlockingEnabled = FALSE;
            DebugPrint(("KBFILTR IOCTL: Blocking DISABLED by client command.\n",0));

        }
        else if (newConfig->OperationFlag == 1) {
            // Флаг = 1: ВКЛЮЧИТЬ блокировку И сохранить новые ключи

            // Если клиент передал список ключей, сохраняем его
            if (newConfig->Count > 0) {
                RtlCopyMemory(&devExt->BlockedKeys, newConfig, sizeof(BLOCKED_KEYS_CONFIG));
                DebugPrint(("KBFILTR IOCTL: Successfully updated keys and Enabled blocking.\n", 0));
            }
            else {
                DebugPrint(("KBFILTR IOCTL: Enabled blocking. Key list unchanged.\n", 0));
            }
            devExt->BlockingEnabled = TRUE;

        }
        else {
            // Неизвестный OperationFlag
            status = STATUS_INVALID_PARAMETER;
            DebugPrint(("KBFILTR IOCTL: ERROR: Invalid OperationFlag (%lu).\n", newConfig->OperationFlag));
        }

        // --- КОНЕЦ АЛГОРИТМА ---
        WdfSpinLockRelease(devExt->ConfigLock);

        // 5. ПОДТВЕРЖДЕНИЕ ВЫВОДОМ
        DebugPrint(("KBFILTR IOCTL: Successfully updated keys. Final Count: %lu\n", devExt->BlockedKeys.Count));
        for (ULONG i = 0; i < devExt->BlockedKeys.Count; i++) {
            DebugPrint(("KBFILTR IOCTL: Key[%lu] set to 0x%x\n", i, devExt->BlockedKeys.Keys[i]));
        }

        bytesTransferred = 0;
    }
    break;

    case IOCTL_KBFILTR_GET_KEYBOARD_ATTRIBUTES:
        // Перенаправляем запрос Query Attributes родительскому фильтру, который знает атрибуты.
        WDF_REQUEST_FORWARD_OPTIONS_INIT(&forwardOptions);
        status = WdfRequestForwardToParentDeviceIoQueue(Request, pdoData->ParentQueue, &forwardOptions);
        if (!NT_SUCCESS(status)) {
            WdfRequestComplete(Request, status);
        }
        // Возвращаемся, так как запрос был перенаправлен, и будет завершен в другом месте.
        return;

    default:
        // Неизвестный или нереализованный IOCTL
        status = STATUS_NOT_IMPLEMENTED;
        break;
    }

    DebugPrint(("KBFILTR IOCTL: ** DEBUG POINT 4: Completing request with status 0x%x **\n", status));

    // Завершаем запрос, если он не был перенаправлен (в случае IOCTL_KBFILTR_SET_BLOCKED_KEYS)
    WdfRequestCompleteWithInformation(Request, status, bytesTransferred);

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

