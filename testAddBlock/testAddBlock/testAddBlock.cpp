#include <windows.h>
#include <stdio.h>
#include <setupapi.h>
#include <devguid.h>
#include <initguid.h>
#include <iostream>
#include <vector>
#include <string>

// Подключаем библиотеку SetupAPI для поиска устройств
#pragma comment(lib, "setupapi.lib")

// --- Здесь должен быть ваш public.h, с определениями IOCTL и структур ---
#include "public.h" 

// {3FB7299D-6847-4490-B0C9-99E0986AB886} - GUID_DEVINTERFACE_KBFILTER
DEFINE_GUID(GUID_DEVINTERFACE_KBFILTER,
    0x3fb7299d, 0x6847, 0x4490, 0xb0, 0xc9, 0x99, 0xe0, 0x98, 0x6a, 0xb8, 0x86);


// Новая функция для поиска и сохранения всех путей устройств по GUID (без изменений)
BOOL FindAllDevicePaths(LPGUID InterfaceGuid, std::vector<std::wstring>& DevicePaths)
{
    HDEVINFO hDevInfo = SetupDiGetClassDevs(
        InterfaceGuid,
        NULL,
        NULL,
        DIGCF_DEVICEINTERFACE | DIGCF_PRESENT
    );

    if (hDevInfo == INVALID_HANDLE_VALUE) {
        printf("SetupDiGetClassDevs failed. Error: %d\n", GetLastError());
        return FALSE;
    }

    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    DWORD deviceIndex = 0;
    BOOL foundAny = FALSE;

    while (SetupDiEnumDeviceInterfaces(hDevInfo, NULL, InterfaceGuid, deviceIndex, &deviceInterfaceData))
    {
        foundAny = TRUE;
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetail(hDevInfo, &deviceInterfaceData, NULL, 0, &requiredSize, NULL);

        if (requiredSize > 0)
        {
            std::vector<BYTE> buffer(requiredSize);
            PSP_DEVICE_INTERFACE_DETAIL_DATA detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)buffer.data();
            detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

            if (SetupDiGetDeviceInterfaceDetail(hDevInfo, &deviceInterfaceData, detailData, requiredSize, NULL, NULL)) {
                DevicePaths.push_back(detailData->DevicePath);
            }
        }
        deviceIndex++;
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return foundAny;
}


int main() {
    std::vector<std::wstring> devicePaths;

    printf("Searching for ALL Keyboard Filter Devices (Raw PDO GUID)...\n");

    if (!FindAllDevicePaths((LPGUID)&GUID_DEVINTERFACE_KBFILTER, devicePaths)) {
        printf("Error: Could not find any device. Is the driver installed?\n");
        return 1;
    }

    if (devicePaths.empty()) {
        printf("Error: Device enumeration found 0 devices.\n");
        return 1;
    }

    // -------------------------------------------------------------------
    // ВЫВОД ВСЕХ НАЙДЕННЫХ УСТРОЙСТВ
    // -------------------------------------------------------------------
    wprintf(L"\n--- Found %zu device(s) with GUID {3FB7...} ---\n", devicePaths.size());
    for (size_t i = 0; i < devicePaths.size(); ++i) {
        wprintf(L"[%zu] Path: %s\n", i, devicePaths[i].c_str());
    }
    wprintf(L"----------------------------------------------------\n\n");

    // -------------------------------------------------------------------
    // ОТПРАВКА IOCTL: Берем первое найденное устройство для теста
    // -------------------------------------------------------------------
    std::wstring devicePath = devicePaths[0];

    wprintf(L"Attempting to open device #0: %s\n", devicePath.c_str());

    // Открываем файл по найденному пути
    HANDLE hDevice = CreateFile(
        devicePath.c_str(),
        GENERIC_WRITE | GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        printf("Failed to open device #0. Error: %d\n", GetLastError());
        return 1;
    }

    // ===================================================================
    // 1. ЛОГИКА БЛОКИРОВКИ КЛАВИШ (БЕЗ ИЗМЕНЕНИЙ)
    // ===================================================================
    BLOCKED_KEYS_CONFIG blockConfig;
    RtlZeroMemory(&blockConfig, sizeof(blockConfig));

    blockConfig.Count = 3;
    blockConfig.Keys[0] = 0x0F; // TAB
    blockConfig.Keys[1] = 0x1C; // ENTER
    blockConfig.Keys[2] = 0x39; // SPACE 

    printf("Sending IOCTL to block %lu keys...\n", blockConfig.Count);

    DWORD bytesReturned;
    BOOL blockResult = DeviceIoControl(
        hDevice,
        IOCTL_KBFILTR_SET_BLOCKED_KEYS,
        &blockConfig, sizeof(blockConfig),
        NULL, 0,
        &bytesReturned,
        NULL
    );

    if (blockResult) {
        printf("Success! Keys blocked on device #0.\n");
    }
    else {
        printf("Failed to send blocking IOCTL. Error: %d\n", GetLastError());
    }

    // ===================================================================
    // 2. ДОБАВЛЕННАЯ ЛОГИКА ПОДМЕНЫ КЛАВИШ
    // ===================================================================
    KEY_REMAP_CONFIG remapConfig;
    RtlZeroMemory(&remapConfig, sizeof(remapConfig));

    // Пример: меняем 'Q' (0x10) и 'W' (0x11) местами
    remapConfig.Count = 2;

    // 1. Q (0x10) -> W (0x11)
    remapConfig.Remaps[0].OriginalMakeCode = 0x10;
    remapConfig.Remaps[0].NewMakeCode = 0x11;

    // 2. W (0x11) -> Q (0x10)
    remapConfig.Remaps[1].OriginalMakeCode = 0x11;
    remapConfig.Remaps[1].NewMakeCode = 0x10;

    printf("\nSending IOCTL to remap %lu keys (Q <-> W)...\n", remapConfig.Count);

    BOOL remapResult = DeviceIoControl(
        hDevice,
        IOCTL_KBFILTR_SET_REMAPPED_KEYS, // 🔑 Используем новый IOCTL
        &remapConfig, sizeof(remapConfig), // Отправляем структуру KEY_REMAP_CONFIG
        NULL, 0,
        &bytesReturned,
        NULL
    );

    if (remapResult) {
        printf("Success! Keys remapped on device #0.\n");
    }
    else {
        printf("Failed to send remapping IOCTL. Error: %d\n", GetLastError());
    }


    CloseHandle(hDevice); // Закрываем дескриптор после отправки обеих команд

    printf("\nConfiguration sent. Try pressing Q, W, TAB, and ENTER.\n");
    printf("Press Enter to exit...");
    getchar();

    return 0;
}