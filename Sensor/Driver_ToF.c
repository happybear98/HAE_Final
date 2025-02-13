/***********************************************************************/
/*Include*/
/***********************************************************************/
#include <Driver_ToF.h>

#include <InterruptPriority.h>

#include "Ifx_Types.h"
#include "ASCLIN_UART.h"
#include "ERR_CALC.h"
#include "KALMAN_FILT.h"

#include <string.h>
#include "math.h"
/***********************************************************************/
/*Define*/
/***********************************************************************/

/***********************************************************************/
/*Typedef*/
/***********************************************************************/

/***********************************************************************/
/*Static Function Prototype*/
/***********************************************************************/
static int verify_checksum(unsigned char data[]);
static int check_tof_strength(unsigned char data[]);
/***********************************************************************/
/*Variable*/
/***********************************************************************/
uint8 g_asclin1_tx_buffer[ASC1_TX_BUFFER_SIZE + sizeof(Ifx_Fifo) + 8];
uint8 g_asclin1_rx_buffer[ASC1_RX_BUFFER_SIZE + sizeof(Ifx_Fifo) + 8];
uint32 g_rx_idx = 0;
uint8 g_buf_tof[16] = { 0 };
IfxAsclin_Asc g_asclin1;
const uint8 g_tof_length = 16;
uint32_t tof_distance = 0;

uint32 ToFCnt = 0;
uint32 filtSum = 0;
float32_t ERR = 0.0;
float32_t dev = 0.0;
float32_t MEAN = 0.0;

KalmanFilter kaf;
uint8_t initFlag = 0;
/***********************************************************************/
/*Function*/
/***********************************************************************/
IFX_INTERRUPT(asclin1_tx_ISR, 0, ISR_PRIORITY_ASCLIN1_TX);
void asclin1_tx_ISR(void)
{
    IfxAsclin_Asc_isrTransmit(&g_asclin1);
}

IFX_INTERRUPT(asclin1_rx_ISR, 0, ISR_PRIORITY_ASCLIN1_RX);
void asclin1_rx_ISR(void)
{
    IfxAsclin_Asc_isrReceive(&g_asclin1);
    static uint8 s_rx_buf[16] = { 0 };

    uint8 c = (uint8)IfxAsclin_Asc_blockingRead(&g_asclin1);

    s_rx_buf[g_rx_idx] = c;
    ++g_rx_idx;

    if (g_rx_idx == g_tof_length) {
        memcpy(g_buf_tof, s_rx_buf, g_tof_length);
        g_rx_idx = 0;
    }
}

IFX_INTERRUPT(asclin1_err_ISR, 0, ISR_PRIORITY_ASCLIN1_ER);
void asclin1_err_ISR(void)
{
    ;
}
void init_tof(void)
{
    IfxAsclin_Asc_Config asc_conf;

    /* Set default configurations */
    IfxAsclin_Asc_initModuleConfig(&asc_conf, &MODULE_ASCLIN1); /* Initialize the structure with default values      */

    /* Set the desired baud rate */
    asc_conf.baudrate.baudrate = ASC1_BAUDRATE;                                   /* Set the baud rate in bit/s       */
    asc_conf.baudrate.oversampling = IfxAsclin_OversamplingFactor_16;            /* Set the oversampling factor      */
    asc_conf.baudrate.prescaler = IfxAsclin_OversamplingFactor_5;
    /* Configure the sampling mode */
    asc_conf.bitTiming.medianFilter = IfxAsclin_SamplesPerBit_three;             /* Set the number of samples per bit*/
    asc_conf.bitTiming.samplePointPosition = IfxAsclin_SamplePointPosition_7;    /* Set the first sample position    */

    asc_conf.frame.dataLength = IfxAsclin_DataLength_8;
    /* ISR priorities and interrupt target */
    asc_conf.interrupt.txPriority = ISR_PRIORITY_ASCLIN1_TX;  /* Set the interrupt priority for TX events             */
    asc_conf.interrupt.rxPriority = ISR_PRIORITY_ASCLIN1_RX;  /* Set the interrupt priority for RX events             */
    asc_conf.interrupt.erPriority = ISR_PRIORITY_ASCLIN1_ER;  /* Set the interrupt priority for Error events          */
    asc_conf.interrupt.typeOfService = IfxSrc_Tos_cpu0;

    /* Pin configuration */
    const IfxAsclin_Asc_Pins pins = {
            .cts        = NULL_PTR,                         /* CTS pin not used                                     */
            .ctsMode    = IfxPort_InputMode_pullUp,
            .rx         = &IfxAsclin1_RXB_P15_5_IN ,        /* Select the pin for RX connected to the USB port      */
            .rxMode     = IfxPort_InputMode_pullUp,         /* RX pin                                               */
            .rts        = NULL_PTR,                         /* RTS pin not used                                     */
            .rtsMode    = IfxPort_OutputMode_pushPull,
            .tx         = &IfxAsclin1_TX_P15_4_OUT,         /* Select the pin for TX connected to the USB port      */
            .txMode     = IfxPort_OutputMode_pushPull,      /* TX pin                                               */
            .pinDriver  = IfxPort_PadDriver_cmosAutomotiveSpeed1
    };
    asc_conf.pins = &pins;

    /* FIFO buffers configuration */
    asc_conf.txBuffer = g_asclin1_tx_buffer;                      /* Set the transmission buffer                          */
    asc_conf.txBufferSize = ASC1_TX_BUFFER_SIZE;              /* Set the transmission buffer size                     */
    asc_conf.rxBuffer = g_asclin1_rx_buffer;                      /* Set the receiving buffer                             */
    asc_conf.rxBufferSize = ASC1_RX_BUFFER_SIZE;              /* Set the receiving buffer size                        */

    /* Init ASCLIN module */
    IfxAsclin_Asc_initModule(&g_asclin1, &asc_conf);          /* Initialize the module with the given configuration   */
}

static int verify_checksum(unsigned char data[])
{
    uint8 checksum = 0;
    for (int i = 0; i < g_tof_length - 1; i++) {
        checksum += data[i];
    }
    if (data[0] == 0x57 && data[1] == 0x0 && data[2] == 0xFF) {
        return checksum == data[g_tof_length - 1];
    } else {
        return 0;
    }
}

static int check_tof_strength(unsigned char data[])
{
    sint32 tof_distance = data[8] | (data[9] << 8) | (data[10] << 16);
    sint32 tof_signal_strength = data[12] | (data[13] << 8);
    /* when distance over 2m - out of range */

    if (tof_signal_strength != 0 && tof_distance != 0xFFFFF6u) {
        return 1;
    } else {
        return 0;
    }
}

uint32 get_tof_distance(void)
{
    uint8 buf_tof[16];

    memcpy(buf_tof, g_buf_tof, g_tof_length);

    if (!verify_checksum(buf_tof)) {
        return -1;
    }
    if (!check_tof_strength(buf_tof)) {
        return -2;
    }

    tof_distance = buf_tof[8] | (buf_tof[9] << 8) | (buf_tof[10] << 16);

    if(tof_distance < 0)
    {
        tof_distance = 0;
    }

    //Filt_ErrVal(tof_distance);

    if(tof_distance != 0) {
        if(initFlag == 0){
            initFlag = 1;
            kalman_init(&kaf, (float32_t)tof_distance);
        }
        kalman_update(&kaf, (float32_t)tof_distance);
    }

    return tof_distance;
}

/*
float32_t Filt_ErrVal(uint32 val){
    if(val !=0 ){
        ToFCnt++;
        filtSum += val;
        MEAN = (float32_t)(filtSum / (float32_t)ToFCnt);

        dev = fabsf(val - MEAN);
        if(ToFCnt != 0){
            ERR = ((ERR + dev) / (float32_t)ToFCnt);
            calc_RMSE(val, ToFCnt);
            calc_MAE(val, ToFCnt);
            calc_Rel_Err(val, ToFCnt);
        }
    }
    return ERR;
}
*/
