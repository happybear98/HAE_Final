#ifndef BSW_IO_ULTRASONIC_H_
#define BSW_IO_ULTRASONIC_H_
#include "Ifx_Types.h"

void Init_Ultrasonics(void);
double ReadUltrasonic_noFilt(void);
double SideUltrasonic_noFilt(void);
uint32_t get_Ultrasonic_val(void);

#define FILT_SIZE 5

#endif /* BSW_IO_ULTRASONIC_H_ */

