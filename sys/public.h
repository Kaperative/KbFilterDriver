#ifndef _PUBLIC_H
#define _PUBLIC_H

#define IOCTL_INDEX             0x800

#define IOCTL_KBFILTR_GET_KEYBOARD_ATTRIBUTES CTL_CODE( FILE_DEVICE_KEYBOARD,   \
                                                        IOCTL_INDEX,    \
                                                        METHOD_BUFFERED,    \
                                                        FILE_READ_DATA)

#define IOCTL_KBFILTR_SET_BLOCKED_SCANCODE CTL_CODE( FILE_DEVICE_KEYBOARD, \
                                                    IOCTL_INDEX + 1, \
                                                    METHOD_BUFFERED, \
                                                    FILE_WRITE_DATA)

#define KBFILTR_POOL_TAG 'tlfK'

#endif
