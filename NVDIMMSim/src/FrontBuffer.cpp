/*********************************************************************************
*  Copyright (c) 2011-2012, Paul Tschirhart
*                             Peter Enns
*                             Jim Stevens
*                             Ishwar Bhati
*                             Mu-Tien Chang
*                             Bruce Jacob
*                             University of Maryland 
*                             pkt3c [at] umd [dot] edu
*  All rights reserved.
*  
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions are met:
*  
*     * Redistributions of source code must retain the above copyright notice,
*        this list of conditions and the following disclaimer.
*  
*     * Redistributions in binary form must reproduce the above copyright notice,
*        this list of conditions and the following disclaimer in the documentation
*        and/or other materials provided with the distribution.
*  
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
*  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
*  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
*  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
*  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
*  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
*  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
*  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*********************************************************************************/

// FrontBuffer.cpp
// class file for the front side buffer for nvdimm

#include "FrontBuffer.h"
#include "NVDIMM.h"

using namespace NVDSim;

FrontBuffer::FrontBuffer(NVDIMM* parent, Ftl* f){
    parentNVDIMM = parent;
    sender = -1;
    ftl = f;

    // transaction queues, separate command queue not needed because commands
    // always accompany requests
    requests = queue<FlashTransaction>();
    responses = queue<FlashTransaction>();
    commands = queue<FlashTransaction>();

    // usage of buffer space
    requestsSize = 0;
    responsesSize = 0;
    
    // transaction currently being serviced by each potential channel
    requestTrans = FlashTransaction();
    responseTrans = FlashTransaction();
    commandTrans = FlashTransaction();
    
    // queue of transactions that have been finished by command or data channels but
    // still waiting on the other channel in a split channel setup
    pendingData = vector<FlashTransaction>();
    pendingCommand = vector<FlashTransaction>();

    // channel progress counters
    requestCyclesLeft = 0;
    responseCyclesLeft = 0;
    commandCyclesLeft = 0;
}

// place the transaction in the appropriate queue
bool FrontBuffer::addTransaction(FlashTransaction transaction){
    switch (transaction.transactionType)
    {
    case DATA_READ: 
	if(requestsSize <= (REQUEST_BUFFER_SIZE - COMMAND_LENGTH))
	{
	    requests.push(transaction);
	    if(ENABLE_COMMAND_CHANNEL)
	    {
		commands.push(transaction);
	    }
	    requestsSize += COMMAND_LENGTH;
	    return true;
	}
	else
	{
	    return false;
	}
    case DATA_WRITE:
	if(requestsSize <= (REQUEST_BUFFER_SIZE - (COMMAND_LENGTH + (NV_PAGE_SIZE*8))))
	{
	    requests.push(transaction);
	    if(ENABLE_COMMAND_CHANNEL)
	    {
		commands.push(transaction);
	    }
	    requestsSize += (COMMAND_LENGTH + (NV_PAGE_SIZE*8));
	    return true;
	}
	else
	{
	    return false;
	}
    case RETURN_DATA:
	if(responsesSize <= (RESPONSE_BUFFER_SIZE - (NV_PAGE_SIZE*8)))
	{
	    responses.push(transaction);
	    responsesSize += (NV_PAGE_SIZE*8);
	    return true;
	}
	else
	{
	    return false;
	}
    default:
	ERROR("Illegal transaction added \n");
	return false;
    }
}

void FrontBuffer::update(void){

    // always check the response channel status cause we will always have a
    // response channel
    // channel already doing something, keep doing it
    if(responseTrans.transactionType != EMPTY)
    {
	updateResponse();
    }
    // if half duplex case, responses get strict priority over requests
    else if(!responses.empty())
    {
	responseTrans = responses.front();
	responses.pop();
	responsesSize = subtract_params(responsesSize, (NV_PAGE_SIZE*8));
	updateResponse();
    }
    // half duplex case, requests also use the response channel
    // we need to do this even if there is a command channel because there
    // may be write data to send (read cases are handled by the updateResponse function)
    else if(!ENABLE_REQUEST_CHANNEL && !requests.empty())
    {
	responseTrans = newRequestTrans();
	updateResponse();
    }

    // full duplex case, requests use dedicated request channel
    if(ENABLE_REQUEST_CHANNEL)
    {
	// channel already doing something, keep doing it
	if(requestTrans.transactionType != EMPTY)
	{
	    updateRequest();
	}
	// again we need to do this even if there is a command channel for the 
	// write data (reads will be handled differently in the updateRequest function)
	else if(!requests.empty())
	{
	    requestTrans = newRequestTrans();
	    updateRequest();
	}
    }

    // separate command channel
    if(ENABLE_COMMAND_CHANNEL)
    {
	// channel already doing something, keep doing it
	if(commandTrans.transactionType != EMPTY)
	{
	    updateCommand();
	}
	else if(!commands.empty())
	{
	    commandTrans = commands.front();
	    commands.pop();
	    requestsSize = subtract_params(requestsSize, COMMAND_LENGTH);
	    updateCommand();
	}
    }
}

// creates a new request transaction
FlashTransaction FrontBuffer::newRequestTrans(void){
    FlashTransaction new_requestTrans = requests.front();
    if(!ENABLE_COMMAND_CHANNEL)
    {
	requestsSize = subtract_params(requestsSize, (COMMAND_LENGTH + (NV_PAGE_SIZE*8)));
    }
    else
    {
	requestsSize = subtract_params(requestsSize, (NV_PAGE_SIZE*8));
    }
    requests.pop();
    return new_requestTrans;
}

void FrontBuffer::updateResponse(void){
    // first time calling this for this command
    // need to figure out how many cycles we need to move the transaction to the FTL or Hybrid controller
    if(responseCyclesLeft == 0 && responseTrans.transactionType != EMPTY)
    {
	if(responseTrans.transactionType == RETURN_DATA)
	{
	    // number of channel cycles to go equals the total number of data bits divided by the bits 
	    // moved per channel cycle
	    responseCyclesLeft = divide_params_64b((NV_PAGE_SIZE*8), CHANNEL_WIDTH);
	}
	// no request channel to handle request data and commands so we need to handle those cases
	else if(!ENABLE_REQUEST_CHANNEL)
	{
	    // function handles determining the right number of cycles to delay by
	    responseCyclesLeft = setDataCycles(responseTrans, CHANNEL_WIDTH);
	}
	else
	{
	    // sanity check
	    ERROR("unidentified command on response channel");
	}
    }

    // we're updating so data has moved
    // even if we're calling this for the time
    responseCyclesLeft--;

    // we're done here
    if(responseCyclesLeft <= 0)
    {
	// just to make sure we precisely tigger the first if statement
	responseCyclesLeft = 0;

	if(responseTrans.transactionType == RETURN_DATA)
	{
	    // data has been moved, do the callback to hybrid
	    sendToHybrid(responseTrans);
	    responseTrans = FlashTransaction();
	}
	else{
	    if(ENABLE_COMMAND_CHANNEL)
	    {
		responseTrans = findTransaction(&pendingData, &pendingCommand, responseTrans);
	    }
	    else
	    {
		// no command channel to wait on so if we're done then just send to the FTL
		bool success = sendToFTL(responseTrans);
		
		// if we added successfully then remove the transaction and move on
		if(success)
		{
		    responseTrans = FlashTransaction();
		}
	    }
	}
    }
}

void FrontBuffer::updateRequest(void){
    // first time calling this for this command
    // need to figure out how many cycles we need to move the transaction to the FTL
    if(requestCyclesLeft == 0 && requestTrans.transactionType != EMPTY)
    {
	// function handles determining the right number of cycles to delay by
	requestCyclesLeft = setDataCycles(requestTrans, REQUEST_CHANNEL_WIDTH);
    }
    
    // we're updating so data has moved
    // even if we're calling this for the time
    requestCyclesLeft--;

    // we're done here
    if(requestCyclesLeft <= 0)
    {
	// just to make sure we precisely tigger the first if statement
	requestCyclesLeft = 0;
	
	if(ENABLE_COMMAND_CHANNEL)
	{
	    requestTrans = findTransaction(&pendingData, &pendingCommand, requestTrans);
	}
	else
	{
	    // no command channel to wait on so if we're done then just send to the FTL
	    bool success = sendToFTL(requestTrans);

	    // if we added successfully then remove the transaction and move on
	    if(success)
	    {
		requestTrans = FlashTransaction();
	    }
	}
    }
}

void FrontBuffer::updateCommand(void){
    // first time calling this for this command
    // need to figure out how many cycles we need to move the command to the FTL
    if(commandCyclesLeft == 0 && commandTrans.transactionType != EMPTY)
    {
	commandCyclesLeft = divide_params_64b(COMMAND_LENGTH, COMMAND_CHANNEL_WIDTH);
    }

    // we're updating so command bits have moved
    // even if we're calling this for the time
    commandCyclesLeft--;

    // we're done here
    if(commandCyclesLeft <= 0)
    {
	// just to make sure we precisely tigger the first if statement
	commandCyclesLeft = 0;

	commandTrans = findTransaction(&pendingCommand, &pendingData, commandTrans);
    }
}

uint64_t FrontBuffer::setDataCycles(FlashTransaction transaction, uint64_t width){
    if(transaction.transactionType == DATA_READ)
    {
	if(!ENABLE_COMMAND_CHANNEL)
	{
	    // number of channel cycles to go equals the total number of command bits
	    // divided by the bits moved per channel cycle
	    return divide_params_64b(COMMAND_LENGTH, width);
	}
	// command channel is handling the read command so do nothing
	// set to one so that the decrement doesn't break things
	else
	{
	    return 1;
	}
    }
    else if(transaction.transactionType == DATA_WRITE)
    {
	if(!ENABLE_COMMAND_CHANNEL)
	{
	    // number of channel cycles to go equals the total number of data bits plus the number of command bits
	    // divided by the bits moved per channel cycle
	    return divide_params_64b(((NV_PAGE_SIZE*8)+COMMAND_LENGTH), width);
	}
	else
	{
	    // number of channel cycles to go equals the total number of data bits divided by the bits 
	    // moved per channel cycle
	    return divide_params_64b((NV_PAGE_SIZE*8), width);
	}
    }
    ERROR("Something bad happened in setDataCycles");
    abort();
}

FlashTransaction FrontBuffer::findTransaction(std::vector<FlashTransaction> *pendingSearch, std::vector<FlashTransaction> *pendingAdd, FlashTransaction transaction){
    // switch to identify the situation where we are done but the FTL doesn't have room
    bool full = false;

    // see if the command had already been sent over the command channel
    vector<FlashTransaction>::iterator it;
    for (it = pendingSearch->begin(); it != pendingSearch->end(); it++)
    {
	// if the transactions are for the same address then the command channel has sent this
	// transactions command already
	if((*it).address == transaction.address)
	{
	    // command has already been sent, now data is done so send to FTL
	    bool success = sendToFTL(transaction);
	    
	    // if we added successfully then remove the transaction and move on
	    if(success)
	    {		
		// clear pending entry
		pendingSearch->erase(it);
		return FlashTransaction();
	    }
	    else
	    {
		// FTL doesn't have room so we need to do this all again
		// setting this to true keeps the following if statement from triggering
		full = true;
	    }
			
	    // iterators now invalid due to reordering, have to break
	    // should only be one entry that matches anyway
	    break;
	}
    }
    
    // didn't find anything, still have a transaction
    // need to wait for command channel to send command
    // so push onto the pendingCommand vector
    if(transaction.transactionType != EMPTY && full == false)
    {
	pendingAdd->push_back(transaction);
	
	// we can use the response channel for other things though
	// while waiting on the command channel
	return FlashTransaction();
    }

    // if nothing else happened then just send the same transaction back
    return transaction;
}

// we are done, these functions handle the finish cases
// done with outgoing data to dies, so add this transaction to the FTL cause the data has been sent
bool FrontBuffer::sendToFTL(FlashTransaction transaction){
    return ftl->addTransaction(transaction);
}

// done with returning data from dies, so initiate a callback 
void FrontBuffer::sendToHybrid(const FlashTransaction &transaction){
    if(parentNVDIMM->ReturnReadData!=NULL){
	(*parentNVDIMM->ReturnReadData)(parentNVDIMM->systemID, transaction.address, currentClockCycle, true);
    }
    parentNVDIMM->numReads++;
}
