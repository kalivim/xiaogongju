/**
* File Name :AgentManager/AgentCMDProtocol.c
* Author: rootkiter
* E-mail: rootkiter@rootkiter.com
* Created Time : Tue 29 Dec 2015 07:31:43 PM PST
*/

#include "AgentCMDParse.h"
#include "GlobalValue.h"
#include "AgentIDCtrl.h"
#include "AgentJobMsg.h"
#include "PCNodeManager.h"
#include "AgentConnHandle.h"
#include "AgentChildSync.h"
#include "AgentTunnelHandle.h"

//    int cmdType;
//    int cmdID;

#define ON_NEWCONN_ERROR    -1
#define ON_NEWCONN_OK        1
int on_NewConn(pAgent_proto proto,pPCConn conn){
    Printf_DEBUG("new agent connected here");
    if( ON_NEWAGENT_CONNECT_ERROR == 
        on_NewAgent_Connect(proto,conn)){
        return ON_NEWCONN_ERROR;
    }
    return ON_NEWCONN_OK;
}

int on_MyMsgHere(pAgent_proto proto,pPCConn conn){
    switch(proto->cmdID){
    case CMDID_NEWID_ASK:
        Printf_DEBUG("fresh new id and give it back");
        on_AGENT_ID_Ask(proto,conn);
        break;
    case CMDID_NEWID_RPL:
        Printf_DEBUG("get a new id replay how to use it ?");
        JOB_CloseJob(GLOBAL_GetJobList(),
            proto->jobID,
            proto->cmdargs);
    case CMDID_CHILDSYNC_UPPER:
        Printf_DEBUG("sync agent info upper");
        on_SyncInfoUpper(proto);
        break;
    case CMDID_ID_RESET:
        Printf_DEBUG("need replace my id");
        break;
    default:
        Printf_DEBUG("Unsupport now %d",proto->cmdID);
        break;
    }
    return 1;
}

#define ON_TUNNELCTRL_ERROR    -1
#define ON_TUNNELCTRL_OK        1
int on_TunnelCTRL(pAgent_proto proto,pPCConn conn){
    switch(proto->cmdID){
    case CMDID_NEWTUNNEL_RC_ASK:
        on_reverse_Tunnel_Ask(proto,conn);
        break;
    case CMDID_NEWTUNNEL_RC_RPL:
        on_reverse_Reply(proto,conn);
        break;
    case CMDID_NEWTUNNEL_ASK:
        on_new_tunnel_ask(proto,conn);
        break;
    }
    return ON_TUNNELCTRL_OK;
}

int CMDParse_SendProto_Upper(pAgent_proto proto){
    int result = CMDPARSE_SENDPROTO_UPPER_ERROR;
    pPCConn conn     = NULL;
    pPCNodeInfo father = NULL;
    if(PCMANAGER_Get_RootID() == 0){
        // no upper , I'm admin now
        goto ok_exit;
    }
    father = PCMANAGER_Get_FatherNode();
    if(father == PCMANAGER_GET_FATHERNODE_ERROR){
        // get father info error
        goto error_exit;
    }
    conn = &(father->conn);
    if(conn == PCCONN_COPY_ERROR){
        goto error_exit;
    }
    PROTO_SendProto(conn,proto);
    MIC_USLEEP(1);
ok_exit:
    result = CMDPARSE_SENDPROTO_UPPER_OK;
    goto exit;
error_exit:
    result = CMDPARSE_SENDPROTO_UPPER_ERROR;
    goto exit;
exit:
    PCNODE_Free(father);
    conn   = NULL;
    father = NULL;
    return result;
}

#define ON_TRANSMIT_ERROR   -1
#define ON_TRANSMIT_OK       1
int on_Transmit(pAgent_proto proto,pPCConn conn){
    Printf_DEBUG("normal cmd info");

    if(PCMANAGER_Get_RootID() == proto->toID){
        Printf_DEBUG("This is my Msg");
        on_MyMsgHere(proto,conn);
        goto exit;
    }

    switch(proto->toID){
    case AGENTID_UPPER_AGENT: 
        // analysis it && To All upper Agent
        on_MyMsgHere(proto,conn);
        Printf_DEBUG("To All upper Agent && analysis it ");
    case AGENTID_ADMIN: // To Admin, transmit it upper
        Printf_DEBUG("To Admin, transmit it upper");
        CMDParse_SendProto_Upper(proto);
        break;
    default:
        Printf_DEBUG("Need find this agent from tree iam(%d),target(%d)",
            PCMANAGER_Get_RootID(), proto->toID);
        pPCNodeInfo info2 = 
            PCMANAGER_GETNextJump(proto->toID);
        if(info2 == 
            PCMANAGER_GETNEXTJUMP_ERROR){
            goto error_exit;
        }
        pPCConn conn2 = &(info2->conn);
        if(conn2 == NULL){
            goto error_exit;
        }
        PROTO_SendProto(conn2,proto);
//        pPCConn conn2= AGENT_TUNN_BuildTunnel(proto->toID);
//        if(conn2 == AGENT_TUNN_BUILDTUNNEL_ERROR){
//            Printf_DEBUG("Find Error ???? targetid = %d",proto->toID);
//            return ON_TRANSMIT_ERROR;
//        }
//        PCCONN_Conn_2_Conn(conn,conn2,10000);
        break;
    }
error_exit:
    return ON_TRANSMIT_ERROR;
exit:
    return ON_TRANSMIT_OK;
}

int CMDParse_And_Do(pAgent_proto proto,pPCConn conn){
    int result = CMDPARSE_AND_DO_ERROR;
    switch(proto->cmdType){
    case CMDTYPE_NEWCONN:
        if(ON_NEWCONN_OK == on_NewConn(proto,conn)){
            result = CMDPARSE_AND_DO_OK;
        }
        break;
    case CMDTYPE_TUNNELCTRL:
//        Printf_DEBUG("add Tunnel Ctrl Code here");
        on_TunnelCTRL(proto,conn);
        break;
    case CMDTYPE_TRANSMIT:
        if(ON_TRANSMIT_OK == on_Transmit(proto,conn)){
            result = CMDPARSE_AND_DO_OK;
        }
        break;
    default:
        Printf_Error("UNKNOWN proto->cmdType %d",proto->cmdType);
        break;
    }
    return result;
}

