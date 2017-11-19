
#ifndef SECSOCK_SECSOCK_H_
#define SECSOCK_SECSOCK_H_

#include <stdlib.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

// Status[8bit] definitions macro
#define ENCPTD   0  // To pack encrypted message
#define REQREG   1  // registration request sent, used by entities
#define ACPTREG  2  // registration request accepted, used by auth servers
#define RJCTREG  3  // registration request rejected, used by auth servers
#define ACKAUTH  4  // acknowledge to auth server
#define REQACC   5  // request to access entity and session key gen, used by entities
#define ACPTACC  6  // access request accepted, used by auth servers
#define RJCTACC  7  // access request rejected, used by auth servers
#define REQCOMM  8  // communication request to entity by entity
#define RESPCOMM 9  // Communication response from entity to entity
#define ACKACC  10  // Ack for entity getting accessed
#define NONCE  11

#define SEP ':'

#define ENTITY_NAME "tm4cBulb"
#define GROUP_NAME "Public"
#define MAX_ENTITY_NAME 50
#define MAX_TABLES 10
#define BUFF_SIZE 1024
#define ENTITY_IS_SERVER 1
#define ENTITY_IS_CLIENT 0
#define ENTITY_PORT 2222
//#define AUTH_IP {10,114,56,169}
#define AUTH_IP {192,168,0,104}
#define AUTH_PORT 5555

typedef struct{
    uint8_t entintyName[MAX_ENTITY_NAME];
    uint8_t key[16];//128 bit
    uint32_t time;
    uint8_t ip[4];
    uint16_t port;
}secsockAccessTable;


//secsock function
bool reqRegistration(void* xSocket);

#if ENTITY_IS_SERVER
uint32_t secsock_listen(void* xSocket,uint8_t *xEntityName, uint8_t* msg, uint32_t msgMaxLength);
bool sendResp(void* xSocket, uint8_t* xEntityName, uint8_t* msg, uint32_t msgLength);
#endif

#if ENTITY_IS_CLIENT
bool reqAccess(void* xSocket, uint8_t* xEntityName, uint32_t acctime);
bool sendMsg(void* xSocket, uint8_t* xEntityName, uint8_t* msg, uint32_t msgLength);
uint32_t recvMsg(void* xSocket, uint8_t* xEntityName, uint8_t* msg, uint32_t msgMaxLength);
#endif

uint32_t secsock_encrypt(uint8_t *data, uint32_t legth, uint8_t *key);
bool secsock_decrypt(uint8_t *data, uint32_t length, uint8_t *key);
bool secsock_rsa_encrypt_128(uint8_t res[], uint8_t data[], uint8_t length,uint8_t key[]);
bool secsock_rsa_decrypt_128(uint8_t res[], uint8_t data[]);
#endif /* SECSOCK_SECSOCK_H_ */
