#include <windows.h>
#include <stdio.h>
#include <setupapi.h>
#include <devguid.h>
#include <initguid.h>
#include <iostream>
#include <vector>

// Подключаем библиотеку SetupAPI для поиска устройств
#pragma comment(lib, "setupapi.lib")

#include "public.h" // Ваш заголовок с IOCTL и struct

// Убедитесь, что этот GUID совпадает с тем, что в драйвере (kbfiltr.h / inf файле)
// {A65C87F9-BE02-4ed9-92EC-012D416169FA} - GUID_BUS_KBFILTER (используется как ID шины в примере MS)
// {3FB7299D-6847-4490-B0C9-99E0986AB886} - GUID_DEVINTERFACE_KBFILTER
DEFINE_GUID(GUID_DEVINTERFACE_KBFILTER,
    0x3fb7299d, 0x6847, 0x4490, 0xb0, 0xc9, 0x99, 0xe0, 0x98, 0x6a, 0xb8, 0x86);

// Функция для получения пути к устройству (Symbolic Link) по GUID
BOOL GetDevicePath(LPGUID InterfaceGuid, std::wstring& DevicePath)
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

    // 2. Берем первое попавшееся устройство (индекс 0)
    // Если у вас несколько клавиатур и драйвер стоит на всех, можно сделать цикл
    if (!SetupDiEnumDeviceInterfaces(hDevInfo, NULL, InterfaceGuid, 0, &deviceInterfaceData)) {
        printf("SetupDiEnumDeviceInterfaces failed. Device not found.\n");
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return FALSE;
    }

    // 3. Узнаем, сколько памяти нужно для пути
    DWORD requiredSize = 0;
    SetupDiGetDeviceInterfaceDetail(hDevInfo, &deviceInterfaceData, NULL, 0, &requiredSize, NULL);

    if (requiredSize == 0) {
        printf("SetupDiGetDeviceInterfaceDetail failed to get size.\n");
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return FALSE;
    }

    // 4. Выделяем память
    std::vector<BYTE> buffer(requiredSize);
    PSP_DEVICE_INTERFACE_DETAIL_DATA detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)buffer.data();
    detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

    // 5. Получаем сам путь
    if (!SetupDiGetDeviceInterfaceDetail(hDevInfo, &deviceInterfaceData, detailData, requiredSize, NULL, NULL)) {
        printf("SetupDiGetDeviceInterfaceDetail failed to get path.\n");
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return FALSE;
    }

    // Сохраняем путь в строку
    DevicePath = detailData->DevicePath;

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return TRUE;
}

int main() {
    std::wstring devicePath;

    printf("Searching for Keyboard Filter Device...\n");

    // Ищем путь автоматически
    if (!GetDevicePath((LPGUID)&GUID_DEVINTERFACE_KBFILTER, devicePath)) {
        printf("Error: Could not find the device. Is the driver installed?\n");
        return 1;
    }

    wprintf(L"Found device at: %s\n", devicePath.c_str());

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
        printf("Failed to open device. Error: %d\n", GetLastError());
        return 1;
    }

    // Подготовка данных для отправки
    BLOCKED_KEYS_CONFIG config;
    RtlZeroMemory(&config, sizeof(config)); // Хорошая практика обнулять память

    config.Count = 3;
    config.Keys[0] = 0x0F; // TAB
    config.Keys[1] = 0x1C; // ENTER
    config.Keys[2] = 0x39; // SPACE (Пробел - для теста)

    printf("Sending IOCTL to block %d keys...\n", config.Count);

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
        printf("Success! Keys blocked.\n");
    }
    else {
        printf("Failed to send IOCTL. Error: %d\n", GetLastError());
    }

    CloseHandle(hDevice);

    printf("Press Enter to exit...");
    getchar();

    return 0;
}