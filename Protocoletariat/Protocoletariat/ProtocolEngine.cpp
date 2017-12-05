/*
Protocol Structure (Protocol Thread)
After the primary thread has initialized the necessary components for the program to function, it moves into the main protocol structure and handles the communication between the devices. It’s job is to ensure that the program is sending data to the serial port only when the paired device is ready to receive, and to ensure that if the other program wants to send data, that it is ready to receive that data.
*/
ProtocolThread()
{
	Idle()
}

/*
TRANSMIT Data Side
This side of the program is for when the system has data to send.
*/

/*
Idle
Protocol Entry Point
This is the default state of the main protocol, and it has only two directions: if there are no frames inside the program’s output queue it simply waits for an ENQ from the paired system; if there are frames inside the output queue then the device will transmit an ENQ to the paired in this state.
*/
Idle()
{
	// Loop
	while(true)
	{
		if (globalRVI)
		{
			globalRVI = false;
			globalDownloadQueue.pop();
			BidForLine();
		}
		
		// A signal has been received
		if(CommEvent (hComm, &dwEvent, NULL))
		{
			// Check if the front of the queue an ENQ
			if(globalDownloadQueue.front() = ENQ)
			{
				// Remove the ENQ
				globalDownloadQueue.pop();
				AcknowledgeBid();
			}
			else
			{
				break;
			}
		}
		
		// If this device wants to take the handle
		if(EnqRequestEvent (hComm, &dwEvent, NULL))
		{
			// Transmit ENQ
			globalUploadQueue.push(ENQ);
			
			// Move to BidForLine
			BidForLine();
		}
	}
}

/*
BidForLine
An ENQ has been TRANSMITTED
The program must wait to RECEIVE an ACK from the pair device to ensure that it is ready to receive data. If it does, it can proceed down the TRANSMIT tree. If the timeout expires before that time, the device must move to the delay state.
*/
BidForLine()
{
	// Start TOS
	int timer = 0;
	
	// Loop while timeout has not exceeded
	while(timer < 200)
	{
		// Check for a CommEventTrigger
		if (CommEvent (hComm, &dwEvent, NULL))
		{
			// Check if the front of the queue an ACK
			if(globalDownloadQueue.front() = ACK)
			{
				// Remove the ACK
				globalDownloadQueue.pop();
				
				// Move to SendData()
				SendData();
				return;
			}
		}
		sleep(10);
		timer++;
	}
}

/*
SendData
An ACK has been RECEIVED in the BidForLine state OR returning from ConfirmTransmission OR a Retransmit has been acknowledged
The program’s bid for the line has been acknowledged and the other system is moving into a RECEIVING state. The program will start a loop to wait for the CommEvent triggered by the upload buffer. The program will send the frame,created in the File Upload Thread, at the front of it’s queue. It will then move to the ConfirmTransmission state on a Data Frame transmission. If the EOT Control Frame is submitted or the system has sent 10 frames, it will instead proceed to the LinkReset State.
*/
bool SendData()
{
	// Start TOS
	int timer = 0;
	int dfs = 0;
	
	do
	{	
		// Loop while the timeout has no exceeded
		while(timer < 20)
		{
			// If CommEvent Triggered
			if (CommEvent (hComm, &dwEvent, NULL))
			{
				// If front of download queue is RVI
				if(globalDownloadQueue.front() = RVI)
				{
					// Set global RVI variable to false
					globalRVI = true;
					// Clear download buffer
					globalDownloadQueue.clear()
					// Transmit EOT control frame through serial port
					
					// Move to LinkReset
					LinkReset();
					return;
				}
				// If front of upload queue is EOT
				else if (globalUploadQueue.front() = EOT)
				{
					// Transmit EOT control frame through Serial Port
					
					// Move to LinkReset
					LinkReset();
					return;
				}
				// If the frame at the front of the upload queue is a data frame
				else if (globalUploadQueue.front() = STX)
				{
					// Transmit the data frame through the serial port
					
					// Move to ConfirmTransmission
					
					if (!ConfirmTransmission())
					{
						// Transmit EOT control frame through Serial Port
						
						// Move to LinkReset
						LinkReset();
						return;
					}
					timer = 0;
					continue;
				}
			}
			sleep(10);
			timer++;
		} // TOS expires
	} while (dfs < 10) // 10 Frames confirmed
}

/*
ConfirmTransmission
A Data Frame has been TRANSMITTED
The program must wait for an ACK control frames from the paired device to continue with its transmission. If the program RECEIVES the ACK Control Frame it can pop the TRANSMITTED Data Frame from the queue and return to the SendData state. If the timeout expires, then the program will instead move to the Retransmit state.
*/
bool ConfirmTransmission()
{
	// Start TOR
	int timer = 0;
	
	//initialize success
	bool success = false;
	
	// Loop while timeout has not expired
	while(timer < 20 && success == false)
	{
		// If CommEvent Triggered
		if (CommEvent (hComm, &dwEvent, NULL))
		{
			// If download queue front is ACK
			if (globalUploadQueue.front() = ACK)
			{
				// Pop front of download buffer
				globaldownloadQueue.pop();
				// Pop front of upload buffer
				globaluploadQueue.pop();
				// Increment logfile successful frames variable
				logfile.successfulFrames++;
				// Move back to SendData
				return true;
			}
		}
	} // TOR expires
	
	if(success == false)
	{
		// Move to retransmit
		success = Retransmit();
	}
	
	//return success
	return success;
}

/*
Retransmit
A Data Frame has failed to TRANSMIT
The program will attempt three retransmissions of the same frame, and wait to RECEIVE the ACK. If all three transmissions fail, then the program will move to the LinkDelay state. If however, one of the transmission is responded to by an ACK then the program will pop the front frame from the output queue and return to the SendData state.
*/
Retransmit()
{
	// Initialize txCounter variable to 1
	int txCounter = 1;
	
	// Start TOR
	int timer;
	
	// Retransmit up to 3 times
	do
	{
		// Loop while timer has not expired
		while(timer < 20)
		{
			// If CommEvent Triggered
			if (CommEvent (hComm, &dwEvent, NULL))
			{
				// If download queue front is ACK
				if (globalUploadQueue.front() = ACK)
				{
					// Pop front of download buffer
					globaldownloadQueue.pop();
					// Pop front of upload buffer
					globaluploadQueue.pop();
					// Increment logfile successful frames variable
					logfile.successfulFrames++;
					// Move back to SendData
					return true;
				}
			}
		} // TOR expires
		txCounter++;
	} while (txCounter < 3);
	
	//return
	return false;
}

/*
LinkReset
Any State on the TRANSMIT side has experienced a timeout expiry, or failed
This state exists as a buffer for the post-TRANSMIT states, to stop one device from hogging the line. This state begins a TOR listens to RECEIVE an ENQ from the pair device. If the TOR expires before an ENQ is RECEIVED, the program move to the idle state.
*/
LinkReset()
{
	// Start TOR
	int timer = 0;
	
	// Loop while timeout has not expired
	while(timer < 20)
	{
		// If CommEvent Triggered
		if (CommEvent (hComm, &dwEvent, NULL))
		{
			// Check if the front of the queue an ENQ
			if(globalDownloadQueue.front() = ENQ)
			{
				// Pop download queue front
				globalDownloadQueue.pop();
				// Move to AcknowledgeBid
				Idle(true);
			}
			else
			{
				return;
			}
		}
	}		
}

/*
RECEIVE Data Side
This side of the protocol handles the receiving of data from the paired device. 
*/

/*
AcknowledgeBid
An ENQ has been RECEIVED
This state is necessary to acknowledge the other device’s bid for the line.
*/
AcknowledgeBid()
{
	// Transmit ACK control frame
	
	// Move to ReceiveData
	ReceiveData();
}

/*
ReceiveData
An ACK has been TRANSMITTED in response to the RECEIVED ENQ
This state will prepare the system to RECEIVE Data Frames from the serial port. This state will wait for a Data Frame from the serial port, until the timeout expires. This timeout is required to be 3 times longer than the corresponding wait on the TRANSMIT Data Side since it must account for 3 failed transmissions.
*/
ReceiveData()
{
	// Start TOR
	int timer = 0;
	
	// Initialize string holder
	String frame = "";
	
	// Initialize error detection success
	bool errorDetection = false;
	
	// Initialize recieved/failed frames counter
	int RxCounter = 0;
	int FailedFrames = 0;
	
	// Loop until 3 consecutive failures
	do
	{
		// Loop while timeout not exceeded
		while(timer < 20)
		{
			// If RVIevent triggered
			if (RVIevent (hComm, &dwEvent, NULL))
			{
				// Move to Idle
				return;
			}
			// If CommEvent is triggered
			if (CommEvent (hComm, &dwEvent, NULL))
			{
				// If download queue front is EOT
				if(globalDownloadQueue.front() = EOT)
				{
					// Remove the EOT char
					globalDownloadQueue.pop();
					// Return to idle
					return;
				}
				// Else if download queue front is STX
				if(globalDownloadQueue.front() = STX)
				{
					// Remove STX char
					globlaDownloadQueue.pop();

					// Send the frame for error detection
					if(ErrorDetection())
					{
						// Increment the frames received counter
						RxCounter++;
						continue;
					}
					else
					{
						// Increment the failed frames counter
						FailedFrames++;
						// Reset the timer
						timer = 0;
						continue;
					}
				}
			}
			// Increment the timer
			sleep(10);
			timer++;
		}
	} while (RxCounter < 10 && FailedFrames < 3) // 10 Frames have been successfully received	
}

/*
ErrorDetection
A Data Frame has been RECEIVED
This state will handle the detection of errors using the CRC at the end of the of the Data Frame. All remaining bytes will be read from the download queue; the first 512 will be the data bytes, the next 4 will be the CRC bytes. If no error is detected the system will TRANSMIT an ACK, send the received data to be printed, and return to the Receive state. Otherwise the system will timeout and return to the Receive state without TRANSMITTING an ACK. This timeout must be very short.
*/
bool ErrorDetection()
{
	// Start TOS
	int timer = 0;
	
	// Initialize frame and CRC holder
	String frame = "";
	String CRC = "";
	
	while (timer < 20)
	{
		// Get the next 512 characters from the download queue
		for(i = 0; i < 512; i++)
		{
			// Read all chars from the front of the queue and remove it
			frame += globalDownloadQueue.front();
			globlaDownloadQueue.pop();
		}
		for(i = 0; i < 4; i ++)
		{
			// Read the remaining chars from the front of the queue as CRC
			frame += globalDownloadQueue.front();
			globlaDownloadQueue.pop();
		}
		
		// Implement CRC error detection -- use available source code
		
		// If an error is detected
		if(errorDetected)
		{
			// Increment logfile corrupt frame counter
			logfile.corruptFrameCounter++;
			// Move back to ReceiveData
			return false;
		}
		else
		{
			// Send data to print
			
			// Increment the logfile successful frames counter
			logfile.successfulFramesCounter++;
			// Transmit ACK control frame
			
			// Move back to ReceiveData
			return true;
		}
		sleep(20);
		timer++;
	} // TOS Ends
	// Move back to ReceiveData
	return false;
}