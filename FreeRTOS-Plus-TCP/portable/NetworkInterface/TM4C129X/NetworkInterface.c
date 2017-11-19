/*
 * Some constants, hardware definitions and comments taken from ST's HAL driver
 * library, COPYRIGHT(c) 2015 STMicroelectronics.
 */

/*
 * FreeRTOS+TCP Labs Build 160919 (C) 2016 Real Time Engineers ltd.
 * Authors include Hein Tibosch and Richard Barry
 *
 *******************************************************************************
 ***** NOTE ******* NOTE ******* NOTE ******* NOTE ******* NOTE ******* NOTE ***
 ***                                                                         ***
 ***                                                                         ***
 ***   FREERTOS+TCP IS STILL IN THE LAB (mainly because the FTP and HTTP     ***
 ***   demos have a dependency on FreeRTOS+FAT, which is only in the Labs    ***
 ***   download):                                                            ***
 ***                                                                         ***
 ***   FreeRTOS+TCP is functional and has been used in commercial products   ***
 ***   for some time.  Be aware however that we are still refining its       ***
 ***   design, the source code does not yet quite conform to the strict      ***
 ***   coding and style standards mandated by Real Time Engineers ltd., and  ***
 ***   the documentation and testing is not necessarily complete.            ***
 ***                                                                         ***
 ***   PLEASE REPORT EXPERIENCES USING THE SUPPORT RESOURCES FOUND ON THE    ***
 ***   URL: http://www.FreeRTOS.org/contact  Active early adopters may, at   ***
 ***   the sole discretion of Real Time Engineers Ltd., be offered versions  ***
 ***   under a license other than that described below.                      ***
 ***                                                                         ***
 ***                                                                         ***
 ***** NOTE ******* NOTE ******* NOTE ******* NOTE ******* NOTE ******* NOTE ***
 *******************************************************************************
 *
 * FreeRTOS+TCP can be used under two different free open source licenses.  The
 * license that applies is dependent on the processor on which FreeRTOS+TCP is
 * executed, as follows:
 *
 * If FreeRTOS+TCP is executed on one of the processors listed under the Special
 * License Arrangements heading of the FreeRTOS+TCP license information web
 * page, then it can be used under the terms of the FreeRTOS Open Source
 * License.  If FreeRTOS+TCP is used on any other processor, then it can be used
 * under the terms of the GNU General Public License V2.  Links to the relevant
 * licenses follow:
 *
 * The FreeRTOS+TCP License Information Page: http://www.FreeRTOS.org/tcp_license
 * The FreeRTOS Open Source License: http://www.FreeRTOS.org/license
 * The GNU General Public License Version 2: http://www.FreeRTOS.org/gpl-2.0.txt
 *
 * FreeRTOS+TCP is distributed in the hope that it will be useful.  You cannot
 * use FreeRTOS+TCP unless you agree that you use the software 'as is'.
 * FreeRTOS+TCP is provided WITHOUT ANY WARRANTY; without even the implied
 * warranties of NON-INFRINGEMENT, MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. Real Time Engineers Ltd. disclaims all conditions and terms, be they
 * implied, expressed, or statutory.
 *
 * 1 tab == 4 spaces!
 *
 * http://www.FreeRTOS.org
 * http://www.FreeRTOS.org/plus
 * http://www.FreeRTOS.org/labs
 *
 */

/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>


/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP_Private.h"
#include "NetworkBufferManagement.h"
#include "NetworkInterface.h"
#include "FreeRTOSIPConfig.h"
/* TI includes. */
#include "inc/hw_emac.h"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/emac.h"
#include "driverlib/sysctl.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/debug.h"
#include "tm4c129x.h"

#include "firewall.h"

extern uint32_t g_ui32SysClock;

/********************Buffer related costants******************************/
#define NUM_TX_DESCRIPTORS 3
#define NUM_RX_DESCRIPTORS 3

#define RX_BUFFER_SIZE 1500
#define TX_BUFFER_SIZE 1500
//*****************************************************************************
//
// A set of flags.  The flag bits are defined as follows:
//
//     1 -> An RX Packet has been received.
//     2 -> A TX packet DMA transfer is pending.
//     3 -> An RX packet DMA transfer is pending.
//
//*****************************************************************************
volatile uint32_t g_ui32Flags;
//*****************************************************************************
//
// Ethernet DMA descriptors.
//
// the MAC hardware needs a minimum of
// 3 receive descriptors to operate.
//
//*****************************************************************************

tEMACDMADescriptor g_psRxDescriptor[NUM_RX_DESCRIPTORS];
tEMACDMADescriptor g_psTxDescriptor[NUM_TX_DESCRIPTORS];
//*****************************************************************************
//
// Transmit and receive buffers.
//
//*****************************************************************************

uint8_t g_pui8RxBuffer[RX_BUFFER_SIZE];
uint8_t g_pui8TxBuffer[TX_BUFFER_SIZE];
uint32_t g_ui32RxDescIndex;
uint32_t g_ui32TxDescIndex;

#define BUFFER_SIZE ( ipTOTAL_ETHERNET_FRAME_SIZE + ipBUFFER_PADDING )
#define BUFFER_SIZE_ROUNDED_UP ( ( BUFFER_SIZE + 7 ) & ~0x07UL )
static uint8_t ucBuffers[ ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS ][ BUFFER_SIZE_ROUNDED_UP ];

/* Next provide the vNetworkInterfaceAllocateRAMToBuffers() function, which
simply fills in the pucEthernetBuffer member of each descriptor. */
void vNetworkInterfaceAllocateRAMToBuffers(
    NetworkBufferDescriptor_t pxNetworkBuffers[ ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS ] )
{
BaseType_t x;

    for( x = 0; x < ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS; x++ )
    {
        /* pucEthernetBuffer is set to point ipBUFFER_PADDING bytes in from the
        beginning of the allocated buffer. */
        pxNetworkBuffers[ x ].pucEthernetBuffer = &( ucBuffers[ x ][ ipBUFFER_PADDING ] );

        /* The following line is also required, but will not be required in
        future versions. */
        *( ( uint32_t * ) &ucBuffers[ x ][ 0 ] ) = ( uint32_t ) &( pxNetworkBuffers[ x ] );
    }
}


//*****************************************************************************
//
// Initialize the transmit and receive DMA descriptors.
//
//*****************************************************************************
void InitDescriptors(uint32_t ui32Base)
{
    uint32_t ui32Loop;

    //
    // Initialize each of the transmit descriptors.  Note that we leave the OWN
    // bit clear here since we have not set up any transmissions yet.
    //
    for(ui32Loop = 0; ui32Loop < NUM_TX_DESCRIPTORS; ui32Loop++)
    {
        g_psTxDescriptor[ui32Loop].ui32Count =
            (DES1_TX_CTRL_SADDR_INSERT |
             (TX_BUFFER_SIZE << DES1_TX_CTRL_BUFF1_SIZE_S));
        g_psTxDescriptor[ui32Loop].pvBuffer1 = g_pui8TxBuffer;
        g_psTxDescriptor[ui32Loop].DES3.pLink =
            (ui32Loop == (NUM_TX_DESCRIPTORS - 1)) ?
            g_psTxDescriptor : &g_psTxDescriptor[ui32Loop + 1];
        g_psTxDescriptor[ui32Loop].ui32CtrlStatus =
            (DES0_TX_CTRL_LAST_SEG | DES0_TX_CTRL_FIRST_SEG |
             DES0_TX_CTRL_INTERRUPT| DES0_TX_CTRL_CHAINED   |
             DES0_TX_CTRL_IP_ALL_CKHSUMS
             );
    }

    //
    // Initialize each of the receive descriptors.  We clear the OWN bit here
    // to make sure that the receiver doesn't start writing anything
    // immediately.
    //
    for(ui32Loop = 0; ui32Loop < NUM_RX_DESCRIPTORS; ui32Loop++)
    {
        g_psRxDescriptor[ui32Loop].ui32CtrlStatus = 0;
        g_psRxDescriptor[ui32Loop].ui32Count =
            (DES1_RX_CTRL_CHAINED |
             (RX_BUFFER_SIZE << DES1_RX_CTRL_BUFF1_SIZE_S));
        g_psRxDescriptor[ui32Loop].pvBuffer1 = g_pui8RxBuffer;
        g_psRxDescriptor[ui32Loop].DES3.pLink =
            (ui32Loop == (NUM_RX_DESCRIPTORS - 1)) ?
            g_psRxDescriptor : &g_psRxDescriptor[ui32Loop + 1];
    }

    //
    // Set the descriptor pointers in the hardware.
    //
    MAP_EMACRxDMADescriptorListSet(ui32Base, g_psRxDescriptor);
    MAP_EMACTxDMADescriptorListSet(ui32Base, g_psTxDescriptor);

    //
    // Start from the beginning of both descriptor chains.  We actually set
    // the transmit descriptor index to the last descriptor in the chain
    // since it will be incremented before use and this means the first
    // transmission we perform will use the correct descriptor.
    //
    g_ui32RxDescIndex = 0;
    g_ui32TxDescIndex = NUM_TX_DESCRIPTORS - 1;
}
//*****************************************************************************
//
// Read a packet from the DMA receive buffer into the uIP packet buffer.
//
//*****************************************************************************
int32_t
PacketReceive(uint32_t ui32Base, uint8_t *pui8Buf, int32_t i32BufLen)
{
    int_fast32_t  i32Loop,i32FrameLen;

    //
    // Check the arguments.
    //
    ASSERT(ui32Base == EMAC0_BASE);
    ASSERT(pui8Buf != 0);
    ASSERT(i32BufLen > 0);

    //
    // By default, we assume we got a bad frame.
    //
    i32FrameLen = 0;

    //
    // Make sure that we own the receive descriptor.
    //
    if(!(g_psRxDescriptor[g_ui32RxDescIndex].ui32CtrlStatus & DES0_RX_CTRL_OWN))
    {
        //
        // We own the receive descriptor so check to see if it contains a valid
        // frame.  Look for a descriptor error, indicating that the incoming
        // packet was truncated or, if this is the last frame in a packet,
        // the receive error bit.
        //
        if(!(g_psRxDescriptor[g_ui32RxDescIndex].ui32CtrlStatus &
             DES0_RX_STAT_ERR))
        {
            //
            // We have a valid frame so copy the content to the supplied
            // buffer. First check that the "last descriptor" flag is set.  We
            // sized the receive buffer such that it can always hold a valid
            // frame so this flag should never be clear at this point but...
            //
            if(g_psRxDescriptor[g_ui32RxDescIndex].ui32CtrlStatus &
               DES0_RX_STAT_LAST_DESC)
            {
                i32FrameLen =
                    ((g_psRxDescriptor[g_ui32RxDescIndex].ui32CtrlStatus &
                      DES0_RX_STAT_FRAME_LENGTH_M) >>
                     DES0_RX_STAT_FRAME_LENGTH_S);

                //
                // Sanity check.  This shouldn't be required since we sized the
                // uIP buffer such that it's the same size as the DMA receive
                // buffer but, just in case...
                //
                if(i32FrameLen > i32BufLen)
                {
                    i32FrameLen = i32BufLen;
                }

                //
                // Copy the data from the DMA receive buffer into the provided
                // frame buffer.
                //
                for( i32Loop= 0; i32Loop < i32FrameLen;)
                {
                    pui8Buf[i32Loop]=g_pui8RxBuffer[i32Loop];
                    i32Loop++;
                }
            }
        }

        //
        // Move on to the next descriptor in the chain.
        //
        g_ui32RxDescIndex++;
        if(g_ui32RxDescIndex == NUM_RX_DESCRIPTORS)
        {
            g_ui32RxDescIndex = 0;
        }

        //
        // Mark the next descriptor in the ring as available for the receiver
        // to write into.
        //
        g_psRxDescriptor[g_ui32RxDescIndex].ui32CtrlStatus = DES0_RX_CTRL_OWN;
    }

    //
    // Return the Frame Length
    //
    HWREGBITW(&g_ui32Flags, 2) = 0;
    return(i32FrameLen);
}

//*****************************************************************************
//
// The interrupt handler for the Ethernet interrupt.
//
//*****************************************************************************
void
EthernetIntHandler(void)
{
    uint32_t ui32Temp;

    //
    // Read and Clear the interrupt.
    //
    ui32Temp = MAP_EMACIntStatus(EMAC0_BASE, true);
    MAP_EMACIntClear(EMAC0_BASE, ui32Temp);

    //
    // Check to see if an RX Interrupt has occurred.
    //
    if(ui32Temp & EMAC_INT_RECEIVE)
    {
        //
        // Indicate that a packet has been received.
        //
        HWREGBITW(&g_ui32Flags, 2) = 1;
        /////////////////////////////////////
        NetworkBufferDescriptor_t* rvPacket;



        rvPacket=pxNetworkBufferGetFromISR(BUFFER_SIZE);
        //rvPacket=(NetworkBufferDescriptor_t*)(&g_pui8RxBuffer[nwBuffer][0]);
        if(rvPacket != NULL)
        {
            static IPStackEvent_t rvEvent;
            PacketReceive(EMAC0_BASE, (uint8_t *)rvPacket->pucEthernetBuffer, BUFFER_SIZE);

            if(u32FirewallCheckRule(rvPacket->pucEthernetBuffer, "ethernet"))
            {
                rvEvent.eEventType = eNetworkRxEvent;
                rvEvent.pvData = (void *) rvPacket;
                if(xSendEventStructToIPTask(&rvEvent,0)==0)
                    FreeRTOS_debug_printf("Unable to send packet to list\n");
            }
            else
            {
                vReleaseNetworkBufferAndDescriptor(rvPacket);
            }
        }
        else
        {
            FreeRTOS_debug_printf("Unable to aquire Buffer\n");
        }
    }

    //
    // Has the DMA finished transferring a packet to the transmitter?
    //
    if(ui32Temp & EMAC_INT_TRANSMIT)
    {
        //
        // Indicate that a packet has been sent.
        //
        HWREGBITW(&g_ui32Flags, 1) = 0;
        static uint32_t ui32Count=0;
        FreeRTOS_debug_printf("Tx %d packets\n",ui32Count++);
    }

}

//*****************************************************************************
//
// Ethernet initialization.
//
//*****************************************************************************
BaseType_t xNetworkInterfaceInitialise( void )
{
    uint8_t macid[6];
    uint32_t ui32User0, ui32User1;
    uint32_t ui32PHYConfig;
    static BaseType_t xReturn = pdFAIL;     //If some error occurs in initialitation xReturn should return "pdFAIL", "pdPASS" otherwise

    if(xReturn == pdTRUE)
            return xReturn;
    ui32PHYConfig = (EMAC_PHY_TYPE_INTERNAL | EMAC_PHY_INT_MDIX_EN |
                         EMAC_PHY_AN_100B_T_FULL_DUPLEX);

        //
        // Read the MAC address from the user registers.
        //
        MAP_FlashUserGet(&ui32User0, &ui32User1);
        if((ui32User0 == 0xffffffff) || (ui32User1 == 0xffffffff))
        {
            //
            // We should never get here.  This is an error if the MAC address has
            // not been programmed into the device.  Exit the program.
            //
            //UARTprintf("MAC Address Not Programmed!");
            //xReturn = pdFAIL;
            return xReturn;
        }
        macid[0] = ((ui32User0 >> 0) & 0xff);
        macid[1] = ((ui32User0 >> 8) & 0xff);
        macid[2] = ((ui32User0 >> 16) & 0xff);
        macid[3] = ((ui32User1 >> 0) & 0xff);
        macid[4] = ((ui32User1 >> 8) & 0xff);
        macid[5] = ((ui32User1 >> 16) & 0xff);



        //
        // Enable and reset the Ethernet modules.
        //
        MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_EMAC0);
        MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_EPHY0);
        MAP_SysCtlPeripheralReset(SYSCTL_PERIPH_EMAC0);
        MAP_SysCtlPeripheralReset(SYSCTL_PERIPH_EPHY0);

        //
        // Wait for the MAC to be ready.
        //
        //UARTprintf("Waiting for MAC to be ready...");
        while(!MAP_SysCtlPeripheralReady(SYSCTL_PERIPH_EMAC0))
        {
        }

        //
        // Configure for use with the internal PHY.
        //
        MAP_EMACPHYConfigSet(EMAC0_BASE, ui32PHYConfig);
        //UARTprintf("MAC ready.");

        //
        // Reset the MAC.
        //
        MAP_EMACReset(EMAC0_BASE);

        //
        // Initialize the MAC and set the DMA mode.
        //
        MAP_EMACInit(EMAC0_BASE, g_ui32SysClock,
                     EMAC_BCONFIG_MIXED_BURST | EMAC_BCONFIG_PRIORITY_FIXED, 4, 4,
                     0);

        //
        // Set MAC configuration options.
        //
        MAP_EMACConfigSet(EMAC0_BASE,
                          (EMAC_CONFIG_FULL_DUPLEX | EMAC_CONFIG_CHECKSUM_OFFLOAD |
                           EMAC_CONFIG_7BYTE_PREAMBLE | EMAC_CONFIG_IF_GAP_96BITS |
                           EMAC_CONFIG_USE_MACADDR0 |
                           EMAC_CONFIG_SA_FROM_DESCRIPTOR |
                           EMAC_CONFIG_BO_LIMIT_1024),
                          (EMAC_MODE_RX_STORE_FORWARD |
                           EMAC_MODE_TX_STORE_FORWARD |
                           EMAC_MODE_TX_THRESHOLD_64_BYTES |
                           EMAC_MODE_RX_THRESHOLD_64_BYTES), 0);

        //
        // Initialize the Ethernet DMA descriptors.
        //
        InitDescriptors(EMAC0_BASE);

        //
        // Program the hardware with its MAC address (for filtering).
        //
        EMACAddrSet(EMAC0_BASE, 0,(uint8_t*) macid);

        //
        // Wait for the link to become active.
        //
        //UARTprintf("Waiting for Link.");
        while((MAP_EMACPHYRead(EMAC0_BASE, 0, EPHY_BMSR) &
               EPHY_BMSR_LINKSTAT) == 0)
        {
        }

        //UARTprintf("Link Established.");

        //
        // Set MAC filtering options.  We receive all broadcast and multicast
        // packets along with those addressed specifically for us.
        //
        MAP_EMACFrameFilterSet(EMAC0_BASE, (EMAC_FRMFILTER_SADDR |
                                            EMAC_FRMFILTER_PASS_MULTICAST |
                                            EMAC_FRMFILTER_PASS_NO_CTRL));

        //
        // Clear any pending interrupts.
        //
        MAP_EMACIntClear(EMAC0_BASE, EMACIntStatus(EMAC0_BASE, false));

        //
        // Enable the Ethernet MAC transmitter and receiver.
        //
        MAP_EMACTxEnable(EMAC0_BASE);
        MAP_EMACRxEnable(EMAC0_BASE);

        //
        // Enable the Ethernet interrupt.
        //
        MAP_IntEnable(INT_EMAC0);

        //
        // Enable the Ethernet RX Packet interrupt source.
        //
        MAP_EMACIntEnable(EMAC0_BASE, EMAC_INT_RECEIVE);

        //
        // Mark the first receive descriptor as available to the DMA to start
        // the receive processing.
        //
        g_psRxDescriptor[g_ui32RxDescIndex].ui32CtrlStatus |= DES0_RX_CTRL_OWN;

        //initialize flags to zero
        g_ui32Flags=0;
        xReturn = pdTRUE;
        return xReturn;
}
//*****************************************************************************
//
//     Sends data received from the embedded TCP/IP stack to the
//     Ethernet MAC driver for transmission.
//
//*****************************************************************************
BaseType_t xNetworkInterfaceOutput( xNetworkBufferDescriptor_t * const pxDescriptor, BaseType_t bReleaseAfterSend )
{
    //uint32_t ui32Base=EMAC0_BASE;
    uint8_t *pui8Buf = pxDescriptor->pucEthernetBuffer;
    int32_t i32BufLen = pxDescriptor->xDataLength; //BUFFER_SIZE_ROUNDED_UP;//
    int_fast32_t i32Loop;
    tEMACDMADescriptor *currentMACDMADescriptor;

    //
    // Check that we're not going to overflow the transmit buffer.
    //
    if(i32BufLen==0)
        i32BufLen=BUFFER_SIZE_ROUNDED_UP;
    if(i32BufLen > TX_BUFFER_SIZE)
    {
        return pdFAIL;
    }
    //
    // Move to the next descriptor.
    //
    currentMACDMADescriptor = MAP_EMACTxDMACurrentDescriptorGet(EMAC0_BASE);
    while(currentMACDMADescriptor->ui32CtrlStatus &  \
             DES0_TX_CTRL_OWN)    \
       {
           //
           // Spin and waste time.
           //
       }
    //
    // Indicate that a packet is being sent.
    //
    HWREGBITW(&g_ui32Flags, 1) = 1;



    // Copy the packet data into the transmit buffer.
    //
    currentMACDMADescriptor->pvBuffer1=pui8Buf;
    //g_psTxDescriptor[g_ui32TxDescIndex].pvBuffer1=pui8Buf;
    //for(i32Loop = 0; i32Loop < i32BufLen; i32Loop++)
    //{
    //    g_pui8TxBuffer[i32Loop] = pui8Buf[i32Loop];
    //}




    //
    // Fill in the packet size and tell the transmitter to start work.
    //
    //g_psTxDescriptor[g_ui32TxDescIndex].ui32Count = (uint32_t)i32BufLen;
    //g_psTxDescriptor[g_ui32TxDescIndex].ui32CtrlStatus =
    currentMACDMADescriptor->ui32Count = (uint32_t)i32BufLen;
    currentMACDMADescriptor->ui32CtrlStatus =
        (DES0_TX_CTRL_LAST_SEG | DES0_TX_CTRL_FIRST_SEG |
         DES0_TX_CTRL_INTERRUPT  |DES0_TX_CTRL_IP_ALL_CKHSUMS  |
         DES0_TX_CTRL_CHAINED | DES0_TX_CTRL_OWN);
        //
        // Indicate that a packet is being sent.
        //
        HWREGBITW(&g_ui32Flags, 1) = 1;
    //
    // Tell the DMA to reacquire the descriptor now that we've filled it in.
    //
    MAP_EMACTxDMAPollDemand(EMAC0_BASE);

    //Uncomment below line for blocking transmit
    //while(HWREGBITW(&g_ui32Flags, 1) == 1);


    //Release network buffer
    if( bReleaseAfterSend  != pdFALSE  )
    {
            vReleaseNetworkBufferAndDescriptor( pxDescriptor );
    }

    return pdPASS;

}
//*****************************************************************************


/*
 * typedef struct xNETWORK_BUFFER for referance
{
    ListItem_t xBufferListItem;     /* Used to reference the buffer form the free buffer list or a socket.
    uint32_t ulIPAddress;           /* Source or destination IP address, depending on usage scenario.
    uint8_t *pucEthernetBuffer;     /* Pointer to the start of the Ethernet frame.
    size_t xDataLength;             /* Starts by holding the total Ethernet frame length, then the UDP/TCP payload length.
    uint16_t usPort;                /* Source or destination port, depending on usage scenario.
    uint16_t usBoundPort;           /* The port to which a transmitting socket is bound.
    #if( ipconfigUSE_LINKED_RX_MESSAGES != 0 )
        struct xNETWORK_BUFFER *pxNextBuffer; /* Possible optimisation for expert users - requires network driver support.
    #endif
} NetworkBufferDescriptor_t;
 */
