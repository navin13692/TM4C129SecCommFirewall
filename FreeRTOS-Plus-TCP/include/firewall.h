/*
 * firewall.h
 *
 *  Created on: 22-Feb-2017
 *      Author: navin
 */

#ifndef __FIREWALL_H_
#define __FIREWALL_H_

#include <stdint.h>

#define firewallMAX_RULES 10
#define firewallMAX_RULE_LENGTH 64  //in Bytes

typedef struct firewall_rule {
    //public
    void* rule;
    //private
    struct firewall_rule* next_rule_struct; //its private variable so user should not use this
} firewall_rule_struct;


typedef struct firewall_group{
    //public
    uint8_t* group_name;
    //private
    firewall_rule_struct* first_rule;
    struct firewall_group* next_group_struct;
}firewall_group_struct;


uint32_t u32FirewallAddRule(firewall_rule_struct* rule_struct,uint8_t* group);
uint32_t u32FirewallAddGroup(firewall_group_struct* group);
uint32_t u32FirewallRemoveRule(firewall_rule_struct* rule_struct);
uint32_t u32FirewallRemoveGroup(firewall_group_struct* group);
uint32_t u32FirewallRemoveGWname(uint8_t* group); //Remove Group with name
uint32_t u32FirewallCheckRule(uint8_t* data,uint8_t* group);


#endif /* FREERTOS_PLUS_TCP_INCLUDE_FIREWALL_H_ */
//
// Usage Example



