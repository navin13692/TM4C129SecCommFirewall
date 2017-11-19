//*****************************************************************************
//
// led_task.c - A simple flashing LED task.
//
// Copyright (c) 2012-2015 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
// 
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
// 
// This is part of revision 2.1.2.111 of the EK-TM4C123GXL Firmware Package.
//
//*****************************************************************************

#define SECSOCK_CLIENT_DEMO 0
#define SECSOCK_SERVER_DEMO 1
#define HARD_AES 0
#define SOFT_AES 0
#define RSA_TEST 0

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/rom.h"
#include "drivers/rgb.h"
#include "drivers/buttons.h"
#include "utils/uartstdio.h"
#include "pinout.h"
#include "led_task.h"
#include "priorities.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "NetworkBufferManagement.h"
#include "NetworkInterface.h"

#include "secsock.h"
#include "firewall.h"

//*****************************************************************************
//
// The stack size for the LED toggle task.
//
//*****************************************************************************
#define LEDTASKSTACKSIZE        1500         // Stack size in words

//*****************************************************************************
//
// The item size and queue size for the LED message queue.
//
//*****************************************************************************
#define LED_ITEM_SIZE           sizeof(uint8_t)
#define LED_QUEUE_SIZE          5

//*****************************************************************************
//
// Default LED toggle delay value. LED toggling frequency is twice this number.
//
//*****************************************************************************
#define LED_TOGGLE_DELAY        250

//*****************************************************************************
//
// The queue that holds messages sent to the LED task.
//
//*****************************************************************************
xQueueHandle g_pLEDQueue;

typedef struct
    {
        uint8_t dest_addr_ethr[6];
        uint8_t src_addr_ethr[6];
        uint8_t type[2];

        uint8_t version;
        uint8_t service_type;
        uint8_t total_length[2];

        uint8_t identification[2];
        uint8_t fragment[2];
        uint8_t time_to_live;
        uint8_t protocol;
        uint8_t header_checksum[2];

        uint8_t src_add_ip[4];
        uint8_t dest_add_ip[4];
    }packet;
#include "secsock.h"
#include "secsock_portable.h"
extern uint8_t authIP[];
extern secsockAccessTable from_access_table[];
uint32_t ip_check(uint8_t *data)
{
    int32_t accessCount;
    uint32_t currentTime = secsock_timeins();
    packet * ip_packet = data;
        //if((ip_packet->src_add_ip[0]== 192) && (ip_packet->src_add_ip[1]== 168) && (ip_packet->src_add_ip[2]== 1))
    //if((ip_packet->src_add_ip[0]== authIP[0]) && (ip_packet->src_add_ip[1]== authIP[1]) && (ip_packet->src_add_ip[2]== authIP[2]))

    if((*((uint32_t*)(ip_packet->src_add_ip))) == (*((uint32_t*)authIP)))
    {
                //if(ip_packet->src_add_ip[3]== authIP[3] )
                return 1;
    }
    else
    {
        for(accessCount=0;accessCount<MAX_TABLES;accessCount++)
        {
            if(from_access_table[accessCount].time > currentTime)
            {
                if((*((uint32_t*)(ip_packet->src_add_ip))) == (*((uint32_t*)(from_access_table[accessCount].ip))))
                     return 1;
            }
            else
                from_access_table[accessCount].time = 0;
        }

    }
    return 0;
}

uint32_t arp_check(uint8_t *data)
{
    packet * ip_packet = data;
    if(ip_packet->type[0] == 0x08 && ip_packet->type[1] == 0x06) //pass arp
    {
        return 1;
    }
    return 0;
}
extern uint8_t ucGatewayAddress[];
uint32_t gateway_check(uint8_t *data)
{
    packet * ip_packet = data;

    if((*((uint32_t*)(ip_packet->src_add_ip))) == (*((uint32_t*)ucGatewayAddress)))
    {
                //if(ip_packet->src_add_ip[3]== authIP[3] )
                return 1;
    }

    return 0;
}
extern xSemaphoreHandle g_pUARTSemaphore;

//*****************************************************************************
//
// This task toggles the user selected LED at a user selected frequency. User
// can make the selections by pressing the left and right buttons.
//
//*****************************************************************************
static void
LEDTask(void *pvParameters)
{
    //test
    portTickType ui16LastTime;
        Socket_t xSocket;
        struct freertos_sockaddr xRemoteAddress,xBindAddress;
        UARTprintf("Led blinking.\n");
        ui16LastTime = xTaskGetTickCount();
        //
           // Initialize the GPIOs and Timers that drive the three LEDs.
           //
        /* Set the IP address and port of the remote socket
                              to which this client socket will transmit. */
          xRemoteAddress.sin_port = FreeRTOS_htons(9999);
          xRemoteAddress.sin_addr = FreeRTOS_inet_addr_quick( 192,168,1,1);

          /* Create a socket. */
          xSocket = FreeRTOS_socket(FREERTOS_AF_INET,
                                    FREERTOS_SOCK_DGRAM,/* FREERTOS_SOCK_STREAM for TCP. */
                                    FREERTOS_IPPROTO_UDP );

          configASSERT( xSocket != FREERTOS_INVALID_SOCKET );

          xBindAddress.sin_port = FreeRTOS_htons(ENTITY_PORT);
          if( FreeRTOS_bind( xSocket, &xBindAddress, sizeof( &xBindAddress ) ) != 0 )
          {
              while(1); //Could not bind, should not happen
          }

          //Firewall
          firewall_group_struct Network_firewall_group;
          Network_firewall_group.group_name = "ethernet";
          u32FirewallAddGroup(&Network_firewall_group);

          firewall_rule_struct ethernet_rule1;
          ethernet_rule1.rule= (void *) arp_check;
          u32FirewallAddRule(&ethernet_rule1,"ethernet");

          firewall_rule_struct ethernet_rule2;
          ethernet_rule2.rule= (void *) gateway_check;
          u32FirewallAddRule(&ethernet_rule2,"ethernet");

          firewall_rule_struct ethernet_rule3;
          ethernet_rule3.rule= (void *) ip_check;
          u32FirewallAddRule(&ethernet_rule3,"ethernet");




    //
    // Initialize the LED Toggle Delay to default value.
    //

    //
    while(1)
    {
        static unsigned char ulFlags;
        static unsigned long uCount=0;
        static uint32_t ui32Loop=0;
        BaseType_t xLength;

        ulFlags ^= 1;
        if(uCount>50)
        {
            if(ulFlags)
            {
                //GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, GPIO_PIN_0);
            }
            else
            {
                //GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, 0x0);
            }
            uCount=0;

            //////////////////////////////////////////////////////////////////////////////////////////////////////////////
            if(ui32Loop>9)
            {
                ui32Loop=0;
            }
            else
            {
                ui32Loop++;
            }
#if SECSOCK_CLIENT_DEMO
            if(reqRegistration( xSocket))
            {
                if(reqAccess(xSocket, "Entity1", 15))
                {
                    char msg[]="This is a test message sent by 'tm4c1' to 'Entity1'";
                    if(sendMsg(xSocket, "Entity1", msg,strlen(msg)))
                    {
                        uint8_t msg[100];
                        int32_t xSize;
                        xSize=recvMsg(xSocket, "Entity1", msg, 100);
                        if(xSize){
                            UARTprintf("%s",msg);
                            while(1);
                        }
                    }

                }
            }
#elif SECSOCK_SERVER_DEMO
            if(reqRegistration( xSocket))
            {
                uint8_t msgBuffer[50];
                uint8_t xEntityName[50];
                uint32_t xSize;
                uint8_t bulbState[4]={'0','0','0','0'};
                LEDWrite(CLP_D1|CLP_D2|CLP_D3|CLP_D4,0);
                ROM_GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3,0);
                while(1)
                {
                    xSize = secsock_listen(xSocket, xEntityName, msgBuffer, 250);
                    if(xSize>0)
                    {
                        UARTprintf("Got %s from %s\n",msgBuffer,xEntityName);
                        sendResp(xSocket,xEntityName, msgBuffer, 50);
                        if(msgBuffer[0]=='1')
                        {
                            LEDWrite(CLP_D1,CLP_D1);
                            bulbState[0]= '1';
                            ROM_GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_0, GPIO_PIN_0);
                        }
                        else if(msgBuffer[0]=='0')
                        {
                            LEDWrite(CLP_D1,0);
                            bulbState[0]= '0';
                            ROM_GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_0, 0);
                        }

                        if(msgBuffer[1]=='1')
                        {
                            LEDWrite(CLP_D2,CLP_D2);
                            bulbState[1]= '1';
                            ROM_GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_1, GPIO_PIN_1);
                        }
                        else if(msgBuffer[1]=='0')
                        {
                            LEDWrite(CLP_D2,0);
                            bulbState[1]= '0';
                            ROM_GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_1, 0);
                        }

                        if(msgBuffer[2]=='1')
                        {
                            LEDWrite(CLP_D3,CLP_D3);
                            bulbState[2]= '1';
                            ROM_GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_2, GPIO_PIN_2);
                        }
                        else if(msgBuffer[2]=='0')
                        {
                            LEDWrite(CLP_D3,0);
                            bulbState[2]= '0';
                            ROM_GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_2, 0);
                        }

                        if(msgBuffer[3]=='1')
                        {
                            LEDWrite(CLP_D4,CLP_D4);
                            bulbState[3]= '1';
                            ROM_GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_3, GPIO_PIN_3);
                        }
                        else if(msgBuffer[3]=='0')
                        {
                            LEDWrite(CLP_D4,0);
                            bulbState[3]= '0';
                            ROM_GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_3, 0);
                        }
                        sendResp(xSocket,xEntityName, bulbState, 4);

                    }
                }
            }


#elif HARD_AES
            uint8_t msgBuffer[ 128 ];
            uint8_t cphBuffer[ 128 ];
            uint32_t auther[4] = 0x4e4f5d40;
            secure_msg* send_msg = (secure_msg*) msgBuffer;
            secure_msg* recv_msg = (secure_msg*) cphBuffer;
            BaseType_t i32Loop;
            uint32_t key[4] = {0x08833067, 0x948f6a6d, 0x1c736586, 0x92e9fffe};
           //uint32_t key[4] = {0x67308308, 0x6d6a8f94, 0x8665731c, 0xfeffe992};
           //uint32_t key[4] = {0x92e9fffe, 0x1c736586, 0x948f6a6d, 0x08833067};
            tAESGCMVectorstruct dataVector;
           //send_msg->iv[0] = rand();send_msg->iv[1] = rand();send_msg->iv[2] = rand();
            send_msg->iv[0] = 0x000041C6;send_msg->iv[1] = 0x0000167E;send_msg->iv[2] = 0x00002781;
            uint32_t data[] = {0x41424344, 0x41424344, 0x41424344, 0x41424344};
            dataVector.ui32DataLength = 16;
            dataVector.ui32IVLength = 12;
            dataVector.pui32IV = send_msg->iv;
            dataVector.pui32PlainText = data;//(uint32_t*)recv_msg->data;
            dataVector.pui32CipherText = (uint32_t*)send_msg->data;
            dataVector.pui32AuthData = (uint32_t*) auther;
            dataVector.ui32AuthDataLength = 4;
            dataVector.pui32Key =(uint32_t*) key;
            dataVector.pui32Tag = send_msg->tag;
            xLength = 12 /*IV*/ + 16/*tag*/ + dataVector.ui32DataLength;
#elif SOFT_AES
            uint32_t msglen;
            uint8_t msg[ 128 ]={0};
            uint8_t cphBuffer[ 128 ];
            uint8_t key[] = { 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c };
            uint8_t iv[]  = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };
            strcpy(msg,"Navin Shamji Maheshwari\nhello i am navin from iisc.\n i have some msg.");
            msglen =strlen(msg);
            AES128_CBC_encrypt_buffer(msg, msg, msglen, key, iv);
            xLength = (msglen%16)?(msglen+16-msglen%16):(msglen);
            xLength= FreeRTOS_sendto( xSocket, msg, (size_t) xLength , (BaseType_t) 0, &xRemoteAddress, sizeof(xRemoteAddress) );

            if(xLength>0)
            {
                xLength==FreeRTOS_recvfrom(/* The socket data is being received on. */
                                            xSocket,
                                            /* The buffer into which received data will be
                                            copied. */
                                            cphBuffer,
                                            /* The length of the buffer into which data will be
                                            copied. */
                                            128,
                                            /* ulFlags with the FREERTOS_ZERO_COPY bit clear. */
                                            0,
                                            /* Will get set to the source of the received data. */
                                            &xRemoteAddress,
                                            /* Not used but should be set as shown. */
                                            sizeof(xRemoteAddress)
                   );
                UARTprintf(msg);
            }
#elif RSA_TEST
            int32_t i,j;
            uint64_t e[18]={0};
            uint8_t data[150]={0xbc,0xd8,0xb9,0x11,0x5b,0x57,0xc6,0x8f,0x90,0xc2,0xed,0x97,0x62,0x84,0x2e,0x21,0x99,0x4c,0xb0,0x2d,0xe5,0x75,0x9f,0x87,0x38,0x23,0xad,0xa4,0x74,0xdb,0x16,0x5a,0x29,0x39,0xd8,0xad,0x21,0xcb,0x9c,0x7b,0xbc,0x99,0xc2,0x83,0x5e,0x0d,0x7c,0xd6,0xc5,0x29,0xd2,0xd0,0x71,0xf6,0xa5,0x42,0xc9,0xe0,0x5c,0x5c,0xe2,0xa3,0x91,0x9b,0x1a,0x2d,0x60,0x14,0x0b,0x7c,0x0a,0xfd,0x54,0x5f,0xc7,0xc1,0x0c,0xeb,0xe9,0x59,0x23,0x51,0xf0,0x3e,0x95,0x8f,0xcf,0xf6,0x43,0xcc,0x08,0xf4,0x58,0x62,0xcc,0xe9,0x49,0x6a,0x46,0xb6,0x5a,0x72,0xb4,0x0c,0x38,0xf0,0xc0,0x82,0xd7,0x2e,0xf9,0x9e,0x97,0x2d,0xe6,0xee,0xa9,0xb9,0xe0,0xda,0x9d,0xaa,0xe3,0xd1,0x32,0xd9,0xea,0xf9};
            char msg[150]={0};
            strcpy(msg,"hello, this is first rsa encode.");
            /*j=strlen(msg);
            for(i=0;i<64;i++)
            {
                uint8_t temp=msg[i];
                msg[i]=msg[i+3];
                msg[i+3]=temp;
                temp=msg[i+1];
                msg[i+1]=msg[i+2];
                msg[i+2]=temp;
            }*/

            for(i=0;i<64;i++)
            {
                uint8_t temp=data[i];
                data[i]=data[127-i];
                data[127-i]=temp;
            }
            /*for(i=0;i<64;i+=4)
            {
                uint8_t temp=data[i];
                data[i]=data[i+3];
                data[i+3]=temp;
                temp=data[i+1];
                data[i+1]=data[i+2];
                data[i+2]=temp;
            }*/
            for(i=128;i<150;i++)
                data[i]=0;
            e[0] = 0x10001;
            rsa1024(msg,msg,data,data);
            for(i=0;i<64;i++)
            {
                uint8_t temp=msg[i];
                msg[i]=msg[127-i];
                msg[127-i]=temp;
            }

            xLength = 128;
            xLength= FreeRTOS_sendto( xSocket, msg, (size_t) xLength , (BaseType_t) 0, &xRemoteAddress, sizeof(xRemoteAddress) );

            if(xLength>0)
            {
                xLength==FreeRTOS_recvfrom(/* The socket data is being received on. */
                                            xSocket,
                                            /* The buffer into which received data will be
                                            copied. */
                                            msg,
                                            /* The length of the buffer into which data will be
                                            copied. */
                                            128,
                                            /* ulFlags with the FREERTOS_ZERO_COPY bit clear. */
                                            0,
                                            /* Will get set to the source of the received data. */
                                            &xRemoteAddress,
                                            /* Not used but should be set as shown. */
                                            sizeof(xRemoteAddress)
                   );
                UARTprintf(msg);
            }
#endif








        }
        else
            {uCount++;}

        //
                // Wait for the required amount of time to check back.
                //
                vTaskDelayUntil(&ui16LastTime,LED_TOGGLE_DELAY / portTICK_RATE_MS);

    }
}

//*****************************************************************************
//
// Initializes the LED task.
//
//*****************************************************************************
uint32_t
LEDTaskInit(void)
{
    //
    // Initialize the GPIOs and Timers that drive the three LEDs.
    //
    //RGBInit(1);

    //
    //
        // Enable the GPIO port that is used for the on-board LED.
        //
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOK);
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPION);
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

        //
        // Check if the peripheral access is enabled.
        //
        while(!ROM_SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOK))
        {
        }
        while(!ROM_SysCtlPeripheralReady(SYSCTL_PERIPH_GPION))
        {
        }
        while(!ROM_SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOF))
        {
        }

        //
        // Enable the GPIO pin for the LED (PN0).  Set the direction as output, and
        // enable the GPIO pin for digital function.
        //
        ROM_GPIOPinTypeGPIOOutput(GPIO_PORTK_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 |GPIO_PIN_3);
        ROM_GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_0 | GPIO_PIN_1);
        ROM_GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_0 |GPIO_PIN_4);
   // UARTprintf("Led blinking.\n");

    //
    // Create a queue for sending messages to the LED task.
    //
    g_pLEDQueue = xQueueCreate(LED_QUEUE_SIZE, LED_ITEM_SIZE);

    //
    // Create the LED task.
    //
    if(xTaskCreate(LEDTask, (const portCHAR *)"LED", LEDTASKSTACKSIZE, NULL,
                   tskIDLE_PRIORITY + PRIORITY_LED_TASK, NULL) != pdTRUE)
    {
        return(1);
    }

    //
    // Success.
    //
    return(0);
}
