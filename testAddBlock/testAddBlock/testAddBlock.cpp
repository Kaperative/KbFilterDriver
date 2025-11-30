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

// --- Здесь должен быть ваш public.h, с определениями IOCTL и BLOCKED_KEYS_CONFIG ---
// Для корректной работы обязательно унифицируйте упаковку структур
// #pragma pack(push, 1)
// typedef struct _BLOCKED_KEYS_CONFIG { ... }
// #pragma pack(pop)
#include "public.h" 

// {3FB7299D-6847-4490-B0C9-99E0986AB886} - GUID_DEVINTERFACE_KBFILTER
DEFINE_GUID(GUID_DEVINTERFACE_KBFILTER,
    0x3fb7299d, 0x6847, 0x4490, 0xb0, 0xc9, 0x99, 0xe0, 0x98, 0x6a, 0xb8, 0x86);


// Новая функция для поиска и сохранения всех путей устройств по GUID
BOOL FindAllDevicePaths(LPGUID InterfaceGuid, std::vector<std::wstring>& DevicePaths)
{
    // 1. Получаем список всех устройств с этим интерфейсом
    HDEVINFO hDevInfo = SetupDiGetClassDevs(
        InterfaceGuid,
        NULL,
        NULL,
        DIGCF_DEVICEINTERFACE | DIGCF_PRESENT // Только подключенные устройства
    );

    if (hDevInfo == INVALID_HANDLE_VALUE) {
        printf("SetupDiGetClassDevs failed. Error: %d\n", GetLastError());
        return FALSE;
    }

    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    DWORD deviceIndex = 0;
    BOOL foundAny = FALSE;

    // Цикл по всем устройствам
    while (SetupDiEnumDeviceInterfaces(hDevInfo, NULL, InterfaceGuid, deviceIndex, &deviceInterfaceData))
    {
        foundAny = TRUE;

        // 2. Узнаем, сколько памяти нужно для пути
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetail(hDevInfo, &deviceInterfaceData, NULL, 0, &requiredSize, NULL);

        if (requiredSize > 0)
        {
            // 3. Выделяем память
            std::vector<BYTE> buffer(requiredSize);
            PSP_DEVICE_INTERFACE_DETAIL_DATA detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)buffer.data();
            detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

            // 4. Получаем сам путь
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

    // Ищем все пути автоматически
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

    // Подготовка данных для отправки
    BLOCKED_KEYS_CONFIG config;
    RtlZeroMemory(&config, sizeof(config));

    config.Count = 3;
    config.Keys[0] = 0x0F; // TAB
    config.Keys[1] = 0x1C; // ENTER
    config.Keys[2] = 0x39; // SPACE 

    printf("Sending IOCTL to block %lu keys...\n", config.Count);

    DWORD bytesReturned;
    BOOL result = DeviceIoControl(
        hDevice,
        IOCTL_KBFILTR_SET_BLOCKED_KEYS,
        &config, sizeof(config),
        NULL, 0,
        &bytesReturned,
        NULL
    );

    if (result) {
        printf("Success! Keys blocked on device #0.\n");
    }
    else {
        printf("Failed to send IOCTL. Error: %d\n", GetLastError());
    }

    CloseHandle(hDevice);

    // -------------------------------------------------------------------
    // ДОПОЛНИТЕЛЬНЫЙ ШАГ: Если вы хотите отправить IOCTL ВСЕМ устройствам
    // (Полезно, если у вас несколько клавиатур и вы не уверены, какой индекс верен)
    // -------------------------------------------------------------------
    /*
    if (devicePaths.size() > 1) {
        printf("\nSending IOCTL to all remaining devices...\n");
        // Здесь можно добавить цикл по всем устройствам и отправить команду каждому.
    }
    */

    printf("Press Enter to exit...");
    getchar();

    return 0;
}