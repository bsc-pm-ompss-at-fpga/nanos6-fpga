#include <cstdlib>
#include <vector>

#include "MPIDataTransfer.hpp"
#include "MPIMessenger.hpp"
#include "cluster/messages/Message.hpp"
#include "lowlevel/FatalErrorHandler.hpp"

#include <ClusterNode.hpp>
#include <MemoryAllocator.hpp>

#pragma GCC visibility push(default)
#include <mpi.h>
#pragma GCC visibility pop

MPIMessenger::MPIMessenger()
{
	int support, ret;
	
	MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &support);
	if (support != MPI_THREAD_MULTIPLE) {
		std::cerr << "Could not initialize multithreaded MPI" << std::endl;
		abort();
	}
	
	//! make sure that MPI errors are returned in the COMM_WORLD
	MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);
	
	//! Save the parent communicator
	ret = MPI_Comm_get_parent(&PARENT_COMM);
	checkSuccess(ret, MPI_COMM_WORLD);
	
	//! Create a new communicator
	ret = MPI_Comm_dup(MPI_COMM_WORLD, &INTRA_COMM);
	checkSuccess(ret, MPI_COMM_WORLD);
	
	//! make sure the new communicator returns errors
	MPI_Comm_set_errhandler(INTRA_COMM, MPI_ERRORS_RETURN);
	
	ret = MPI_Comm_rank(INTRA_COMM, &wrank);
	checkSuccess(ret, INTRA_COMM);
	
	ret = MPI_Comm_size(INTRA_COMM, &wsize);
	checkSuccess(ret, INTRA_COMM);
}

MPIMessenger::~MPIMessenger()
{
	int ret;
	
	//! Release the intra-communicator
	ret = MPI_Comm_free(&INTRA_COMM);
	checkSuccess(ret, INTRA_COMM);
	
	ret = MPI_Finalize();
	checkSuccess(ret, MPI_COMM_WORLD);
}

void MPIMessenger::sendMessage(Message *msg, ClusterNode const *toNode, bool block)
{
	int ret;
	Message::Deliverable *delv = msg->getDeliverable();
	const int mpiDst = toNode->getCommIndex();
	size_t msgSize = sizeof(delv->header) + delv->header.size;
	
	//! At the moment we use the Message id and the Message type to create
	//! the MPI tag of the communication
	int tag = (delv->header.id << 8) | delv->header.type;

	assert(mpiDst < wsize && mpiDst != wrank);
	assert(delv->header.size != 0);
	
	if (block) {
		ret = MPI_Send((void *)delv, msgSize, MPI_BYTE, mpiDst,
				tag, INTRA_COMM);
		checkSuccess(ret, INTRA_COMM);
		
		msg->markAsDelivered();
		return;
	}
	
	MPI_Request *request =
		(MPI_Request *)MemoryAllocator::alloc(
				sizeof(MPI_Request));
	FatalErrorHandler::failIf(
		request == nullptr,
		"Could not allocate memory for MPI_Request"
	);
	
	ret = MPI_Isend((void *)delv, msgSize, MPI_BYTE, mpiDst,
			tag, INTRA_COMM, request);
	checkSuccess(ret, INTRA_COMM);
	
	msg->setMessengerData((void *)request);
}

void MPIMessenger::sendData(const DataAccessRegion &region, const ClusterNode *to, int messageId)
{
	int ret, tag;
	const int mpiDst = to->getCommIndex();
	void *address = region.getStartAddress();
	size_t size = region.getSize();
	
	assert(mpiDst < wsize && mpiDst != wrank);
	
	tag = (messageId << 8) | DATA_RAW;
	
	ret = MPI_Send(address, size, MPI_BYTE, mpiDst, tag, INTRA_COMM);
	checkSuccess(ret, INTRA_COMM);
}

void MPIMessenger::fetchData(const DataAccessRegion &region, const ClusterNode *from, int messageId)
{
	int ret, tag;
	const int mpiSrc = from->getCommIndex();
	void *address = region.getStartAddress();
	size_t size = region.getSize();
	
	assert(mpiSrc < wsize && mpiSrc != wrank);
	
	tag = (messageId << 8) | DATA_RAW;
	
	ret = MPI_Recv(address, size, MPI_BYTE, mpiSrc, tag, INTRA_COMM, MPI_STATUS_IGNORE);
	checkSuccess(ret, INTRA_COMM);
}

void MPIMessenger::synchronizeAll(void)
{
	int ret = MPI_Barrier(INTRA_COMM);
	checkSuccess(ret, INTRA_COMM);
}

Message *MPIMessenger::checkMail(void)
{
	int ret, flag, count, type;
	MPI_Status status;
	Message::Deliverable *msg;
	
	ret = MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, INTRA_COMM, &flag, &status);
	checkSuccess(ret, INTRA_COMM);
	
	if (!flag) {
		return nullptr;
	}
	
	//! DATA_RAW type of messages will be received by matching 'fetchData'
	//! methods
	type = status.MPI_TAG & 0xff;
	if (type == DATA_RAW) {
		return nullptr;
	}
	
	ret = MPI_Get_count(&status, MPI_BYTE, &count);
	checkSuccess(ret, INTRA_COMM);
	
	msg = (Message::Deliverable *)malloc(count);
	if (!msg) {
		perror("malloc for message");
		MPI_Abort(INTRA_COMM, 1);
	}
	
	assert(count != 0);
	ret = MPI_Recv((void *)msg, count, MPI_BYTE, status.MPI_SOURCE,
			status.MPI_TAG, INTRA_COMM, MPI_STATUS_IGNORE);
	checkSuccess(ret, INTRA_COMM);
	
	return GenericFactory<int, Message*, Message::Deliverable*>::getInstance().create(type, msg);
}

void MPIMessenger::testMessageCompletion(
	std::vector<Message *> &messages
) {
	assert(!messages.empty());
	
	int msgCount = messages.size(), ret, completedCount;
	MPI_Request requests[msgCount];
	int finished[msgCount];
	MPI_Status status[msgCount];
	
	for (int i = 0; i < msgCount; ++i) {
		Message *msg = messages[i];
		assert(msg != nullptr);
		
		MPI_Request *req =
			(MPI_Request *)msg->getMessengerData();
		assert(req != nullptr);
		
		requests[i] = *req;
	}
	
	ret = MPI_Testsome(msgCount, requests, &completedCount,
			finished, status);
	checkSuccess(ret, status, completedCount, INTRA_COMM);
	
	for (int i = 0; i < completedCount; ++i) {
		int index = finished[i];
		Message *msg = messages[index];
		
		msg->markAsDelivered();
		MPI_Request *req = (MPI_Request *)msg->getMessengerData();
		MemoryAllocator::free(req, sizeof(MPI_Request));
	}
}

void MPIMessenger::testDataTransferCompletion(
	std::vector<DataTransfer *> &transfers
) {
	assert(!transfers.empty());
	
	int msgCount = transfers.size(), ret, completedCount;
	MPI_Request requests[msgCount];
	int finished[msgCount];
	MPI_Status status[msgCount];
	
	for (int i = 0; i < msgCount; ++i) {
		MPIDataTransfer *dt = (MPIDataTransfer *)transfers[i];
		assert(dt != nullptr);
		
		MPI_Request *req = dt->getMPIRequest();
		assert(req != nullptr);
		
		requests[i] = *req;
	}
	
	ret = MPI_Testsome(msgCount, requests, &completedCount, finished,
			status);
	checkSuccess(ret, status, completedCount, INTRA_COMM);
	
	for (int i = 0; i < completedCount; ++i) {
		int index = finished[i];
		MPIDataTransfer *dt = (MPIDataTransfer *)transfers[index];
		
		dt->markAsCompleted();
		MPI_Request *req = dt->getMPIRequest();
		MemoryAllocator::free(req, sizeof(MPI_Request));
	}
}
