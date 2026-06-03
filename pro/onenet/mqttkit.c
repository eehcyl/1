#include "mqttkit.h"

//C?
#include <string.h>
#include <stdio.h>

#define CMD_TOPIC_PREFIX		"$creq"


//==========================================================
//	????:	EDP_NewBuffer
//
//	????:	????
//
//	????:	edpPacket:????
//				size:??
//
//	????:	?
//
//	??:		1.????????????
//				2.???????????????
//==========================================================
void MQTT_NewBuffer(MQTT_PACKET_STRUCTURE *mqttPacket, uint32 size)
{
	
	uint32 i = 0;

	if(mqttPacket->_data == NULL)
	{
		mqttPacket->_memFlag = MEM_FLAG_ALLOC;
		
		mqttPacket->_data = (uint8 *)MQTT_MallocBuffer(size);
		if(mqttPacket->_data != NULL)
		{
			mqttPacket->_len = 0;
			
			mqttPacket->_size = size;
			
			for(; i < mqttPacket->_size; i++)
				mqttPacket->_data[i] = 0;
		}
	}
	else
	{
		mqttPacket->_memFlag = MEM_FLAG_STATIC;
		
		for(; i < mqttPacket->_size; i++)
			mqttPacket->_data[i] = 0;
		
		mqttPacket->_len = 0;
		
		if(mqttPacket->_size < size)
			mqttPacket->_data = NULL;
	}

}

//==========================================================
//	????:	MQTT_DeleteBuffer
//
//	????:	??????
//
//	????:	edpPacket:????
//
//	????:	?
//
//	??:		
//==========================================================
void MQTT_DeleteBuffer(MQTT_PACKET_STRUCTURE *mqttPacket)
{

	if(mqttPacket->_memFlag == MEM_FLAG_ALLOC)
		MQTT_FreeBuffer(mqttPacket->_data);
	
	mqttPacket->_data = NULL;
	mqttPacket->_len = 0;
	mqttPacket->_size = 0;
	mqttPacket->_memFlag = MEM_FLAG_NULL;

}

int32 MQTT_DumpLength(size_t len, uint8 *buf)
{
	
	int32 i = 0;
	
	for(i = 1; i <= 4; ++i)
	{
		*buf = len % 128;
		len >>= 7;
		if(len > 0)
		{
			*buf |= 128;
			++buf;
		}
		else
		{
			return i;
		}
	}

	return -1;
}

int32 MQTT_ReadLength(const uint8 *stream, int32 size, uint32 *len)
{
	
	int32 i;
	const uint8 *in = stream;
	uint32 multiplier = 1;

	*len = 0;
	for(i = 0; i < size; ++i)
	{
		*len += (in[i] & 0x7f) * multiplier;

		if(!(in[i] & 0x80))
		{
			return i + 1;
		}

		multiplier <<= 7;
		if(multiplier >= 2097152)		//128 * *128 * *128
		{
			return -2;					// error, out of range
		}
	}

	return -1;							// not complete

}

//==========================================================
//	????:	MQTT_UnPacketRecv
//
//	????:	MQTT????????
//
//	????:	dataPtr:???????
//
//	????:	0-??		??-????
//
//	??:		
//==========================================================
uint8 MQTT_UnPacketRecv(uint8 *dataPtr)
{
	
	uint8 status = 255;
	uint8 type = dataPtr[0] >> 4;				//????
	
	if(type < 1 || type > 14)
		return status;
	
	if(type == MQTT_PKT_PUBLISH)
	{
		uint8 *msgPtr;
		uint32 remain_len = 0;
		
		msgPtr = dataPtr + MQTT_ReadLength(dataPtr + 1, 4, &remain_len) + 1;
		
		if(remain_len < 2 || dataPtr[0] & 0x01)					//retain
			return 255;
		
		if(remain_len < ((uint16)msgPtr[0] << 8 | msgPtr[1]) + 2)
			return 255;
		
		if(strstr((int8 *)msgPtr + 2, CMD_TOPIC_PREFIX) != NULL)	//???????
			status = MQTT_PKT_CMD;
		else
			status = MQTT_PKT_PUBLISH;
	}
	else
		status = type;
	
	return status;

}

//==========================================================
//	????:	MQTT_PacketConnect
//
//	????:	??????
//
//	????:	user:???:??ID
//				password:??:?????apikey
//				devid:??ID
//				cTime:??????
//				clean_session:????????
//				qos:????
//				will_topic:????topic
//				will_msg:??????
//				will_retain:??????
//				mqttPacket:???
//
//	????:	0-??		??-??
//
//	??:		
//==========================================================
uint8 MQTT_PacketConnect(const int8 *user, const int8 *password, const int8 *devid,
						uint16 cTime, uint1 clean_session, uint1 qos,
						const int8 *will_topic, const int8 *will_msg, int32 will_retain,
						MQTT_PACKET_STRUCTURE *mqttPacket)
{
	
	uint8 flags = 0;
	uint8 will_topic_len = 0;
	uint16 total_len = 15;
	int16 len = 0, devid_len = strlen(devid);
	int16 user_len = strlen(user),password_len = strlen(password);
	
	if(!devid)
		return 1;
	
	total_len += devid_len + 2;
	
	//???,????????:1-??	0-???--------------------------------------------
	if(clean_session)
	{
		flags |= MQTT_CONNECT_CLEAN_SESSION;
	}
	
	//???????,??????topic------------------------------------------------------
	if(will_topic)
	{
		flags |= MQTT_CONNECT_WILL_FLAG;
		will_topic_len = strlen(will_topic);
		total_len += 4 + will_topic_len + strlen(will_msg);
	}
	
	//qos??--????PUBLISH(???)???,?????????-----------------------------
	switch((unsigned char)qos)
	{
		case MQTT_QOS_LEVEL0:
			flags |= MQTT_CONNECT_WILL_QOS0;							//????
		break;
		
		case MQTT_QOS_LEVEL1:
			flags |= (MQTT_CONNECT_WILL_FLAG | MQTT_CONNECT_WILL_QOS1);	//????
		break;
		
		case MQTT_QOS_LEVEL2:
			flags |= (MQTT_CONNECT_WILL_FLAG | MQTT_CONNECT_WILL_QOS2);	//????
		break;
		
		default:
		return 2;
	}
	
	//????PUBLISH(???)???,???????????????,??????????,????????????????????????????
	if(will_retain)
	{
		flags |= (MQTT_CONNECT_WILL_FLAG | MQTT_CONNECT_WILL_RETAIN);
	}
	
	//???? ????---------------------------------------------------------------------
	if(!user || !password)
	{
		return 3;
	}
	flags |= MQTT_CONNECT_USER_NAME | MQTT_CONNECT_PASSORD;
	
	total_len += strlen(user) + strlen(password) + 4;
	
	//????-----------------------------------------------------------------------------
	MQTT_NewBuffer(mqttPacket, total_len);
	if(mqttPacket->_data == NULL)
		return 4;
	
	memset(mqttPacket->_data, 0, total_len);
	
/*************************************????***********************************************/
	
	//????----------------------??????---------------------------------------------
	mqttPacket->_data[mqttPacket->_len++] = MQTT_PKT_CONNECT << 4;
	
	//????----------------------?????-----------------------------------------------
	len = MQTT_DumpLength(total_len - 5, mqttPacket->_data + mqttPacket->_len);
	if(len < 0)
	{
		MQTT_DeleteBuffer(mqttPacket);
		return 5;
	}
	else
		mqttPacket->_len += len;
	
/*************************************????***********************************************/
	
	//????----------------------????? ? ???--------------------------------------
	mqttPacket->_data[mqttPacket->_len++] = 0;
	mqttPacket->_data[mqttPacket->_len++] = 4;
	mqttPacket->_data[mqttPacket->_len++] = 'M';
	mqttPacket->_data[mqttPacket->_len++] = 'Q';
	mqttPacket->_data[mqttPacket->_len++] = 'T';
	mqttPacket->_data[mqttPacket->_len++] = 'T';
	
	//????----------------------protocol level 4-----------------------------------------
	mqttPacket->_data[mqttPacket->_len++] = 4;
	
	//????----------------------????(??????????)-----------------------------
    mqttPacket->_data[mqttPacket->_len++] = flags;
	
	//????----------------------???????(?)----------------------------------------
	mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(cTime);
	mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(cTime);
	 
/*************************************???************************************************/

	//???----------------------------devid???devid-------------------------------------
	mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(devid_len);
	mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(devid_len);
	
	strncat((int8 *)mqttPacket->_data + mqttPacket->_len, devid, devid_len);
	mqttPacket->_len += devid_len;
	

	//???----------------------------will_flag ? will_msg---------------------------------
	if(flags & MQTT_CONNECT_WILL_FLAG)
	{
		unsigned short mLen = 0;
		
		if(!will_msg)
			will_msg = "";
		
		mLen = strlen(will_topic);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(mLen);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(mLen);
		strncat((int8 *)mqttPacket->_data + mqttPacket->_len, will_topic, mLen);
		mqttPacket->_len += mLen;
		
		mLen = strlen(will_msg);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(mLen);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(mLen);
		strncat((int8 *)mqttPacket->_data + mqttPacket->_len, will_msg, mLen);
		mqttPacket->_len += mLen;
	}
	
	//???----------------------------use---------------------------------------------------
	if(flags & MQTT_CONNECT_USER_NAME)
	{
		unsigned short user_len = strlen(user);
		
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(user_len);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(user_len);
		strncat((int8 *)mqttPacket->_data + mqttPacket->_len, user, user_len);
		mqttPacket->_len += user_len;
	}

	//???----------------------------password----------------------------------------------
	if(flags & MQTT_CONNECT_PASSORD)
	{
		unsigned short psw_len = strlen(password);
		
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(psw_len);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(psw_len);
		strncat((int8 *)mqttPacket->_data + mqttPacket->_len, password, psw_len);
		mqttPacket->_len += psw_len;
	}
		
	return 0;

}

//==========================================================
//	????:	MQTT_UnPacketConnectAck
//
//	????:	??????
//
//	????:	rev_data:?????
//
//	????:	1?255-??		??-??????
//
//	??:		
//==========================================================
uint8 MQTT_UnPacketConnectAck(uint8 *rev_data)
{

	if(rev_data[1] != 2)
		return 1;
	
	if(rev_data[2] == 0 || rev_data[2] == 1)
		return rev_data[3];
	else
		return 255;

}

//==========================================================
//	????:	MQTT_PacketSaveData
//
//	????:	???????
//
//	????:	devid:??ID(???)
//				send_buf:json??buf
//				send_len:json??
//				type_bin_head:bin??????
//				type:??
//
//	????:	0-??		1-??
//
//	??:		
//==========================================================
uint1 MQTT_PacketSaveData(const int8 *devid, int16 send_len, int8 *type_bin_head, uint8 type, MQTT_PACKET_STRUCTURE *mqttPacket)
{

	if(MQTT_PacketPublish(MQTT_PUBLISH_ID, "$sys/D283E5KDGV/test/thing/property/post", NULL, send_len, MQTT_QOS_LEVEL0, 0, 1, mqttPacket) == 0)  //Publish??
	{
	}
	else
		return 1;

	return 0;
}

//==========================================================
//	????:	MQTT_UnPacketCmd
//
//	????:	??????
//
//	????:	rev_data:???????
//				cmdid:cmdid-uuid
//				req:??
//
//	????:	0-??		??-????
//
//	??:		
//==========================================================
uint8 MQTT_UnPacketCmd(uint8 *rev_data, int8 **cmdid, int8 **req, uint16 *req_len)
{

	int8 *dataPtr = strchr((int8 *)rev_data + 6, '/');	//?6??????
	
	uint32 remain_len = 0;
	
	if(dataPtr == NULL)									//???'/'
		return 1;
	dataPtr++;											//??'/'
	
	MQTT_ReadLength(rev_data + 1, 4, &remain_len);		//??????
	
	*cmdid = (int8 *)MQTT_MallocBuffer(37);				//cmdid??36??,???????????
	if(*cmdid == NULL)
		return 2;
	
	memset(*cmdid, 0, 37);								//????
	memcpy(*cmdid, (const int8 *)dataPtr, 36);			//??cmdid
	dataPtr += 36;
	
	*req_len = remain_len - 44;							//???? = ????(remain_len) - 2 - 5($creq) - 1(\) - cmdid??
	*req = (int8 *)MQTT_MallocBuffer(*req_len + 1);		//??????+1
	if(*req == NULL)
	{
		MQTT_FreeBuffer(*cmdid);
		return 3;
	}
	
	memset(*req, 0, *req_len + 1);						//??
	memcpy(*req, (const int8 *)dataPtr, *req_len);		//????
	
	return 0;

}

//==========================================================
//	????:	MQTT_PacketCmdResp
//
//	????:	??????
//
//	????:	cmdid:cmdid
//				req:??
//				mqttPacket:???
//
//	????:	0-??		1-??
//
//	??:		
//==========================================================
uint1 MQTT_PacketCmdResp(const int8 *cmdid, const int8 *req, MQTT_PACKET_STRUCTURE *mqttPacket)
{
	
	uint16 cmdid_len = strlen(cmdid);
	uint16 req_len = strlen(req);
	_Bool status = 0;
	
	int8 *payload = MQTT_MallocBuffer(cmdid_len + 6);
	if(payload == NULL)
		return 1;
	
	memset(payload, 0, cmdid_len + 6);
	memcpy(payload, "$crsp/", 6);
	strncat(payload, cmdid, cmdid_len);

	if(MQTT_PacketPublish(MQTT_PUBLISH_ID, payload, req, strlen(req), MQTT_QOS_LEVEL0, 0, 1, mqttPacket) == 0)
		status = 0;
	else
		status = 1;
	
	MQTT_FreeBuffer(payload);
	
	return status;

}


//==========================================================
//	????:	MQTT_PacketPublish
//
//	????:	Pulish????
//
//	????:	pkt_id:pkt_id
//				topic:???topic
//				payload:???
//				payload_len:?????
//				qos:????
//				retain:??????
//				own:
//				mqttPacket:???
//
//	????:	0-??		??-??
//
//	??:		
//==========================================================
uint8 MQTT_PacketPublish(uint16 pkt_id ,const int8 *topic,
						const int8 *payload, uint32 payload_len,
						enum MqttQosLevel qos, int32 retain, int32 own,
						MQTT_PACKET_STRUCTURE *mqttPacket)
{

	uint32 total_len = 0, topic_len = 0;
	int32 len = 0;
	uint8 flags = 0;
	
	//pkt_id??----------------------------------------------------------------------------
	if(pkt_id == 0)
		return 1;
	
	//$dp???????????--------------------------------------------------------------
	for(topic_len = 0; topic[topic_len] != '\0'; ++topic_len)
	{
		if((topic[topic_len] == '#') || (topic[topic_len] == '+'))
			return 2;
	}
	
	//Publish??---------------------------------------------------------------------------
	flags |= MQTT_PKT_PUBLISH << 4;
	
	//retain??----------------------------------------------------------------------------
	if(retain)
		flags |= 0x01;
	
	//???--------------------------------------------------------------------------------
	total_len = topic_len + payload_len + 2;
	
	//qos??--????PUBLISH(???)???,?????????-----------------------------
	switch(qos)
	{
		case MQTT_QOS_LEVEL0:
			flags |= MQTT_CONNECT_WILL_QOS0;	//????
		break;
		
		case MQTT_QOS_LEVEL1:
			flags |= 0x02;						//????
			total_len += 2;
		break;
		
		case MQTT_QOS_LEVEL2:
			flags |= 0x04;						//????
			total_len += 2;
		break;
		
		default:
		return 3;
	}
	
	//????------------------------------------------------------------------------------
	MQTT_NewBuffer(mqttPacket, total_len + 3);
	if(mqttPacket->_data == NULL)
		return 4;
		
	memset(mqttPacket->_data, 0, total_len + 3);
	
/*************************************????***********************************************/
	
	//????----------------------????-------------------------------------------------
	mqttPacket->_data[mqttPacket->_len++] = flags;
	
	//????----------------------?????-----------------------------------------------
	len = MQTT_DumpLength(total_len, mqttPacket->_data + mqttPacket->_len);
	if(len < 0)
	{
		MQTT_DeleteBuffer(mqttPacket);
		return 5;
	}
	else
		mqttPacket->_len += len;
	
/*************************************????***********************************************/
	
	//????----------------------??topic???topic-------------------------------------
	mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(topic_len);
	mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(topic_len);
	
	strncat((int8 *)mqttPacket->_data + mqttPacket->_len, topic, topic_len);
	mqttPacket->_len += topic_len;
	if(qos != MQTT_QOS_LEVEL0)
	{
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_MSB(pkt_id);
		mqttPacket->_data[mqttPacket->_len++] = MOSQ_LSB(pkt_id);
	}
	
	
	
	//????----------------------??payload----------------------------------------------
	if(payload != NULL)
	{
		strncat((int8 *)mqttPacket->_data + mqttPacket->_len, payload, payload_len);
		mqttPacket->_len += payload_len;
	}
	
	return 0;

}

//==========================================================
//	????:	MQTT_UnPacketPublish
//
//	????:	Publish????
//
//	????:	flags:MQTT??????
//				pkt:??????
//				size:????????????
//
//	????:	0-??		??-????
//
//	??:		
//==========================================================
uint8 MQTT_UnPacketPublish(uint8 *rev_data, int8 **topic, uint16 *topic_len, int8 **payload, uint16 *payload_len, uint8 *qos, uint16 *pkt_id)
{
	
	const int8 flags = rev_data[0] & 0x0F;
	uint8 *msgPtr;
	uint32 remain_len = 0;

	const int8 dup = flags & 0x08;

	*qos = (flags & 0x06) >> 1;
	
	msgPtr = rev_data + MQTT_ReadLength(rev_data + 1, 4, &remain_len) + 1;
	
	if(remain_len < 2 || flags & 0x01)							//retain
		return 255;
	
	*topic_len = (uint16)msgPtr[0] << 8 | msgPtr[1];
	if(remain_len < *topic_len + 2)
		return 255;
	
	if(strstr((int8 *)msgPtr + 2, CMD_TOPIC_PREFIX) != NULL)	//???????
		return MQTT_PKT_CMD;
	
	switch(*qos)
	{
		case MQTT_QOS_LEVEL0:									// qos0 have no packet identifier
			
			if(0 != dup)
				return 255;

			*topic = MQTT_MallocBuffer(*topic_len + 1);			//?topic????
			if(*topic == NULL)
				return 255;
			
			memset(*topic, 0, *topic_len + 1);
			memcpy(*topic, (int8 *)msgPtr + 2, *topic_len);		//????
			
			*payload_len = remain_len - 2 - *topic_len;			//?payload????
			*payload = MQTT_MallocBuffer(*payload_len + 1);
			if(*payload == NULL)								//????
			{
				MQTT_FreeBuffer(*topic);						//????topic??????
				return 255;
			}
			
			memset(*payload, 0, *payload_len + 1);
			memcpy(*payload, (int8 *)msgPtr + 2 + *topic_len, *payload_len);
			
		break;

		case MQTT_QOS_LEVEL1:
		case MQTT_QOS_LEVEL2:
			
			if(*topic_len + 2 > remain_len)
				return 255;
			
			*pkt_id = (uint16)msgPtr[*topic_len + 2] << 8 | msgPtr[*topic_len + 3];
			if(pkt_id == 0)
				return 255;
			
			*topic = MQTT_MallocBuffer(*topic_len + 1);			//?topic????
			if(*topic == NULL)
				return 255;
			
			memset(*topic, 0, *topic_len + 1);
			memcpy(*topic, (int8 *)msgPtr + 2, *topic_len);		//????
			
			*payload_len = remain_len - 4 - *topic_len;
			*payload = MQTT_MallocBuffer(*payload_len + 1);		//?payload????
			if(*payload == NULL)								//????
			{
				MQTT_FreeBuffer(*topic);						//????topic??????
				return 255;
			}
			
			memset(*payload, 0, *payload_len + 1);
			memcpy(*payload, (int8 *)msgPtr + 4 + *topic_len, *payload_len);
			
		break;

		default:
			return 255;
	}
	
	if(strchr((int8 *)topic, '+') || strchr((int8 *)topic, '#'))
		return 255;

	return 0;

}


//==========================================================
//	????:	MQTT_UnPacketPublishAck
//
//	????:	Publish Ack????
//
//	????:	rev_data:?????
//
//	????:	0-??		1-????
//
//	??:		
//==========================================================
uint1 MQTT_UnPacketPublishAck(uint8 *rev_data)
{

	if(rev_data[1] != 2)
		return 1;

	if(rev_data[2] == MOSQ_MSB(MQTT_PUBLISH_ID) && rev_data[3] == MOSQ_LSB(MQTT_PUBLISH_ID))
		return 0;
	else
		return 1;

}

