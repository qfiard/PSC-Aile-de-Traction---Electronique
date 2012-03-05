//
//  Capteurs.c
//  PSC
//
//  Created by Quentin Fiard on 26/02/12.
//  Copyright (c) 2011 École Polytechnique. All rights reserved.
//

#include <spi.h>
#include "./Capteurs.h"

#define OCF (byte2 & 0x08 != 0)
#define COF (byte2 & 0x04 != 0)
#define LIN (byte2 & 0x02 != 0)
#define MAGINC (byte2 & 0x01 != 0)
#define MAGDEC (byte3 & 0x80 != 0)
#define PARITY ((byte3 >> 6) & 0x01)

#define SENSOR_PORT0 LATAbits.LATA0
#define SENSOR_PORT1 LATAbits.LATA1
#define SENSOR_PORT2 LATAbits.LATA2
#define SENSOR_PORT3 LATAbits.LATA3
#define SENSOR_PORT4 LATAbits.LATA4
#define SENSOR_PORT5 LATAbits.LATA5

static volatile BOOL isSensorReadReady = FALSE;

void prepareForSensorRead(void)
{
    OpenSPI(SPI_FOSC_64, MODE_10, SMPEND);

    //Initialisation des ports de sélection des capteurs
    LATA |= 0x3F; // Waiting state is high
    ADCON1 |= 0x0F;
    TRISA &= 0xC0; // Ports en sortie

    isSensorReadReady = TRUE;
}

static void selectSensor(Sensor sensor)
{
    LATA |= 0x3F; // Waiting state is high

    LATA -= (0x01 << sensor);
}

SensorStatus getStatusOfSensor(Sensor sensor)
{
    UINT8 byte1,byte2,byte3,test;
    SensorStatus res;

    if(!isSensorReadReady)
    {
        prepareForSensorRead();
    }

    selectSensor(sensor);

    // Waiting 500 ns for sensor to be ready (Period = 83 ns => about 7 cycles)

    Nop();
    Nop();
    Nop();
    Nop();
    Nop();
    Nop();

    byte1 = ReadSPI();
    byte2 = ReadSPI();
    byte3 = ReadSPI();

    LATA |= 0x3F; // Return to waiting state

    res.position = byte1;
    res.position *= 0x10;
    res.position += (byte2 >> 4);

    res.sensor = sensor;

    test = byte2 & 0x08;

    res.error = 0;

    if(OCF)
    {
        res.error += SENSOR_ERROR_OCF_NOT_FINISHED;
    }
    if(COF)
    {
        res.error += SENSOR_ERROR_OVERFLOW;
    }
    if(LIN)
    {
        res.error += SENSOR_ERROR_LINEARITY;
    }
    if(MAGDEC)
    {
        res.error += SENSOR_ERROR_MAG_DEC;
    }
    if(MAGINC)
    {
        res.error += SENSOR_ERROR_MAG_INC;
    }

    if(!checkParity(res,PARITY))
    {
        res.error += SENSOR_ERROR_PARITY;
    }

    return res;
}

BOOL checkParity(SensorStatus status, BIT parity)
{
    UINT8 sum = 0;
    UINT8 tmp = status.position >> 8;
    UINT8 currentBit;

    sum += tmp & 0x01;
    sum += (tmp >> 1) & 0x01;

    tmp = status.position;

    for(currentBit=0 ; currentBit<8 ; currentBit++)
    {
        sum += (tmp >> currentBit) & 0x01;
    }

    tmp = status.error;

    if(tmp & SENSOR_ERROR_OCF_NOT_FINISHED == 0)
    {
        sum++;
    }
    if(tmp & SENSOR_ERROR_OVERFLOW != 0)
    {
        sum++;
    }
    if(tmp & SENSOR_ERROR_LINEARITY != 0)
    {
        sum++;
    }
    if(tmp & SENSOR_ERROR_MAG_DEC != 0)
    {
        sum++;
    }
    if(tmp & SENSOR_ERROR_MAG_INC != 0)
    {
        sum++;
    }

    return ((sum & 0x01) == parity);
}