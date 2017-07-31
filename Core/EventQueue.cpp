#include "EventQueue.h"
#if defined(USE_GPU)
#include <helper_cuda.h>
#endif // USE_GPU

CUDA_CALLABLE EventQueue::EventQueue() 
{
    m_clusterID = 0;
    m_queueEvent = NULL;
    m_nMaxEvent = 0;
    m_idxQueue = 0;
    m_eventHandler = NULL;

#if defined(USE_GPU)
    m_nMaxInterClustersOutgoingEvents = 0;
    m_nInterClustersOutgoingEvents = 0;
    m_interClustersOutgoingEvents = NULL;

    m_nMaxInterClustersIncomingEvents = 0;
    m_nInterClustersIncomingEvents = 0;
    m_interClustersIncomingEvents = NULL;
#endif // USE_GPU
}

CUDA_CALLABLE EventQueue::~EventQueue()
{
    // de-alloacte memory for the collection of event queue
    if (m_queueEvent != NULL) {
        delete[] m_queueEvent;
        m_queueEvent = NULL;
    }

#if defined(USE_GPU)
    // In the device side memories, it should use cudaFree() and set NULL 
    // before destroying the object

    // de-allocate memory for the inter clusters outgoing event queue
    if (m_interClustersOutgoingEvents != NULL) {
        delete[] m_interClustersOutgoingEvents;
        m_interClustersOutgoingEvents = NULL;
    }

    // de-allocate memory for the inter clusters incoming event queue
    if (m_interClustersIncomingEvents != NULL) {
        delete[] m_interClustersIncomingEvents;
        m_interClustersIncomingEvents = NULL;
    }
#endif // USE_GPU
}

#if !defined(USE_GPU)
/*
 * Initializes the collection of queue.
 * 
 * @param clusterID The cluster ID of cluster to be initialized.
 * @param nMaxEvent The number of event queue.
 */
void EventQueue::initEventQueue(CLUSTER_INDEX_TYPE clusterID, BGSIZE nMaxEvent)
{
    m_clusterID = clusterID;

    // allocate & initialize a memory for the event queue
    m_nMaxEvent = nMaxEvent;
    m_queueEvent = new BGQUEUE_ELEMENT[nMaxEvent];
}

#else // USE_GPU
/*
 * Initializes the collection of queue in device memory.
 * 
 * @param clusterID                 The cluster ID of cluster to be initialized.
 * @param nMaxEvent                 The number of event queue.
 * @param pQueueEvent               Pointer to the collection of event queue.
 * @param nMaxInterClustersOutgoingEvents   The maximum number of the inter clusters 
 *                                          outgoing event queue.
 * @param interClustersOutgoingEvents       Pointer to the inter clusters outgoing event queue.
 * @param nMaxInterClustersIncomingEvents   The maximum number of the inter clusters 
 *                                          incoming event queue.
 * @param interClustersIncomingEvents       Pointer to the inter clusters incoming event queue.
 */
__device__ void EventQueue::initEventQueue(CLUSTER_INDEX_TYPE clusterID, BGSIZE nMaxEvent, BGQUEUE_ELEMENT* pQueueEvent, BGSIZE nMaxInterClustersOutgoingEvents, OUTGOING_SYNAPSE_INDEX_TYPE* interClustersOutgoingEvents, BGSIZE nMaxInterClustersIncomingEvents, BGSIZE* interClustersIncomingEvents)
{
    m_clusterID = clusterID;

    // initialize a memory for the event queue
    m_nMaxEvent = nMaxEvent;
    m_queueEvent = pQueueEvent;

    // initialize a memory for the inter clusters outgoing event queue
    m_nMaxInterClustersOutgoingEvents = nMaxInterClustersOutgoingEvents;
    m_interClustersOutgoingEvents = interClustersOutgoingEvents;

    // initialize a memory for the inter clusters incoming event queue
    m_nMaxInterClustersIncomingEvents = nMaxInterClustersIncomingEvents;
    m_interClustersIncomingEvents = interClustersIncomingEvents;
}

/*
 * Initializes the collection of queue in host memory.
 * 
 * @param clusterID                 The cluster ID of cluster to be initialized.
 * @param nMaxEvent                 The number of event queue.
 * @param nMaxInterClustersOutgoingEvents   The maximum number of the inter clusters 
 *                                          outgoing event queue.
 * @param nMaxInterClustersIncomingEvents   The maximum number of the inter clusters 
 *                                          incoming event queue.
 */
__host__ void EventQueue::initEventQueue(CLUSTER_INDEX_TYPE clusterID, BGSIZE nMaxEvent, BGSIZE nMaxInterClustersOutgoingEvents, BGSIZE nMaxInterClustersIncomingEvents)
{
    m_clusterID = clusterID;

    // allocate & initialize a memory for the event queue
    m_nMaxEvent = nMaxEvent;
    m_queueEvent = new BGQUEUE_ELEMENT[nMaxEvent];

    // initialize a memory for the inter clusters outgoing event queue
    m_nMaxInterClustersOutgoingEvents = nMaxInterClustersOutgoingEvents;
    if (nMaxInterClustersOutgoingEvents != 0) {
        m_interClustersOutgoingEvents = new OUTGOING_SYNAPSE_INDEX_TYPE[nMaxInterClustersOutgoingEvents];
    }

    // initialize a memory for the inter clusters incoming event queue
    m_nMaxInterClustersIncomingEvents = nMaxInterClustersIncomingEvents;
    if (nMaxInterClustersIncomingEvents != 0) {
        m_interClustersIncomingEvents = new BGSIZE[nMaxInterClustersIncomingEvents];
    }
}
#endif // USE_GPU

/*
 * Add an event in the queue.
 * 
 * @param idx The queue index of the collection.
 * @param clusterID The cluster ID where the event to be added.
 */
CUDA_CALLABLE void EventQueue::addAnEvent(const BGSIZE idx, const CLUSTER_INDEX_TYPE clusterID)
{
    if (clusterID != m_clusterID) {
#if !defined(USE_GPU)
        // notify the event to other cluster
        assert( m_eventHandler != NULL );
        m_eventHandler->addAnEvent(idx, clusterID);
#else // USE_GPU
        // save the inter clusters outgoing events
        assert( m_nInterClustersOutgoingEvents < m_nMaxInterClustersOutgoingEvents );
        m_interClustersOutgoingEvents[m_nInterClustersOutgoingEvents++] = SynapseIndexMap::getOutgoingSynapseIndex(clusterID, idx);
#endif // USE_GPU
    } else {
        BGQUEUE_ELEMENT &queue = m_queueEvent[idx];

        // Add to event queue

        // set a spike
        assert( !(queue & (0x1 << m_idxQueue)) );
        queue |= (0x1 << m_idxQueue);
    }
}

#if defined(USE_GPU)
/*
 * Add an event in the inter clusters incoming event queue
 *
 * @param idx The queue index of the collection.
 */
__host__ void EventQueue::addAnInterClustersIncomingEvent(const BGSIZE idx)
{
    assert( m_nInterClustersIncomingEvents < m_nMaxInterClustersIncomingEvents );
    m_interClustersIncomingEvents[m_nInterClustersIncomingEvents++] = idx;
}

/*
 * Process inter clusters outgoing events that are stored in the buffer.
 *
 * @param pEventQueue   Pointer to the EventQueue object in device.
 */
__host__ void EventQueue::processInterClustersOutgoingEvents(EventQueue* pEventQueue)
{
    // copy preSpikeQueue data from device to host.
    BGQUEUE_ELEMENT *pInterClustersOutgoingEvents_h;
    BGSIZE nInterClustersOutgoingEvents_h;

    // get event queue data in device
    getInterClustersOutgoingEventPointerInDevice(pEventQueue, &pInterClustersOutgoingEvents_h);
    getNInterClustersOutgoingEventsInDevice(pEventQueue, &nInterClustersOutgoingEvents_h);

    // set the queue index in host
    m_nInterClustersOutgoingEvents = nInterClustersOutgoingEvents_h;

    // copy the inter clusters outgoing event queue data to host
    checkCudaErrors( cudaMemcpy ( m_interClustersOutgoingEvents, pInterClustersOutgoingEvents_h, nInterClustersOutgoingEvents_h * sizeof( OUTGOING_SYNAPSE_INDEX_TYPE ), cudaMemcpyDeviceToHost ) );

    for (BGSIZE i = 0; i < m_nInterClustersOutgoingEvents; i++) {
        OUTGOING_SYNAPSE_INDEX_TYPE idx = m_interClustersOutgoingEvents[i];
        CLUSTER_INDEX_TYPE iCluster = SynapseIndexMap::getClusterIndex(idx);
        BGSIZE iSyn = SynapseIndexMap::getSynapseIndex(idx);
        assert( iCluster != m_clusterID);

        // notify the event to other cluster
        m_eventHandler->addAnEvent(iSyn, iCluster);
    }

    m_nInterClustersOutgoingEvents = 0;
}

/*
 * Process inter clusters incoming events that are stored in the buffer.
 *
 * @param pEventQueue   Pointer to the EventQueue object in device.
 */
__host__ void EventQueue::processInterClustersIncomingEvents(EventQueue* pEventQueue)
{
    // copy preSpikeQueue from host to device
    BGQUEUE_ELEMENT *pInterClustersIncomingEvents_h;

    // get the buffer pointer in device
    getInterClustersIncomingEventPointerInDevice(pEventQueue, &pInterClustersIncomingEvents_h);

    // copy the inter clusters incoming event queue data from host to device
    checkCudaErrors( cudaMemcpy ( pInterClustersIncomingEvents_h, m_interClustersIncomingEvents, m_nInterClustersIncomingEvents * sizeof( BGSIZE ), cudaMemcpyHostToDevice ) );

    // set the number of events stored in the inter clusters outgoing/incoming event queue to device
    setNInterClustersOutgoingEventsDevice <<< 1, 1 >>> ( pEventQueue, m_nInterClustersOutgoingEvents );
    setNInterClustersIncomingEventsDevice <<< 1, 1 >>> ( pEventQueue, m_nInterClustersIncomingEvents );

    // process inter clusters incoming spikes -- call device side preSpikeQueue object
    processInterClustersIncomingEventsDevice <<< 1, 1 >>> (pEventQueue);

    // reset the host side number of incoming events
    m_nInterClustersIncomingEvents = 0;
}

/*
 * Process inter clusters incoming events that are stored in the buffer.
 */
__device__ void EventQueue::processInterClustersIncomingEventsInDevice()
{
    for (BGSIZE i = 0; i < m_nInterClustersIncomingEvents; i++) {
        BGSIZE iSyn = m_interClustersIncomingEvents[i];

        BGQUEUE_ELEMENT &queue = m_queueEvent[iSyn];

        // Add to event queue

        // set a spike
        assert( !(queue & (0x1 << m_idxQueue)) );
        queue |= (0x1 << m_idxQueue);
    }

    m_nInterClustersIncomingEvents = 0;
}

#endif // USE_GPU

/*
 * Add an event in the queue.
 * 
 * @param idx The queue index of the collection.
 *e @param delay The delay descretized into time steps when the event will be triggered.
 */
CUDA_CALLABLE void EventQueue::addAnEvent(const BGSIZE idx, const int delay)
{
    BGQUEUE_ELEMENT &queue = m_queueEvent[idx];

    // Add to event queue

    // calculate index where to insert the event into queueEvent
    uint32_t idxQueue = m_idxQueue + delay;
    if ( idxQueue >= LENGTH_OF_DELAYQUEUE ) {
        idxQueue -= LENGTH_OF_DELAYQUEUE;
    }

    // set a spike
    assert( !(queue & (0x1 << idxQueue)) );
    queue |= (0x1 << idxQueue);
}

/*
 * Checks if there is an event in the queue.
 * 
 * @param idx The queue index of the collection.
 * @return true if there is an event.
 */
CUDA_CALLABLE bool EventQueue::checkAnEvent(const BGSIZE idx)
{
    BGQUEUE_ELEMENT &queue = m_queueEvent[idx];

    // check and reset the event
    bool r = queue & (0x1 << m_idxQueue);
    queue &= ~(0x1 << m_idxQueue);

    return r;
}

/*
 * Checks if there is an event in the queue.
 * 
 * @param idx The queue index of the collection.
 * @param delay The delay descretized into time steps when the event will be triggered.
 * @return true if there is an event.
 */
CUDA_CALLABLE bool EventQueue::checkAnEvent(const BGSIZE idx, const int delay)
{
    BGQUEUE_ELEMENT &queue = m_queueEvent[idx];

    // check and reset the event

    // calculate index where to check if there is an event
    int idxQueue = m_idxQueue - delay;
    if ( idxQueue < 0 ) {
        idxQueue += LENGTH_OF_DELAYQUEUE;
    }

    // check and reset a spike
    bool r = queue & (0x1 << idxQueue);
    queue &= ~(0x1 << idxQueue);

    return r;
}

/*
 * Clears events in the queue.
 * 
 * @param idx The queue index of the collection.
 */
CUDA_CALLABLE void EventQueue::clearAnEvent(const BGSIZE idx)
{
    BGQUEUE_ELEMENT &queue = m_queueEvent[idx];

    queue = 0;
}

/*
 * Advance one simulation step.
 * 
 */
CUDA_CALLABLE void EventQueue::advanceEventQueue()
{
    if ( ++m_idxQueue >= LENGTH_OF_DELAYQUEUE ) {
        m_idxQueue = 0;
    }
}

/*
 * Register an event handler.
 *
 * @param eventHandler  Pointer to the InterClustersEventHandler.
 */
CUDA_CALLABLE void EventQueue::regEventHandler(InterClustersEventHandler* eventHandler)
{
    m_eventHandler = eventHandler;
}

/*
 * Writes the queue data to the stream.
 * We don't need to save inter clusters event data, because
 * these data are temporary used between advanceNeurons() and advanceSynapses().
 *
 * output  stream to print out to.
 */
void EventQueue::serialize(ostream& output)
{
    output << m_idxQueue << ends;
    output << m_nMaxEvent << ends;

    for (BGSIZE idx = 0; idx < m_nMaxEvent; idx++) {
        output << m_queueEvent[idx] << ends;
    }
}

/*
 * Sets the data for the queue to input's data.
 *
 * input istream to read from.
 */
void EventQueue::deserialize(istream& input)
{
    BGSIZE nMaxEvent;

    input >> m_idxQueue; input.ignore();
    input >> nMaxEvent; input.ignore();

    // If the assertion hits, that means we restore simulation by using stored data
    // of different configuration file.
    assert( m_nMaxEvent == nMaxEvent );
    m_nMaxEvent = nMaxEvent;

    for (BGSIZE idx = 0; idx < m_nMaxEvent; idx++) {
        input >> m_queueEvent[idx]; input.ignore();
    }
}

#if defined(USE_GPU)
/*
 * Create an EventQueue class object in device
 *
 * @param pEventQueue_d    Device memory address to save the pointer of created EventQueue object.
 */
__host__ void EventQueue::createEventQueueInDevice(EventQueue** pEventQueue_d)
{
    EventQueue **pEventQueue_t; // temporary buffer to save pointer to EventQueue object.

    // allocate device memory for the buffer.
    checkCudaErrors( cudaMalloc( ( void ** ) &pEventQueue_t, sizeof( EventQueue * ) ) );

    // allocate three device memories used in the EventQueue
    BGSIZE nMaxEvent = m_nMaxEvent;
    BGQUEUE_ELEMENT* queueEvent;
    checkCudaErrors( cudaMalloc(&queueEvent, nMaxEvent * sizeof(BGQUEUE_ELEMENT)) );

    BGSIZE nMaxInterClustersOutgoingEvents = m_nMaxInterClustersOutgoingEvents;
    OUTGOING_SYNAPSE_INDEX_TYPE* interClustersOutgoingEvents = NULL;
    if (nMaxInterClustersOutgoingEvents != 0) {
        checkCudaErrors( cudaMalloc(&interClustersOutgoingEvents, nMaxInterClustersOutgoingEvents * sizeof(OUTGOING_SYNAPSE_INDEX_TYPE)) );
    }

    BGSIZE nMaxInterClustersIncomingEvents = m_nMaxInterClustersIncomingEvents;
    BGSIZE* interClustersIncomingEvents = NULL;
    if (nMaxInterClustersIncomingEvents != 0) {
        checkCudaErrors( cudaMalloc(&interClustersIncomingEvents, nMaxInterClustersIncomingEvents * sizeof(BGSIZE)) );
    }

    // create a EventQueue object in device memory.
    allocEventQueueDevice <<< 1, 1 >>> ( pEventQueue_t, m_clusterID, nMaxEvent, queueEvent, nMaxInterClustersOutgoingEvents, interClustersOutgoingEvents, nMaxInterClustersIncomingEvents, interClustersIncomingEvents);

    // save the pointer of the object.
    checkCudaErrors( cudaMemcpy ( pEventQueue_d, pEventQueue_t, sizeof( EventQueue * ), cudaMemcpyDeviceToHost ) );

    // free device memory for the buffer.
    checkCudaErrors( cudaFree( pEventQueue_t ) );
}

/*
 * Delete an EventQueue class object in device
 *
 * @param pEventQueue_d    Pointer to the EventQueue object to be deleted in device.
 */
void EventQueue::deleteEventQueueInDevice(EventQueue* pEventQueue_d)
{
    BGQUEUE_ELEMENT *pQueueEvent_h;
    BGQUEUE_ELEMENT *pInterClustersOutgoingEvents_h;
    BGSIZE *pInterClustersIncomingEvents_h;

    // get pointers to the queue data buffers in device memory
    getQueueEventPointerInDevice(pEventQueue_d, &pQueueEvent_h);
    getInterClustersOutgoingEventPointerInDevice(pEventQueue_d, &pInterClustersOutgoingEvents_h);
    getInterClustersIncomingEventPointerInDevice(pEventQueue_d, &pInterClustersIncomingEvents_h);

    // free three device memories used in the EventQueue
    checkCudaErrors( cudaFree( pQueueEvent_h ) );
    if (pInterClustersOutgoingEvents_h != NULL) {
        checkCudaErrors( cudaFree( pInterClustersOutgoingEvents_h ) );
    }
    if (pInterClustersIncomingEvents_h != NULL) {
        checkCudaErrors( cudaFree( pInterClustersIncomingEvents_h ) );
    }

    // delete preSpikeQueue object in device memory.
    deleteEventQueueDevice <<< 1, 1 >>> ( pEventQueue_d );
}

/*
 * Copy EventQueue data from host to device
 *
 * @param pEventQueue   Pointer to the EventQueue object in device.
 */
__host__ void EventQueue::copyEventQueueHostToDevice(EventQueue* pEventQueue)
{
    // We need deep copy event queue data here, when we resume simulation by
    // restoring the previous status data.
    // However we don't need to copy inter clusters event data, because
    // these data are temporary used between advanceNeurons() and advanceSynapses().
    BGQUEUE_ELEMENT *pQueueEvent_h;
    getQueueEventPointerInDevice(pEventQueue, &pQueueEvent_h);

    // copy event queue data from host to device (deep copy)
    checkCudaErrors( cudaMemcpy ( pQueueEvent_h, m_queueEvent,
            m_nMaxEvent * sizeof( BGQUEUE_ELEMENT ), cudaMemcpyHostToDevice ) );

    // set queue index to device
    setQueueIndexDevice <<< 1, 1 >>> ( pEventQueue, m_idxQueue );
}

/*
 * Copy EventQueue data from device to host
 *
 * @param pEventQueue   Pointer to the EventQueue object in device.
 */
__host__ void EventQueue::copyEventQueueDeviceToHost(EventQueue* pEventQueue)
{
    // We need deep copy event queue data here, when we store simulation status.
    // However we don't need to copy inter clusters event data, because
    // these data are temporary used between advanceNeurons() and advanceSynapses().
    BGQUEUE_ELEMENT *pQueueEvent_h;
    getQueueEventPointerInDevice(pEventQueue, &pQueueEvent_h);

    // copy event queue data from device to host (deep copy)
    checkCudaErrors( cudaMemcpy ( pQueueEvent_h, m_queueEvent,
            m_nMaxEvent * sizeof( BGQUEUE_ELEMENT ), cudaMemcpyHostToDevice ) );

    // set queue index to host
    uint32_t idxQueue_h;
    EventQueue::getQueueIndexInDevice(pEventQueue, &idxQueue_h);

    // set the queue index in host
    m_idxQueue = idxQueue_h;
}

/*
 * Get index indicating the current time slot in the delayed queue in device
 *
 * @param pEventQueue   Pointer to the EventQueue object in device.
 * @param idxQueue_h    Address to the data to get.
 */
void EventQueue::getQueueIndexInDevice(EventQueue* pEventQueue, uint32_t* idxQueue_h)
{
    uint32_t *pIdxQueue_d;  // temporary buffer to save queue index

    // allocate device memory for the buffer.
    checkCudaErrors( cudaMalloc( ( void ** ) &pIdxQueue_d, sizeof( uint32_t ) ) );

    // get queue index in device memory
    getQueueIndexDevice <<< 1, 1 >>> ( pEventQueue, pIdxQueue_d );

   // copy the queue index to host.
    checkCudaErrors( cudaMemcpy ( idxQueue_h, pIdxQueue_d, sizeof( uint32_t ), cudaMemcpyDeviceToHost ) );

    // free device memory for the buffer.
    checkCudaErrors( cudaFree( pIdxQueue_d ) );
}

/*
 * Get pointer to the collection of event queue in device
 *
 * @param pEventQueue     Pointer to the EventQueue object in device.
 * @param pQueueEvent_h   Address to the data to get.
 */
void EventQueue::getQueueEventPointerInDevice(EventQueue* pEventQueue, BGQUEUE_ELEMENT** pQueueEvent_h) 
{
    BGQUEUE_ELEMENT **pQueueEvent_d; // temporary buffer to save pointer to event queue data

    // allocate device memory for the buffer.
    checkCudaErrors( cudaMalloc( ( void ** ) &pQueueEvent_d, sizeof( BGQUEUE_ELEMENT* ) ) );

    // get the address of event queue data in device memory
    // EventQueue object in device memory resides in device heap,
    // so we can not use cudaMemcpy to copy the object to host memory,
    // and therefore use the kernel function to get the pointer.

    getQueueEventPointerDevice <<< 1, 1 >>> ( pEventQueue, pQueueEvent_d );

    // copy the pointer of the object to host.
    checkCudaErrors( cudaMemcpy ( pQueueEvent_h, pQueueEvent_d, sizeof( BGQUEUE_ELEMENT* ), cudaMemcpyDeviceToHost ) );

    // free device memory for the buffer.
    checkCudaErrors( cudaFree( pQueueEvent_d ) );
}

/*
 * Get number of events stored in the inter clusters outgoing event queue in device
 *
 * @param pEventQueue     Pointer to the EventQueue object in device.
 * @param nInterClustersOutgoingEvents_h   Address to the data to get.
 */
void EventQueue::getNInterClustersOutgoingEventsInDevice(EventQueue* pEventQueue, BGSIZE* nInterClustersOutgoingEvents_h)
{
    BGSIZE *pNInterClustersOutgoingEvents_d; // temporary buffer to save the number

    // allocate device memory for the buffer.
    checkCudaErrors( cudaMalloc( ( void ** ) &pNInterClustersOutgoingEvents_d, sizeof( BGSIZE ) ) );

    // get the number in device memory
    getNInterClustersOutgoingEventsDevice <<< 1, 1 >>> ( pEventQueue, pNInterClustersOutgoingEvents_d );

    // copy the queue index to host.
    checkCudaErrors( cudaMemcpy ( nInterClustersOutgoingEvents_h, pNInterClustersOutgoingEvents_d, sizeof( BGSIZE ), cudaMemcpyDeviceToHost ) );

    // free device memory for the buffer.
    checkCudaErrors( cudaFree( pNInterClustersOutgoingEvents_d ) );
}

/*
 * Get pointer to the inter clusters outgoing event queue in device
 *
 * @param pEventQueue     Pointer to the EventQueue object in device.
 * @param pInterClustersOutgoingEvents_h   Address to the data to get.
 */
void EventQueue::getInterClustersOutgoingEventPointerInDevice(EventQueue* pEventQueue, BGQUEUE_ELEMENT** pInterClustersOutgoingEvents_h)
{
    OUTGOING_SYNAPSE_INDEX_TYPE **pInterClustersOutgoingEvents_d; // temporary buffer to save pointer to inter clusters outgoing event queue data

    // allocate device memory for the buffer
    checkCudaErrors( cudaMalloc( ( void ** ) &pInterClustersOutgoingEvents_d, sizeof( OUTGOING_SYNAPSE_INDEX_TYPE * ) ) );

    // get the address of inter clusters outgoing event queue data in device memory
    // EventQueue object in device memory resides in device heap, 
    // so we can not use cudaMemcpy to copy the object to host memory,
    // and therefore use the kernel function to get the pointer.

    getInterClustersOutgoingEventPointerDevice <<< 1, 1 >>> ( pEventQueue, pInterClustersOutgoingEvents_d );

    // copy the pointer of the object to host.
    checkCudaErrors( cudaMemcpy ( pInterClustersOutgoingEvents_h, pInterClustersOutgoingEvents_d, sizeof( OUTGOING_SYNAPSE_INDEX_TYPE* ), cudaMemcpyDeviceToHost ) );

    // free device memory for the buffer.
    checkCudaErrors( cudaFree( pInterClustersOutgoingEvents_d ) );
}

/*
 * Get pointer to the inter clusters incoming event queue in device
 *
 * @param pEventQueue     Pointer to the EventQueue object in device.
 * @param pInterClustersIncomingEvents_h   Address to the data to get.
 */
void EventQueue::getInterClustersIncomingEventPointerInDevice(EventQueue* pEventQueue, BGQUEUE_ELEMENT** pInterClustersIncomingEvents_h)
{
    BGSIZE **pInterClustersIncomingEvents_d; // temporary buffer to save pointer to inter clusters incoming event queue data

    // allocate device memory for the buffer
    checkCudaErrors( cudaMalloc( ( void ** ) &pInterClustersIncomingEvents_d, sizeof( BGSIZE * ) ) );

    // get the address of inter clusters incoming event queue data in device memory
    // EventQueue object in device memory resides in device heap, 
    // so we can not use cudaMemcpy to copy the object to host memory,
    // and therefore use the kernel function to get the pointer.

    getInterClustersIncomingEventPointerDevice <<< 1, 1 >>> ( pEventQueue, pInterClustersIncomingEvents_d );

    // copy the pointer of the object to host.
    checkCudaErrors( cudaMemcpy ( pInterClustersIncomingEvents_h, pInterClustersIncomingEvents_d, sizeof( BGSIZE* ), cudaMemcpyDeviceToHost ) );

    // free device memory for the buffer.
    checkCudaErrors( cudaFree( pInterClustersIncomingEvents_d ) );
}


/* -------------------------------------*\
|* # CUDA Global Functions
\* -------------------------------------*/

/*
 * Creates a EventQueue object in device memory.
 *
 * @param[in/out] pQueueEvent           Pointer to the collection of event queue.
 * @param[in] clusterID                 The cluster ID of cluster to be initialized.
 * @param[in] nMaxEvent                 The number of event queue.
 * @param[in] pQueueEvent               Pointer to the collection of event queue.
 * @param[in] nMaxInterClustersOutgoingEvents   The maximum number of the inter clusters
 *                                              outgoing event queue.
 * @param[in] interClustersOutgoingEvents       Pointer to the inter clusters outgoing event queue.
 * @param[in] nMaxInterClustersIncomingEvents   The maximum number of the inter clusters
 *                                              incoming event queue.
 * @param[in] interClustersIncomingEvents       Pointer to the inter clusters incoming event queue.
 */
__global__ void allocEventQueueDevice(EventQueue **pEventQueue, CLUSTER_INDEX_TYPE clusterID, BGSIZE nMaxEvent, BGQUEUE_ELEMENT* pQueueEvent, BGSIZE nMaxInterClustersOutgoingEvents, OUTGOING_SYNAPSE_INDEX_TYPE* interClustersOutgoingEvents, BGSIZE nMaxInterClustersIncomingEvents, BGSIZE* interClustersIncomingEvents)
{
    *pEventQueue = new EventQueue();
    (*pEventQueue)->initEventQueue(clusterID, nMaxEvent, pQueueEvent, nMaxInterClustersOutgoingEvents, interClustersOutgoingEvents, nMaxInterClustersIncomingEvents, interClustersIncomingEvents);
}

/*
 * Delete a EventQueue object in device memory.
 *
 * @param[in] pEventQueue          Pointer to the EventQueue object to be deleted.
 */
__global__ void deleteEventQueueDevice(EventQueue *pEventQueue)
{
    if (pEventQueue != NULL) {
        // event queue buffers were freed by calling cudaFree()
        pEventQueue->m_queueEvent = NULL;
        pEventQueue->m_interClustersOutgoingEvents = NULL;
        pEventQueue->m_interClustersIncomingEvents = NULL;

        delete pEventQueue;
    }
}
/*
 * Get the address of event queue data in device memory
 *
 * @param[in] pEventQueue          Pointer to the EventQueue object.
 * @param[in/out] pQueueEvent      Buffer to save pointer to event queue data.
 */
__global__ void getQueueEventPointerDevice(EventQueue *pEventQueue, BGQUEUE_ELEMENT **pQueueEvent)
{
    *pQueueEvent = pEventQueue->m_queueEvent;
}

/*
 * Set queue index to device
 *
 * @param[in] pEventQueue          Pointer to the EventQueue object.
 * @param[in] idxQueue             Queue index.
 */
__global__ void setQueueIndexDevice(EventQueue *pEventQueue, uint32_t idxQueue)
{
    pEventQueue->m_idxQueue = idxQueue;
}

/*
 * get queue index in device memory
 *
 * @param[in] pEventQueue          Pointer to the EventQueue object.
 * @param[in/out] idxQueue         Buffer to save Queue index.
 */
__global__ void getQueueIndexDevice(EventQueue *pEventQueue, uint32_t *idxQueue)
{
    *idxQueue = pEventQueue->m_idxQueue;
}

/*
 * get the address of inter clusters outgoing event queue data in device memory
 *
 * @param[in] pEventQueue                        Pointer to the EventQueue object.
 * @param[in/out] pInterClustersOutgoingEvents   Buffer to save pointer to inter clusters outgoing
 *                                               event queue data.
 */
__global__ void getInterClustersOutgoingEventPointerDevice(EventQueue *pEventQueue, OUTGOING_SYNAPSE_INDEX_TYPE **pInterClustersOutgoingEvents)
{
    *pInterClustersOutgoingEvents = pEventQueue->m_interClustersOutgoingEvents;
}

/*
 * get the address of inter clusters incoming event queue data in device memory
 *
 * @param[in] pEventQueue                        Pointer to the EventQueue object.
 * @param[in/out] pInterClustersIncomingEvents   Buffer to save pointer to inter clusters incoming
 *                                               event queue data.
 */
__global__ void getInterClustersIncomingEventPointerDevice(EventQueue *pEventQueue, BGSIZE **pInterClustersIncomingEvents)
{
    *pInterClustersIncomingEvents = pEventQueue->m_interClustersIncomingEvents;
}

/*
 * get the number events stored in the inter clusters outgoing event queue in device memory
 *
 * @param[in] pEventQueue                         Pointer to the EventQueue object.
 * @param[in/out] pNInterClustersOutgoingEvents   Buffer to save the number.
 */
__global__ void getNInterClustersOutgoingEventsDevice(EventQueue *pEventQueue, BGSIZE *pNInterClustersOutgoingEvents)
{
    *pNInterClustersOutgoingEvents = pEventQueue->m_nInterClustersOutgoingEvents;
}

/*
 * set the number events stored in the inter clusters outgoing event queue in device memory
 *
 * @param[in] pEventQueue                         Pointer to the EventQueue object.
 * @param[in] nInterClustersOutgoingEvents        The number events in the queue..
 */
__global__ void setNInterClustersOutgoingEventsDevice(EventQueue *pEventQueue, BGSIZE nInterClustersOutgoingEvents)
{
    pEventQueue->m_nInterClustersOutgoingEvents = nInterClustersOutgoingEvents;
}

/*
 * set the number events stored in the inter clusters incoming event queue in device memory
 *
 * @param[in] pEventQueue                         Pointer to the EventQueue object.
 * @param[in] nInterClustersIncomingEvents        The number events in the queue..
 */
__global__ void setNInterClustersIncomingEventsDevice(EventQueue *pEventQueue, BGSIZE nInterClustersIncomingEvents)
{
    pEventQueue->m_nInterClustersIncomingEvents = nInterClustersIncomingEvents;
}

/*
 * Process inter clusters incoming events that are stored in the buffer.
 */
__global__ void processInterClustersIncomingEventsDevice(EventQueue *pEventQueue)
{
    pEventQueue->processInterClustersIncomingEventsInDevice();
}

#endif // USE_GPU

