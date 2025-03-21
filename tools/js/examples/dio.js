/***************************************************************************//**
 *   @file   dio.js
 *   @brief  4 bit counter using DigitalIO
 *   @author Antoniu Miclaus (antoniu.miclaus@analog.com)
********************************************************************************
 * Copyright 2018(c) Analog Devices, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  - Neither the name of Analog Devices, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *  - The use of this software may or may not infringe the patent rights
 *    of one or more patent holders.  This license does not release you
 *    from the requirement that you obtain separate licenses from these
 *    patent holders to use this software.
 *  - Use of the software either in source or binary form, must be run
 *    on or directly connected to an Analog Devices Inc. component.
 *
 * THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, NON-INFRINGEMENT,
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ANALOG DEVICES BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, INTELLECTUAL PROPERTY RIGHTS, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

/* Setup DigitalIO */
function set_dio(){
	
	/* Set first 4 digital channels as output */ 
	for (var i = 0; i < 4; i++){
		dio.dir[i]=true
		dio.out[i]=true
	}
	
	/* Set the rest of 12 channels as input */
	for ( var i = 4; i < 16; i++){
		dio.dir[i]=false
		dio.out[i]=false
	}
	
	/* Run DigitalIO */
	dio.running = true
	msleep(1000)
	
}

/* 4-bit Binary count function */
function binary_counter(){
	
	for(var i = 0; i <= 15; i++){
		
		var k = i
		for(var j = 0; j <4; j++){
			
			dio.out[j] = k % 2
			k = Math.floor(k/2)
		
		}
		
		msleep(1000)
		
	}
	dio.running = false
}

/* main function */
function main(){
	/* Feel free to modify this as needed */
	var uri = "ip:192.168.2.1"

	var devId = scopy.addDevice(uri)
	var connected = scopy.connectDevice(devId)
	msleep(1000)
	if (!connected)
		return Error()
	scopy.switchTool("Digital I/O")
	set_dio()
	binary_counter()
	
	msleep(1000)

	scopy.disconnectDevice(devId)
	scopy.removeDevice(uri)
	msleep(1000)
}

/* To keep the application session after running a certain script */
/* use the command line options: -r or --keep-running. */
main()