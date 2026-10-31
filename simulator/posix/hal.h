#pragma once

#include "ch.h"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#ifndef HAL_SUCCESS
#define HAL_SUCCESS 0
#endif
#ifndef HAL_FAILED
#define HAL_FAILED (-1)
#endif

// HAL feature toggles for POSIX simulator
#ifndef HAL_USE_SPI
#define HAL_USE_SPI 0
#endif
#ifndef HAL_USE_I2C
#define HAL_USE_I2C 0
#endif
#ifndef HAL_USE_PWM
#define HAL_USE_PWM 0
#endif
#ifndef HAL_USE_ICU
#define HAL_USE_ICU 0
#endif
#ifndef HAL_USE_GPT
#define HAL_USE_GPT 0
#endif
#ifndef HAL_USE_USB
#define HAL_USE_USB 0
#endif
#ifndef HAL_USE_SDC
#define HAL_USE_SDC 0
#endif
#ifndef HAL_USE_RTC
#define HAL_USE_RTC 0
#endif
#ifndef HAL_USE_WDG
#define HAL_USE_WDG 0
#endif
#ifndef HAL_USE_SERIAL_USB
#define HAL_USE_SERIAL_USB 0
#endif
#ifndef HAL_USE_SERIAL
#define HAL_USE_SERIAL 0
#endif
#ifndef HAL_USE_UART
#define HAL_USE_UART 0
#endif
#ifndef HAL_USE_CAN
#define HAL_USE_CAN 1
#endif
#ifndef HAL_USE_ADC
#define HAL_USE_ADC 0
#endif
#ifndef HAL_USE_PAL
#define HAL_USE_PAL 0
#endif

typedef int ioportid_t;
typedef int iomode_t;
typedef uint32_t ioportmask_t;

#if HAL_USE_CAN
#ifndef CAN_ENFORCE_USE_CALLBACKS
#define CAN_ENFORCE_USE_CALLBACKS FALSE
#endif
#ifndef CAN_USE_SLEEP_MODE
#define CAN_USE_SLEEP_MODE FALSE
#endif
typedef enum {
	CAN_UNINIT = 0,
	CAN_STOP = 1,
	CAN_STARTING = 2,
	CAN_STOPPING = 3,
	CAN_READY = 4,
	CAN_SLEEP = 5
} canstate_t;
typedef void (*can_callback_t)(void);

#include "hal_can_lld.h"

#ifndef CAN_ANY_MAILBOX
#define CAN_ANY_MAILBOX 0U
#endif

#ifdef __cplusplus
extern "C" {
#endif
void canInit(void);
void canObjectInit(CANDriver* canp);
void canStart(CANDriver* canp, const CANConfig* config);
void canStop(CANDriver* canp);
msg_t canTransmitTimeout(CANDriver* canp, canmbx_t mailbox, const CANTxFrame* ctfp, sysinterval_t timeout);
msg_t canReceiveTimeout(CANDriver* canp, canmbx_t mailbox, CANRxFrame* crfp, sysinterval_t timeout);
msg_t canTransmit(CANDriver* canp, canmbx_t mailbox, const CANTxFrame* ctfp, sysinterval_t timeout);
msg_t canReceive(CANDriver* canp, canmbx_t mailbox, CANRxFrame* crfp, sysinterval_t timeout);
void _can_rx_full_isr(CANDriver* canp, canmbx_t mailbox);
#ifdef __cplusplus
}
#endif
#endif

/* Real ChibiOS halInit() initialises every enabled HAL driver. Skipping the CAN one left
 * CAND1..3 with the zero-initialised sock of a global, i.e. fd 0 (stdin) rather than the -1
 * that can_lld_transmit() checks for -- so every frame was written to stdin and reported as
 * an EBADF failure, once per frame. */
static inline void halInit(void) {
#if HAL_USE_CAN
	canInit();
#endif
}

static inline void palWritePad(ioportid_t port, int pad, int mode) { (void)port; (void)pad; (void)mode; }
static inline int palReadPad(ioportid_t port, int pad) { (void)port; (void)pad; return 0; }
static inline void palSetPadMode(ioportid_t port, int pad, iomode_t mode) { (void)port; (void)pad; (void)mode; }

// PAL mode placeholders
#ifndef PAL_MODE_OUTPUT_PUSHPULL
#define PAL_MODE_OUTPUT_PUSHPULL 0
#endif
#ifndef PAL_MODE_OUTPUT_OPENDRAIN
#define PAL_MODE_OUTPUT_OPENDRAIN 0
#endif
#ifndef PAL_MODE_INPUT
#define PAL_MODE_INPUT 0
#endif
#ifndef PAL_MODE_ALTERNATE
#define PAL_MODE_ALTERNATE(x) 0
#endif

// No-op queue reset helpers
static inline void iqResetI(void* queue) { (void)queue; }
static inline void oqResetI(void* queue) { (void)queue; }

// Channel/event flags (subset)
#define CHN_CONNECTED        (eventflags_t)1
#define CHN_DISCONNECTED     (eventflags_t)2

// Debug checks
#define osalDbgCheck(x) do { (void)(x); } while (0)

// IRQ prologue/epilogue placeholders
#define CH_IRQ_PROLOGUE()
#define CH_IRQ_EPILOGUE()

// Basic sequential stream type placeholder
typedef struct BaseSequentialStream {
	int dummy;
} BaseSequentialStream;

// Forward declarations for CAN/serial types if referenced
typedef struct SerialDriver SerialDriver;
typedef struct UARTDriver UARTDriver;
typedef struct CANDriver CANDriver;
