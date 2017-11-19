#ifndef _SECSOCK_PORTABLE_H_
#define _SECSOCK_PORTABLE_H_

#include <stdlib.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

#include "rsa.h"

#define secsock_ip_addr(x3,x2,x1,x0) FreeRTOS_inet_addr_quick(x3,x2,x1,x0)  //convert ip address to 32 bit int
#define secsock_htons(x) FreeRTOS_htons(x) //for 16 bit variable
#define secsock_htonl(x) FreeRTOS_htonl(x) //for 32 bit variable
#define secsock_ntohs(x) FreeRTOS_ntohs(x) //for 16 bit variable
#define secsock_ntohl(x) FreeRTOS_ntohl(x) //for 32 bit variable
#define secsock_timeins() (xTaskGetTickCount()/2200) //get current time in seconds

int32_t secsock_sendto(void* xSocket, uint8_t* data, size_t DataLength, uint8_t* IPaddr, uint16_t* port);
int32_t secsock_recvfrom(void* xSocket, uint8_t* data, size_t DataLength, uint8_t* IPaddr, uint16_t* port);

#endif /* SECSOCK_SECSOCK_PORTABLE_H_ */
