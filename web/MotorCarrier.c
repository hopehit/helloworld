/****************************************************************
�ļ����ܣ���Ƶ���ز�������ؼ���,�������Ʒ�ʽ��ȷ��
�ļ��汾:
�������ڣ� 

****************************************************************/
#include "MotorInclude.h"
#include "MotorEncoder.h"
#include "MotorPwmInclude.h"

// ȫ�ֱ�������
FC_CAL_STRUCT			gFcCal;
SYN_PWM_STRUCT			gSynPWM;
ANGLE_STRUCT			gPhase;		//�ǶȽṹ
DEAD_BAND_STRUCT		gDeadBand;

// ͬ������Ƶ�ʷֶα��ز��ȱ�ÿ�ز����ڲ�����
Uint const gSynFreqTable[10] =
{
		75,		113, 	169, 	250, 	355, 
//NC=	80		60		42		30		20
		535, 	854, 	1281, 	1922, 	65535
//NC=	12		9		6		3
};

Uint const gSynNcTable[10] = 
{
		80,		60,		42,		30,		20,
		12,		9,		6,		3,		1
};

Ulong const gSynStepAngleTable[10] = 	//65536*65536/N
{
	53687091,	71582788,	102261126,	143165576,	214748365,	
	357913941,	477218588,	715827883,	1431655765, 4294967295 
};

// �ļ��ڲ���������
void AsynPWMAngleCal(Ulong);
void SynPWMAngleCal(void);

/************************************************************
�����ز�Ƶ��(2ms)��
����:   gBasePar.FcSet�� gTemperature.Temp��
���:   gBasePar.FcSetApply��

1. ��Ƶ���¶Ƚ��͵�������1K���趨Ƶ�ʵ���1K��������
2. ����С��12����Ƶ������12K��С��15����Ƶ������10K������15���ϣ������Ƶ8K��
   ��С��Ƶ0.8K������Ƶ��С��8Hz����Ƶ���5KHz��
3. VC���е���Ƶ��Χ��2K��8K��
4. ��Ƶ����ʱÿ40ms����0.1KHz��
************************************************************/
void CalCarrierWaveFreq(void)
{
	Uint	m_TempLim;
    Uint    m_MaxFc;
    Uint    m_MinFc;

// ȷ���ز�Ƶ��
	gFcCal.Time++;

	gFcCal.Cnt += gBasePar.FcSetApply;              //��Ƶ�͵�ʱ�򣬵�������һЩ
	if(gFcCal.Cnt < 100)				
	{
		return;
	}
	gFcCal.Cnt = 0;
	
    //...�˴���FC���¶ȹ�ϵ�ļ���
	m_TempLim = 70;
    if(gInvInfo.InvTypeApply >= 19)
    {
        m_TempLim = 70;
    }

	if((gSubCommand.bit.VarFcByTem != 0) &&         //��Ƶ���¶ȵ�������ѡ��
	   (gMainStatus.RunStep != STATUS_GET_PAR) &&   // ��гʱ���ز�������
	   (gTemperature.Temp > (m_TempLim-5)) &&       //�¶ȴ��ڣ�����ֵ-5����
	   (10 < gBasePar.FcSet))                       //��Ƶ���¶Ƚ��͵�������1K���趨Ƶ�ʵ���1K������
	{
		if(gFcCal.Time >= 10000)				    //ÿ20�����һ��
		{
			gFcCal.Time = 0;
			if(gTemperature.Temp <= m_TempLim)	    //�ز�Ƶ���𽥻���
			{
				gFcCal.FcBak += 5;
			}
			else if(gTemperature.Temp >= (m_TempLim+5))
			{
				gFcCal.FcBak -= 5;				    //�ز�Ƶ���𽥽���
				gFcCal.FcBak = (gFcCal.FcBak<10) ? 10 : gFcCal.FcBak;
			}
		}
	}
	else										    //�ز�Ƶ�ʲ���������
	{
		gFcCal.Time = 0;
		gFcCal.FcBak = gBasePar.FcSet;
	}
    gFcCal.FcBak = (gFcCal.FcBak > gBasePar.FcSet) ? gBasePar.FcSet : gFcCal.FcBak;
    //���ݵ�ǰ�Ĺ��������С�ز�Ƶ�ʣ��������ص�50����ʼ�����ز�Ƶ�ʣ���4KHz
	if(gOverLoad.InvTotal.half.MSW < 16000)
	{ 
		gFcCal.FcLimitOvLoad = gBasePar.FcSet*10;
	}
	else if(gOverLoad.InvTotal.half.MSW > 18000)
	{
		gFcCal.FcLimitOvLoad--;
		if(gFcCal.FcLimitOvLoad < 400)	gFcCal.FcLimitOvLoad = 400;
	}
	if(gFcCal.FcBak > (gFcCal.FcLimitOvLoad/10))
	{
		gFcCal.FcBak = (gFcCal.FcLimitOvLoad/10);
	}
//...��ʼ��FC��������С����
	m_MinFc = 8;								    //��С0.8KHz�ز�Ƶ��
	m_MaxFc = 120;								    //���12KHz�ز�Ƶ��
	if(gInvInfo.InvTypeApply > 15)		
	{
		m_MaxFc = 80;							    //����15�������8KHz�ز�Ƶ��
	}
	else if(gInvInfo.InvTypeApply > 12)	
	{
		m_MaxFc = 100;							    //����12�������10KHz�ز�Ƶ��
	}
	if(gMainCmd.Command.bit.ControlMode != IDC_VF_CTL)
	{
		m_MaxFc = (m_MaxFc>80)?80:m_MaxFc;		    //VC����8KHz����ز�Ƶ������
		m_MinFc = 20;							    //VC����2KHz��С�ز�Ƶ������
	}
	if(gMainCmd.FreqReal < 800)					
	{
		m_MaxFc = (m_MaxFc>50)?50:m_MaxFc;		    //8Hz����5KHz����ز�Ƶ������
	}
    if(INV_VOLTAGE_1140V == gInvInfo.InvVoltageType)
    {
		m_MaxFc = (m_MaxFc>10)?10:m_MaxFc;		    //1140V�����Ƶ1K����С0.5K
		m_MinFc = 8;							    //2011.5.7 L1082
    }
    else if(INV_VOLTAGE_690V == gInvInfo.InvVoltageType)
    {
        m_MaxFc = (m_MaxFc>40)?40:m_MaxFc;		    //690V�����Ƶ4K����С0.8K
		m_MinFc = 8; 
    }
	gFcCal.FcBak = __IQsat(gFcCal.FcBak,m_MaxFc,m_MinFc);


//...��ʼ�����ز�Ƶ�ʣ�ÿ40ms����0.1KHz
	if(gFcCal.FcBak > gBasePar.FcSetApply)
	{
		gBasePar.FcSetApply ++;
	}
	else if(gFcCal.FcBak < gBasePar.FcSetApply)
	{
		gBasePar.FcSetApply --;
	}

// ͬ���������첽���Ƶ�ѡ��
	gSynPWM.AbsFreq = gMainCmd.FreqReal/100;
    // �Զ��л�
	if((IDC_VF_CTL != gMainCmd.Command.bit.ControlMode)||
	   (gExtendCmd.bit.ModulateType == 0)	|| 
	   (gSynPWM.AbsFreq < gSynFreqTable[0]-10))     // 65Hz
	{
		gSynPWM.ModuleApply = 0;
	}
	else
	{
		if((gSynPWM.ModuleApply == 0) && (gSynPWM.AbsFreq > gSynFreqTable[0]+10))
		{
			gSynPWM.ModuleApply = 1;
			gSynPWM.Index       = 0;
			gSynPWM.Flag        = 0;
		}
		else if((gSynPWM.ModuleApply >= 1) && (gSynPWM.AbsFreq < gSynFreqTable[0]-10))
		{
			gSynPWM.ModuleApply = 0;
		}
	}
    if(gSynPWM.ModuleApply == 0)
    {
        gSynPWM.FcApply = ((Ulong)gBasePar.FcSetApply)<<9;
    }
// ��������Ƶ�ʼ��������ѹ��λ�Ƕȱ仯���� : gPhase.StepPhase
	if(gSynPWM.ModuleApply == 1)		
	{
		SynPWMAngleCal();
	}
	else								
	{		
		AsynPWMAngleCal(gSynPWM.FcApply);
	}
// DPWM �� CPWM ��ѡ��
    // CPWM����DPWM���Ʒ�ʽ�Զ��л�����ѭ280���Զ��л�ԭ��
	if((gBasePar.FcSetApply <= 15) || ( 1 == gExtendCmd.bit.ModulateType) ||(gCtrMotorType != ASYNC_VF)	)	
	{
		gPWM.PWMModle = MODLE_CPWM;			//��Ƶ���ϣ��ȫ��CPWM����
	}
	else if( gMainCmd.FreqReal > gPWM.PwmModeSwitchLF + 300) /*CPWM��DPWM�л�ͨ��Ƶ�ʵ�2011.5.7 L1082*/
	{
        gPWM.PWMModle = MODLE_DPWM;
	}
	else if( gMainCmd.FreqReal < gPWM.PwmModeSwitchLF)
	{
        gPWM.PWMModle = MODLE_CPWM;
	}

}

/************************************************************
0.5ms����:
	���ݵ�ǰ�ز�Ƶ�ʺ�����Ƶ�ʼ����ز���������λ�仯������
	����ͬ�����ƺ��첽����

1. ��VC��SVC����ʱʹ���첽���ƣ�
2. ����Ƶ��С��75Hzʹ���첽���ƣ�
3. �ֶ�ѡ���첽������Ч��
4. ֻ����Vf���ƣ���������Ƶ�ʴ���75Hzʱ�������ֶ�ѡ��ͬ�����Ʋ��л���ͬ�����ƣ�
5. 75Hz�л�ʱ������10Hz���ͻ���
************************************************************/

/************************************************************
	�첽��������¸��ݵ�ǰ�ز�Ƶ�ʺ�����Ƶ�ʼ����ز���������λ�仯����
Input:    gFCApply  gSpeed.SpeedGivePer
OutPut:   gPhase.StepPhase  gPhase.CompPhase gPWMTc
TC/2 = 250000/Fc  
StepPhase = (TC/2)*f*Maxf*1125900 >> 32
************************************************************/
void AsynPWMAngleCal(Ulong FcApply)
{
	Uint  m_HalfTc,m_AbsFreq,m_Fc;

	Ulong m_LData = (Ulong)PWM_CLOCK_HALF * 10000L * 512L;
	if(FcApply > 61440)
	{
		m_Fc = FcApply>>1;
		m_LData = m_LData>>1;
	}
	else
	{
		m_Fc = FcApply;
	}
	m_HalfTc = (Uint)(m_LData/m_Fc);

	m_AbsFreq  = abs(gMainCmd.FreqSyn);
    m_LData = ((Ullong)m_AbsFreq * (Ullong)gBasePar.FullFreq01 * 439804651L / m_Fc)>>16;

	DINT;
	if(gMainCmd.FreqSyn >= 0)
	{
		gPhase.StepPhase = m_LData;
		gPhase.CompPhase = (m_LData>>15) + COM_PHASE_DEADTIME;
	}
	else
	{
		gPhase.StepPhase = -m_LData;
		gPhase.CompPhase = -(m_LData>>15) - COM_PHASE_DEADTIME;
	}
    
	gPWM.gPWMPrd = m_HalfTc;
	EINT;
}

/************************************************************
	ͬ�������¼����ز����ں�ÿ�ز����ڵĽǶȱ仯����
************************************************************/
void SynPWMAngleCal(void)
{
	Uint  m_AbsFreq,m_HalfTc,m_Index;
	Ulong m_Fc,m_Long,m_LData;

	m_Index = 0;
	while(gSynPWM.AbsFreq >= gSynFreqTable[m_Index]) 	
	{
		m_Index++;
		if(m_Index >= 10)	break;
	}

	m_Index--;
	if((m_Index < gSynPWM.Index) && 
	   (gSynPWM.AbsFreq <= gSynFreqTable[gSynPWM.Index]-10))
	{
		gSynPWM.Flag  = 1;					//��ʾƵ�ʼ�С���ز�Ƶ��ͻȻ���ӵĹ��ɹ���
		gSynPWM.Index = m_Index;
	}
	else if((m_Index > gSynPWM.Index) && 
	        (gSynPWM.AbsFreq > gSynFreqTable[gSynPWM.Index+1]+10))
	{
		gSynPWM.Flag  = 2;					//��ʾƵ�����ӡ��ز�Ƶ��ͻȻ��С�Ĺ��ɹ���
		gSynPWM.Index = m_Index;
	}

	m_AbsFreq  = abs(gMainCmd.FreqSyn);
	m_Long = ((Ullong)gBasePar.FullFreq01 * (Ullong)m_AbsFreq)>>7;
	m_Long = ((Ullong)m_Long * (Ullong)gSynNcTable[gSynPWM.Index] / 100)>>8;
    
	m_Fc = (((Ullong)m_Long<<9))/100;			//��ǰ�ز�Ƶ��

	if(gSynPWM.Flag == 1)
	{
		gSynPWM.FcApply += 100;				//ÿ�ز����ڱ仯Լ0.02KHz�ز�Ƶ��
		if(gSynPWM.FcApply > m_Fc)
		{
			gSynPWM.Flag = 0;
		}
	}
	else if(gSynPWM.Flag == 2)
	{
		gSynPWM.FcApply -= 100;
		if(gSynPWM.FcApply < m_Fc)
		{
			gSynPWM.Flag = 0;
		}
	}
	else
	{
		gSynPWM.FcApply = m_Fc;
	}

	if(gSynPWM.Flag != 0)
	{
		AsynPWMAngleCal(gSynPWM.FcApply);
		return;
	}
    m_LData = (Ulong)PWM_CLOCK_HALF * 1000000L;
	m_HalfTc = m_LData/m_Long;

	DINT;
	if(gMainCmd.FreqSyn >= 0)
	{
		gPhase.StepPhase = gSynStepAngleTable[gSynPWM.Index];
		gPhase.CompPhase = (gSynStepAngleTable[gSynPWM.Index]>>15) + COM_PHASE_DEADTIME;
	}
	else
	{
		gPhase.StepPhase = -gSynStepAngleTable[gSynPWM.Index];
		gPhase.CompPhase = -(gSynStepAngleTable[gSynPWM.Index]>>15) - COM_PHASE_DEADTIME;
	}

	gPWM.gPWMPrd = m_HalfTc;
	EINT;
}

/*************************************************************
	���PWM����ʹ�ز����ں������λ��Ч

���PWN���õ������ǣ�
   �첽���� + ��Ƶ1k-6k֮�� + Vf���� + �ֶ�ѡ�����ã�
*************************************************************/
void SoftPWMProcess(void)
{
	Uint  m_Coff;

	if((IDC_VF_CTL != gMainCmd.Command.bit.ControlMode) ||            // ֻ��Vf���������PWM		
        (gPWM.SoftPWMTune == 0) || (gSynPWM.ModuleApply >= 1) ||     // ͬ�������²��������PWM     
	   (gBasePar.FcSetApply < 10) || (gBasePar.FcSetApply > 60)) // �ز�Ƶ��1K��6K֮����Ч
	{
	    gPWM.gPWMPrdApply = gPWM.gPWMPrd;	        		
   	    gPhase.StepPhaseApply = gPhase.StepPhase;	
		return;
	}
    
    m_Coff = 3277;
    if( 30 < gBasePar.FcSetApply )
    {
        m_Coff = 1966;
    }
	m_Coff = (gPWM.SoftPWMTune * m_Coff)>>4;    //���PWM�ĵ��ڿ��10��Ӧ50%;����3Kʱ�����Ӧ30%
	gPWM.SoftPWMCoff = gPWM.SoftPWMCoff + 29517 * GetTime();    //����һ��Q15��ʽ�������
	m_Coff = 4096 + (((long)gPWM.SoftPWMCoff * (long)m_Coff)>>15);

 	gPWM.gPWMPrdApply = ((Ulong)gPWM.gPWMPrd * (Ulong)m_Coff)>>12;
    gPhase.StepPhaseApply = (Ulong)(gPhase.StepPhase>>12) * (Ulong)m_Coff;
}


/************************************************************
	���������ѹ����λ�ǣ�VF�����ֱ���ۼ�
************************************************************/
void CalOutputPhase(void)
{ 
	//if(DC_CONTROL == gCtrMotorType) 	//ֱ���ƶ�
	//{
	    // ��ѹ������2ms���㣬�첽�����迼�ǵ�ѹ��λ��
	    // ͬ������ʱ��M�᷽�򷢵�ѹ�� ��ѹ�Ƕ�����Ϊ0��
	    // gPhase.OutPhase ������
		//return;
	//}

    // ͬ������ȡת��λ�ã�����ת�Ӵų�����
    if(MOTOR_TYPE_PM == gMotorInfo.MotorType)
    {
		if(0 == gPGData.PGMode)                         // ����ʽ uvw, Abz
		{
			SynCalRealPos();                            // ͬ������ʶ��ʱ�򲻸��� gIPMPos.RotorPos
			// �ǿ��ر�ʶʱ:
			            
        	if((gMainStatus.RunStep != STATUS_GET_PAR) ||
        	   (gMainStatus.RunStep == STATUS_GET_PAR && TUNE_STEP_NOW != PM_EST_NO_LOAD)        	   
        	   )
        	{
        	    if(STATUS_IPM_INIT_POS != gMainStatus.RunStep)
                {   
            		gIPMPos.RotorPos = gIPMZero.FeedPos;            // ���ر�ʶ��ʱ��gIPMPos.RotorPos�Զ��ۼ�        		
                    gIPMPos.ABZ_RotorPos_Ref = gIPMZero.FeedABZPos;
               }
            }
		}
		else if(gPGData.PGType == PG_TYPE_RESOLVER)             // ������ʽ, ����
		{
			if(gMainStatus.RunStep != STATUS_GET_PAR)       //ͬ�������ر�ʶ������gIPMPos.RotorPos   
			{
				gIPMPos.RotorPos = gRotorTrans.RTPos + gIPMPos.RotorZero + gRotorTrans.PosComp;
			}
            else if(gMainStatus.RunStep == STATUS_GET_PAR && TUNE_STEP_NOW != PM_EST_NO_LOAD)
            {            
                gIPMPos.RotorPos = gRotorTrans.RTPos + gPmParEst.EstZero;// + gRotorTrans.PosComp;
            }
            else            // ������ر�ʶʱ�� �Ƕ��ɱ�ʶ�ۼ�
            {
                ;   // ??
            }
		}
        
        // ת�Ӵų�����
        if(TUNE_STEP_NOW == PM_EST_BEMF)           // ���綯�Ʊ�ʶ�� �ɹ��ܸ���ת�ټ�����λ������
        {
            gPhase.IMPhase += gPhase.StepPhaseApply;
        }
        else
        {
            gPhase.IMPhase  = ((long)gIPMPos.RotorPos) << 16;	// ��������
        }
	}

    if((gMotorInfo.MotorType != MOTOR_TYPE_PM) ||  // �첽����λ���ۼ�		
        (gCtrMotorType == SYNC_VF || gCtrMotorType == SYNC_SVC))    // ͬ����VF ��SVC
	{
		gPhase.IMPhase += gPhase.StepPhaseApply;
	}
    
	gPhase.OutPhase = (int)(gPhase.IMPhase>>16) + gOutVolt.VoltPhaseApply;
}

/*************************************************************
	�����������������ж�
*************************************************************/
void CalDeadBandComp(void)
{
	int   phase,m_Com;
	//int	  DetaPhase;

	if(gMainCmd.FreqReal <= 40000)	    // 400.00Hz
	{
		m_Com = gDeadBand.Comp;
	}
	else 
	{
		m_Com = (int)(((long)gDeadBand.Comp * (long)(gMainCmd.FreqReal - 40000))>>15);
	}
   
	if((gMotorInfo.MotorType == MOTOR_TYPE_PM) && (gDeadBand.InvCurFilter < 512))
	{
	    m_Com = ((long)m_Com * gDeadBand.InvCurFilter)>>9;
	}
    
	if((gMainCmd.Command.bit.StartDC == 1) || (gMainCmd.Command.bit.StopDC == 1))
        
	{
        m_Com = 0;
	}
    
    gIAmpTheta.ThetaFilter = gIAmpTheta.Theta;/*��������ΪAD�в�����MD320 0.5MS����ȡ�����Ƕ��˲�ȡ�� 2011.5.7 L1082 */

    
	phase = (int)(gPhase.IMPhase>>16) + gIAmpTheta.ThetaFilter + gPhase.CompPhase + 16384;
 #if 1
   gDeadBand.CompU = (phase <= 0) ? m_Com : (-m_Com);

	phase -= 21845;
	gDeadBand.CompV = (phase <= 0) ? m_Com : (-m_Com);

	phase -= 21845;
	gDeadBand.CompW = (phase <= 0) ? m_Com : (-m_Com);
 
#else        
    if(phase <= 0)
	{
		if((phase >= -32504)&&(phase <= -264))//32404,364
		{
			gDeadBand.CompU = m_Com;
		}
		else
		{
			gDeadBand.CompU = 0;
		}
	}
	else
	{
		if((phase <= 32504)&&(phase >= 264))
		{
			gDeadBand.CompU = (-m_Com);
		}
		else
		{
			gDeadBand.CompU = 0;
		}
	}

	phase -= 21845;

	if(phase <= 0)
	{
		if((phase >= -32504)&&(phase <= -264))
		{			
			gDeadBand.CompV = m_Com;
		}
		else
		{
			gDeadBand.CompV = 0;
		}
	}
	else
	{
		if((phase <= 32504)&&(phase >= 264))
		{			
			gDeadBand.CompV = (-m_Com);
		}
		else
		{
			gDeadBand.CompV = 0;
		}
	}
	

	phase -= 21845;

	if(phase <= 0)
	{
		if((phase >= -32504)&&(phase <= -264))
		{			
			gDeadBand.CompW = m_Com;			
		}
		else
		{
			gDeadBand.CompW = 0;
		}
	}
	else
	{
		if((phase <= 32504)&&(phase >= 264))
		{
			gDeadBand.CompW = (-m_Com);			
		}
		else
		{
			gDeadBand.CompW = 0;
		}
	}
 #endif
 }
#if 0
// �첽��SVC ����������
void ImSvcDeadBandComp()
{
    ;
}

void ImFvcDeadBandComp()
{
    ;
}

void PmFvcDeadBandComp()
{
    ;
}
#endif

