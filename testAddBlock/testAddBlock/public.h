#ifndef _PUBLIC_H
#define _PUBLIC_H

#define IOCTL_INDEX             0x800

#define IOCTL_KBFILTR_GET_KEYBOARD_ATTRIBUTES CTL_CODE( FILE_DEVICE_KEYBOARD,   \
                                                        IOCTL_INDEX,    \
                                                        METHOD_BUFFERED,    \
                                                        FILE_READ_DATA)
#define IOCTL_KBFILTR_SET_BLOCKED_KEYS  CTL_CODE(FILE_DEVICE_KEYBOARD, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_KBFILTR_SET_REMAPPED_KEYS CTL_CODE(FILE_DEVICE_KEYBOARD, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define MAX_BLOCKED_KEYS 255
#define MAX_REMAPPED_KEYS 255
#pragma pack(push, 1)
typedef struct _BLOCKED_KEYS_CONFIG {
    ULONG OperationFlag;
    ULONG Count;                    // Количество клавиш для блокировки
    USHORT Keys[MAX_BLOCKED_KEYS];  // Массив MakeCode'ов клавиш
} BLOCKED_KEYS_CONFIG, * PBLOCKED_KEYS_CONFIG;

typedef struct _KEY_REMAP_ENTRY {
    USHORT OriginalMakeCode;
    USHORT NewMakeCode;
} KEY_REMAP_ENTRY, * PKEY_REMAP_ENTRY;

typedef struct _KEY_REMAP_CONFIG {
    ULONG Count;
    KEY_REMAP_ENTRY Remaps[MAX_REMAPPED_KEYS];
} KEY_REMAP_CONFIG, * PKEY_REMAP_CONFIG;
#pragma pack(pop)
#endif
