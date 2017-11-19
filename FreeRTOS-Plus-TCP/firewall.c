
/*
 * firewall.c
 *
 *  Created on: 22-Feb-2017
 *      Author: navin
 */

#include "string.h"
#include "firewall.h"

firewall_group_struct firewallGroupHead = {0, 0, 0} ; //head entry to trace all group and rules
                                                      //make sure head of groups is not uninitialized

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t u32FirewallAddRule(firewall_rule_struct* rule_struct, uint8_t* group)
{
    uint32_t xReturn=0;
    firewall_group_struct* xGroup = &firewallGroupHead;
    firewall_rule_struct* xRule;
    do{
        if(xGroup->next_group_struct==0)
            return xReturn;
        xGroup = xGroup->next_group_struct;
    }while(strcmp(xGroup->group_name,group));

    //strcmp(xGroup->group_name,group)
    if(xGroup->first_rule == 0)
    {
        xGroup->first_rule = rule_struct;
        xGroup->first_rule->next_rule_struct = 0;
        xReturn = 1;
        return xReturn;
    }

    xRule = xGroup->first_rule;
    while(xRule->next_rule_struct != 0)
    {
         xRule = xRule->next_rule_struct;
    }
    xRule->next_rule_struct = rule_struct;
    rule_struct->next_rule_struct = 0;
    xReturn = 1;
    return xReturn;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t u32FirewallAddGroup(firewall_group_struct* group)
{
    uint32_t xReturn = 0;
    firewall_group_struct* xGroup = &firewallGroupHead;
    while(xGroup->next_group_struct != 0)
    {
        xGroup = xGroup->next_group_struct;
        if(!strcmp(xGroup->group_name, group->group_name))
            return xReturn;
    }
    group->first_rule = 0;
    group->next_group_struct = 0;
    xGroup->next_group_struct = group;
    xReturn = 1;
    return xReturn;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t u32FirewallRemoveRule(firewall_rule_struct* rule_struct)
{
    uint32_t xReturn=0;
    firewall_group_struct* xGroup = &firewallGroupHead;
    firewall_rule_struct* xRule;
    while(xGroup->next_group_struct != 0)
    {
        xGroup = xGroup->next_group_struct;
        xRule = xGroup->first_rule;
        if(xRule == rule_struct)
        {
            xGroup->first_rule = xRule->next_rule_struct;
            xReturn = 1;
            return xReturn;
        }
        if(xRule == 0)
            continue;
        while(xRule->next_rule_struct != 0)
        {
            if(xRule->next_rule_struct == rule_struct)
            {
                xRule->next_rule_struct = rule_struct->next_rule_struct;
                xReturn = 1;
                return xReturn;
            }
            xRule = xRule->next_rule_struct;
        }
    }
    return xReturn;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t u32FirewallRemoveGroup(firewall_group_struct* group)
{
    uint32_t xReturn=0;
    firewall_group_struct* xGroup = &firewallGroupHead;
    while(xGroup->next_group_struct != 0)
    {
        if(xGroup->next_group_struct == group)
        {
            xGroup->next_group_struct = group->next_group_struct;
            xReturn = 1;
            return xReturn;
        }
        xGroup = xGroup->next_group_struct;
    }
    return xReturn;
}
uint32_t u32FirewallRemoveGWname(uint8_t* group)
{
    uint32_t xReturn=0;
    firewall_group_struct* xGroup = &firewallGroupHead;
    while(xGroup->next_group_struct != 0)
    {
        if(!strcmp(xGroup->next_group_struct->group_name,group))
        {
            xGroup->next_group_struct = xGroup->next_group_struct->next_group_struct;
            xReturn = 1;
            return xReturn;
        }
        xGroup = xGroup->next_group_struct;
    }
    return xReturn;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//uint32_t u32FirewallCheckRule(uint8_t* data,uint8_t* group)
//{
//    uint32_t xReturn=0,xResult;
//    firewall_group_struct* xGroup = &firewallGroupHead;
//    firewall_rule_struct* xRule;
//    do{
//        if(xGroup->next_group_struct==0)
//            return xReturn;
//        xGroup = xGroup->next_group_struct;
//    }while(strcmp(xGroup->group_name,group));
//
//    xRule = xGroup->first_rule;
//    while(xRule)
//    {
//        xResult = strncmp(xRule->rule, data + xRule->startfrom, xRule->length);
//        if(xResult == 0)
//        {
//            xReturn=1;
//            return xReturn;
//        }
//    }
//    return xReturn;
//}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t u32FirewallCheckRule(uint8_t* data,uint8_t* group)
{
    uint32_t xReturn=0,xResult;
    firewall_group_struct* xGroup = &firewallGroupHead;
    firewall_rule_struct* xRule;
    do{
        if(xGroup->next_group_struct==0)
            return xReturn;
        xGroup = xGroup->next_group_struct;
    }while(strcmp(xGroup->group_name,group));

    xRule = xGroup->first_rule;
    while(xRule)
    {
        xResult = ((uint32_t(*)(uint8_t*))xRule->rule)(data);
        if(xResult)
        {
            xReturn=1;
            return xReturn;
        }
        xRule = xRule->next_rule_struct;
    }
    return xReturn;
}

