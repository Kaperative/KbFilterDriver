#ifndef _PUBLIC_H
#define _PUBLIC_H

#define IOCTL_INDEX             0x800

#define IOCTL_KBFILTR_GET_KEYBOARD_ATTRIBUTES CTL_CODE( FILE_DEVICE_KEYBOARD,   \
                                                        IOCTL_INDEX,    \
                                                        METHOD_BUFFERED,    \
                                                        FILE_READ_DATA)
#define IOCTL_KBFILTR_SET_BLOCKED_KEYS  CTL_CODE(FILE_DEVICE_KEYBOARD, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define MAX_BLOCKED_KEYS 32
#pragma pack(push, 1)
typedef struct _BLOCKED_KEYS_CONFIG {
    ULONG Count;                    // Количество клавиш для блокировки
    USHORT Keys[MAX_BLOCKED_KEYS];  // Массив MakeCode'ов клавиш
} BLOCKED_KEYS_CONFIG, * PBLOCKED_KEYS_CONFIG;
#pragma pack(pop)
#endif
