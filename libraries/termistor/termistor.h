#ifndef TERMISTOR_H
#define TERMISTOR_H

//////////////////////////////////////////////////////////
/// Настройки термистора (если используется)
//////////////////////////////////////////////////////////
// бета коэффициент термистора (обычно 3000-4000)
#define BCOEFFICIENT 3950
// температура, при которой замерено номинальное сопротивление
#define TEMPERATURENOMINAL 25
// сопротивление при 25 градусах по Цельсию
#define THERMISTOR_OM 10000
// сопротивление второго резистора
#define RESISTOR_OM 9850

#define TERM_ADC_TO_C(v,adc)  { v=1023.0/adc-1;						\
								v=RESISTOR_OM/v;					\
								v/=THERMISTOR_OM;					\
								v=log(v);							\
                        		v/=BCOEFFICIENT;					\
                        		v+=1.0/(TEMPERATURENOMINAL+273.15);	\
                        		v=1.0/v;							\
                        		v-=273.15;}

#endif
