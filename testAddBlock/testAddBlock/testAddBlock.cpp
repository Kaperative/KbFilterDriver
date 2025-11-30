// set_block.cpp - пример: set_block <scancode> <0|1>
#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <iostream>

#pragma comment(lib, "setupapi.lib")

// GUID из драйвера (должен совпадать)
static const GUID GUID_DEVINTERFACE_KBFILTER =
{ 0x3fb7299d, 0x6847, 0x4490, {0xb0,0xc9,0x99,0xe0,0x98,0x6a,0xb8,0x86} };

#define IOCTL_KBFILTR_SET_BLOCKED_SCANCODE CTL_CODE( FILE_DEVICE_KEYBOARD, 0x800 + 1, METHOD_BUFFERED, FILE_WRITE_DATA )

typedef struct _KBFILTR_BLOCK_CMD {
    ULONG Scancode;
    ULONG Action;
} KBFILTR_BLOCK_CMD;

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: set_block <scancode> <0|1>\n";
        return 1;
    }
    ULONG sc = (ULONG)atoi(argv[1]);
    ULONG act = (ULONG)atoi(argv[2]);

    // 1) Get device path for our device interface
    HDEVINFO hDevInfo = SetupDiGetClassDevs(
        &GUID_DEVINTERFACE_KBFILTER,
        NULL,
        NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        std::cerr << "SetupDiGetClassDevs failed\n";
        return 1;
    }

    SP_DEVICE_INTERFACE_DATA interfaceData;
    interfaceData.cbSize = sizeof(interfaceData);
    if (!SetupDiEnumDeviceInterfaces(hDevInfo, NULL, &GUID_DEVINTERFACE_KBFILTER, 0, &interfaceData)) {
        std::cerr << "SetupDiEnumDeviceInterfaces failed\n";
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return 1;
    }

    // get required buffer size
    DWORD required = 0;
    SetupDiGetDeviceInterfaceDetail(hDevInfo, &interfaceData, NULL, 0, &required, NULL);
    PSP_DEVICE_INTERFACE_DETAIL_DATA pDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(required);
    if (!pDetail) {
        std::cerr << "alloc fail\n";
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return 1;
    }
    pDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
    if (!SetupDiGetDeviceInterfaceDetail(hDevInfo, &interfaceData, pDetail, required, NULL, NULL)) {
        std::cerr << "SetupDiGetDeviceInterfaceDetail failed\n";
        free(pDetail);
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return 1;
    }
  //  PSP_DEVICE_INTERFACE_DETAIL_DATA_W pDetail;
    // open device
    HANDLE hDev = CreateFileW(
        pDetail->DevicePath,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );
    if (hDev == INVALID_HANDLE_VALUE) {
        std::cerr << "CreateFile failed. Error: " << GetLastError() << "\n";
        free(pDetail);
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return 1;
    }

    KBFILTR_BLOCK_CMD cmd;
    cmd.Scancode = sc;
    cmd.Action = act ? 1 : 0;

    DWORD bytes = 0;
    BOOL ok = DeviceIoControl(hDev,
        IOCTL_KBFILTR_SET_BLOCKED_SCANCODE,
        &cmd, sizeof(cmd),
        NULL, 0,
        &bytes, NULL);
    if (!ok) {
        std::cerr << "DeviceIoControl failed. Error: " << GetLastError() << "\n";
    }
    else {
        std::cout << "Command sent successfully\n";
    }

    CloseHandle(hDev);
    free(pDetail);
    SetupDiDestroyDeviceInfoList(hDevInfo);
    return 0;
}
