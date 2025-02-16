#ifndef GTM_HANDLE_H_
#define GTM_HANDLE_H_


#include "Ifx_Types.h"
#include "IfxGtm_Atom_Pwm.h"
#include "Bsp.h"
#include "stdlib.h"

/*********************************************************************************************************************/
/*-----------------------------------------------Function Prototypes-------------------------------------------------*/
/*********************************************************************************************************************/
void initHANDLEServo(void);
void HandleAngle(int L_Rpm, int R_Rpm);

void setHANDLEDuty(uint32 dutyCycle);
void updateHANDLEServoDuty(int newCurr);

#endif /* GTM_HANDLE_H_ */
