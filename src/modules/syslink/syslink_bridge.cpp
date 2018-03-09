/****************************************************************************
 *
 *   Copyright (c) 2016 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/


/**
 * @file syslink_bridge.cpp
 *
 * Character device for talking to the radio as a plain serial port
 */

#include "syslink_main.h"



SyslinkBridge::SyslinkBridge(Syslink *link) :
	CDev("SyslinkBridge", "/dev/bridge0"),
	_link(link),
	_readbuffer(16, sizeof(crtp_message_t))
{


}

SyslinkBridge::~SyslinkBridge()
{

}


int
SyslinkBridge::init()
{
	int ret = CDev::init();

	/* if init failed, bail now */
	if (ret != OK) {
		DEVICE_DEBUG("CDev init failed");
		return ret;
	}


	return ret;
}

pollevent_t
SyslinkBridge::poll_state(struct file *filp)
{
	pollevent_t state = 0;

	if (!_readbuffer.empty()) {
		state |= POLLIN;
	}

	if (_link->_writebuffer.space() > 0) {
		state |= POLLOUT;
	}

	return state;
}

ssize_t
SyslinkBridge::read(struct file *filp, char *buffer, size_t buflen)
{
	int nread = 0;
	crtp_message_t msg;

	while (!_readbuffer.empty() && buflen >= sizeof(CRTP_MAX_DATA_SIZE)) {
		_readbuffer.get(&msg, sizeof(msg));
		int size = msg.size - sizeof(msg.header);
		memcpy(buffer, &msg.data, size);

		nread += size;
		buffer += size;
		buflen -= size;
	}

	return nread;
}

ssize_t
SyslinkBridge::write(struct file *filp, const char *buffer, size_t buflen)
{
	crtp_message_t msg;
	msg.header = 0;
	//printf("buflen=%d\n", buflen );

	// Queue and send next time we get a RAW radio packet
	int remaining = buflen;

/*    printf("buflen=%d ", buflen );
     for (size_t i = 0; i < buflen; i++) {
         printf("%d ", (int)buffer[i]);}
    printf("\n");*/


	while (remaining > 0) {
		int datasize = MIN(remaining, CRTP_MAX_DATA_SIZE-1);
		msg.size = datasize + sizeof(msg.header);
		msg.port = CRTP_PORT_MAVLINK;
		memcpy(&msg.data, buffer, datasize);

		if(_link->_writebuffer.force(&msg, sizeof(crtp_message_t))){
			printf("write buffer overflow!!! \n");
		}

		buffer += datasize;
		remaining -= datasize;
	}

	/*static int ack_count=0;
	if (buflen==10){ //writing header of mavlink msg into the buffer
			if(msg.data[7]==77){
				ack_count++;
				printf("ack_count in write before=%d \n", ack_count);
			}
		
	}*/

	return buflen;
}

int
SyslinkBridge::ioctl(struct file *filp, int cmd, unsigned long arg)
{
	// All termios commands should be silently ignored as they are handled

	switch (cmd) {

	case FIONSPACE:
		*((int *) arg) = _link->_writebuffer.space() * CRTP_MAX_DATA_SIZE;
		return 0;

	default:
		/* give it to the superclass */
		CDev::ioctl(filp, cmd, arg);
		return 0;
	}
}


void
SyslinkBridge::pipe_message(crtp_message_t *msg)
{
	static int cmd_count=0;
	static int bytes_received=0;
	static int bytes_received_with_header=0;
	static int prev_seq=0; 

         if(msg->data[4]-prev_seq > 1)
         {
         	printf("msgs lost!!! msg lost=%d bytes_received=%d bytes_received_with_header=%d \n", msg->data[4]-prev_seq-1, bytes_received, bytes_received_with_header);
         	 bytes_received=0;
         	 bytes_received_with_header=0;
         }


	
	if(msg->data[7]==76)
			{cmd_count++;
				//int arm_disarm_cmd=(int)msg->data[13]*256 + msg->data[12];
				printf(" cmd_count after=%d, arm_disarm_cmd_12=%d arm_disarm_cmd_13=%d\n", cmd_count, msg->data[12], msg->data[13]);

			} //sizeof(msg->data)=31


		/*printf("syslink_bridge msg->size=%d ", msg->size);
		for (int i = 0; i < msg->size-1; i++) {
         printf("%d ",msg->data[i]);
        }
         printf("\n");*/

         bytes_received+=msg->size-1;
         bytes_received_with_header+=msg->size;
         prev_seq=msg->data[4];


	
	_readbuffer.force(msg, sizeof(msg->size) + msg->size);
	poll_notify(POLLIN);
}
