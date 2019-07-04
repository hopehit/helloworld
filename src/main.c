/****************************************************************
�ļ�˵��������TMS320F2803X DSP�ĵ���������
�ļ��汾�� 
���¸��£� 
****************************************************************/
#include "MotorInclude.h"

extern void ADCOverInterrupt(void);
extern void PWMZeroInterrupt(void);
extern void HardWareErrorDeal(void);

extern void Main2msMotorA(void);
extern void Main2msMotorB(void);
extern void Main2msMotorC(void);
extern void Main2msMotorD(void);

extern void Main05msMotor(void);     
extern void Main0msMotor(void);

//extern void Main2msFunction(void);
//extern void Main05msFunction(void);     
extern void Main05msFunctionA(void);
extern void Main05msFunctionB(void);
extern void Main05msFunctionC(void);
extern void Main05msFunctionD(void);
extern void Main0msFunction(void);
/***************************************************************
-------------------------�����򲿷�-----------------------------
****************************************************************/
void main(void)
{					
	Ulong m_BaseTime,m_DetaTime;
	Uint  m_LoopFlag;
   
   	InitSysCtrl();						// Step 1. Initialize System Control
   
   	InitInterrupt();					// Step 2. Initialize Interrupt service program:
   
   	InitPeripherals(); 					// Step 3. Initialize all the Device Peripherals:
   
   	InitForMotorApp();					// Step 4. User specific code
   	InitForFunctionApp();

	EnableDog();                  // d
	SetInterruptEnable();				// Step 5. enable interrupts:
   	EINT;   							    
   	ERTM;   							    

	m_LoopFlag = 2;
	m_BaseTime = GetTime();
    
  	while(1)							// Step 6. User Application function:
   	{
		m_DetaTime = m_BaseTime - GetTime();
		if(m_DetaTime >= C_TIME_05MS)	//�ж�0.5MS����
        {            
			m_LoopFlag ++;			
			m_BaseTime -= C_TIME_05MS;
 			KickDog();

            #ifdef CPU_TIME_DEBUG
            gCpuTime.Motor05MsBase = GetTime();
            #endif
            Main05msMotor();                        // ����0.5ms����
            #ifdef CPU_TIME_DEBUG
            gCpuTime.Motor05Ms = gCpuTime.Motor05MsBase - GetTime();
            #endif          
                    
			if((m_LoopFlag & 0x03) == 0)            // prA
			{    
			    #ifdef CPU_TIME_DEBUG
				gCpuTime.MFA2msBase = GetTime();
                #endif       
                Main05msFunctionA();
                Main05msFunctionB();
                #ifdef CPU_TIME_DEBUG
				gCpuTime.MFA2ms = gCpuTime.MFA2msBase - GetTime();
                #endif						
			}
            else if((m_LoopFlag & 0x03) == 1)       // prB
            {                                
                #ifdef CPU_TIME_DEBUG
				gCpuTime.MFB2msBase = GetTime();
                #endif
                //Main05msFunctionB();
                Main05msFunctionC();
                //Main05msFunctionD();
                #ifdef CPU_TIME_DEBUG
				gCpuTime.MFB2ms = gCpuTime.MFB2msBase - GetTime();
                #endif	
            }
			else if((m_LoopFlag & 0x03) == 2)       // prC
			{    
			    #ifdef CPU_TIME_DEBUG
				gCpuTime.MFC2msBase = GetTime();
                #endif  
                Main05msFunctionD();
                Main2msMotorA(); 
				//Main2msMotorB(); 
                #ifdef CPU_TIME_DEBUG
				gCpuTime.MFC2ms = gCpuTime.MFC2msBase - GetTime();
                #endif	
                
				//Main2msFunction();  // ִ��ʱ�伸��Ϊ0
			}
            else if((m_LoopFlag & 0x03) == 3)       // prD
            {            
                #ifdef CPU_TIME_DEBUG
				gCpuTime.MFD2msBase = GetTime();
                #endif 
                Main2msMotorB(); 
                Main2msMotorC(); 
				Main2msMotorD(); 
                #ifdef CPU_TIME_DEBUG
				gCpuTime.MFD2ms = gCpuTime.MFD2msBase - GetTime();
                #endif	
            }

            // ����cpuæµϵ��1
            gCpuTime.Det05msClk = __IQsat(m_DetaTime, 65535, C_TIME_05MS);
        }

		Main0msFunction();				//���ȴ�ѭ����ִ�й��ܲ��ֳ���
		Main0msMotor();					//���ȴ�ѭ����ִ�е�����Ʋ��ֳ���
		gCpuTime.tmp0Ms ++;
   	}
} 


/***************************************************************
-----------------------�жϳ��򲿷�-----------------------------
****************************************************************/

/***************************************************************
	1ms�Ķ�ʱ��0�жϣ���ʱû��ʹ��
****************************************************************/
#if 0
interrupt void cpu_timer0_isr(void)
{
   CpuTimer0.InterruptCount++;
   
   PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;	// Acknowledge this interrupt
}
#endif
/***************************************************************
	EPWM�������жϣ�Լ30us
****************************************************************/
interrupt void ADC_Over_isr(void)
{
    EALLOW;             //28035��ΪEALLOW����
    ADC_CLEAR_INT_FLAG;
    EDIS;
	EINT;
	gCpuTime.ADCIntBase = GetTime();
	ADCOverInterrupt();					
	gCpuTime.ADCInt = gCpuTime.ADCIntBase - GetTime();
	DINT;
    EALLOW;
    ADC_RESET_SEQUENCE;
   	PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;	// Acknowledge this interrupt
    EDIS;
}

/***************************************************************
	EPWM�������жϣ�Լ18us
****************************************************************/
interrupt void EPWM1_zero_isr(void)
{
   	EALLOW;
   	EPwm1Regs.ETCLR.bit.INT = 1;
   	EDIS;

	EINT;
	gCpuTime.PWMIntBase = GetTime();
	PWMZeroInterrupt();						//�����жϣ��������ģ�鴦��
	gCpuTime.PWMInt = gCpuTime.PWMIntBase - GetTime();
	DINT;

   	PieCtrlRegs.PIEACK.all = PIEACK_GROUP3;	// Acknowledge this interrupt
}

/***************************************************************
	EPWM�Ĺ����жϣ���Ӳ�������źŴ���
****************************************************************/
interrupt void EPWM1_TZ_isr(void)
{
	DisableDrive();								//���ȷ������
	HardWareErrorDeal();					    //����Ӳ�����ϣ��������ģ�鴦��
                // 
   	PieCtrlRegs.PIEACK.all = PIEACK_GROUP2;	    // Acknowledge this interrupt
}

/***************************************************************
----------------------------END---------------------------------
****************************************************************/
