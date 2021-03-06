/*
 *      \file GpuSInputPoisson.cu
 *
 *      \author Fumitaka Kawasaki
 *
 *      \brief A class that performs stimulus input (implementation Poisson) on GPU.
 */

#include "GpuSInputPoisson.h"
#include <helper_cuda.h>

/*
 * constructor
 *
 * @param[in] psi       Pointer to the simulation information
 * @param[in] parms     TiXmlElement to examine.
 */
GpuSInputPoisson::GpuSInputPoisson(SimulationInfo* psi, TiXmlElement* parms) : SInputPoisson(psi, parms)
{
}

/*
 * destructor
 */
GpuSInputPoisson::~GpuSInputPoisson()
{
}

/*
 * Initialize data.
 *
 * @param[in] psi             Pointer to the simulation information.
 * @param[in] vtClrInfo       Vector of ClusterInfo.
 */
void GpuSInputPoisson::init(SimulationInfo* psi, vector<ClusterInfo *> &vtClrInfo)
{
    SInputPoisson::init(psi, vtClrInfo);

    if (m_fSInput == false)
        return;

    // allocate GPU device memory and copy values
    allocDeviceValues(psi, vtClrInfo, m_nISIs);
}

/*
 * Terminate process.
 *
 * @param[in] psi             Pointer to the simulation information.
 * @param[in] vtClrInfo       Vector of ClusterInfo.
 */
void GpuSInputPoisson::term(SimulationInfo* psi, vector<ClusterInfo *> &vtClrInfo)
{
    if (m_fSInput)
        deleteDeviceValues(vtClrInfo);

    SInputPoisson::term(psi, vtClrInfo);
}

/*
 * Process input stimulus for each time step.
 * Apply inputs on summationPoint.
 *
 * @param[in] psi             Pointer to the simulation information.
 * @param[in] pci             ClusterInfo class to read information from.
 * @param[in] iStepOffset     Offset from the current simulation step.
 */
void GpuSInputPoisson::inputStimulus(const SimulationInfo* psi, ClusterInfo *pci, int iStepOffset)
{
    if (m_fSInput == false)
        return;

    // Set device ID
    checkCudaErrors( cudaSetDevice( pci->deviceId ) );

    int neuron_count = pci->totalClusterNeurons;
    int synapse_count = pci->totalClusterNeurons;

    // CUDA parameters
    const int threadsPerBlock = 256;
    int blocksPerGrid = ( neuron_count + threadsPerBlock - 1 ) / threadsPerBlock;

    // add input spikes to each synapse
    inputStimulusDevice <<< blocksPerGrid, threadsPerBlock >>> ( neuron_count, pci->nISIs_d, pci->masks_d, psi->deltaT, m_lambda, pci->devStates_d, pci->synapsesPropsDeviceSInput, pci->clusterID, iStepOffset );

    // advance synapses
    int maxSpikes = (int)((psi->epochDuration * psi->maxFiringRate));
    advanceSynapsesDevice <<< blocksPerGrid, threadsPerBlock >>> ( synapse_count, pci->synapseIndexMapDeviceSInput, g_simulationStep, maxSpikes, psi->deltaT, iStepOffset, pci->synapsesDeviceSInput, NULL, NULL );

    // update summation point
    applyI2SummationMap <<< blocksPerGrid, threadsPerBlock >>> ( neuron_count, pci->pClusterSummationMap, pci->synapsesPropsDeviceSInput );
    
}

/*
 * Advance input stimulus state.
 *
 * @param[in] pci             ClusterInfo class to read information from.
 * @param[in] iStep           Simulation steps to advance.
 */
void GpuSInputPoisson::advanceSInputState(const ClusterInfo *pci, int iStep)
{
    // Advances synapses pre spike event queue state of the cluster iStep simulation step
    advanceSpikeQueueDevice <<< 1, 1 >>> (iStep, pci->synapsesDeviceSInput);
}

/*
 * Allocate GPU device memory and copy values
 *
 * @param[in] psi        Pointer to the simulation information.
 * @param[in] vtClrInfo  Vector of ClusterInfo.
 * @param[in] nISIs      Pointer to the interval counter.
 */
void GpuSInputPoisson::allocDeviceValues(SimulationInfo* psi, vector<ClusterInfo *> &vtClrInfo, int *nISIs )
{
    // for each cluster
    for (CLUSTER_INDEX_TYPE iCluster = 0; iCluster < vtClrInfo.size(); iCluster++) 
    {
        ClusterInfo *pci = vtClrInfo[iCluster];
        int neuron_count = pci->totalClusterNeurons;

        // Set device ID
        checkCudaErrors( cudaSetDevice( pci->deviceId ) );

        // Allocate GPU device memory
        BGSIZE nISIs_d_size = neuron_count * sizeof (int);   // size of shift values
        checkCudaErrors( cudaMalloc ( ( void ** ) &(pci->nISIs_d), nISIs_d_size ) );

        // Copy values into device memory
        int beginIdx = pci->clusterNeuronsBegin;
        checkCudaErrors( cudaMemcpy ( pci->nISIs_d, &nISIs[beginIdx], nISIs_d_size, cudaMemcpyHostToDevice ) );

        // create an input synapse layer in device
        AllSynapsesProps *pSynapsesProps = dynamic_cast<AllSynapses*>(pci->synapsesSInput)->m_pSynapsesProps;
        pSynapsesProps->setupSynapsesDeviceProps( (void **)&(pci->synapsesPropsDeviceSInput), neuron_count, 1 ); 
        pSynapsesProps->copySynapseHostToDeviceProps( pci->synapsesPropsDeviceSInput, neuron_count, 1 );

        const int threadsPerBlock = 256;
        int blocksPerGrid = ( neuron_count + threadsPerBlock - 1 ) / threadsPerBlock;

        initSynapsesDevice <<< blocksPerGrid, threadsPerBlock >>> ( pci->synapsesDeviceSInput, neuron_count, pci->synapsesPropsDeviceSInput, pci->pClusterSummationMap, psi->width, psi->deltaT, m_weight );

        // allocate memory for curand global state
        checkCudaErrors( cudaMalloc ( &(pci->devStates_d), neuron_count * sizeof( curandState ) ) );

        // allocate memory for synapse index map and initialize it
        SynapseIndexMap synapseIndexMap;
        BGSIZE* incomingSynapseIndexMap = new BGSIZE[neuron_count];

        BGSIZE syn_i = 0;
        for (int i = 0; i < neuron_count; i++, syn_i++)
        {
            incomingSynapseIndexMap[i] = syn_i;
        }

        checkCudaErrors( cudaMalloc( ( void ** ) &synapseIndexMap.incomingSynapseIndexMap, neuron_count * sizeof( BGSIZE ) ) );
        checkCudaErrors( cudaMemcpy ( synapseIndexMap.incomingSynapseIndexMap, incomingSynapseIndexMap, neuron_count * sizeof( BGSIZE ), cudaMemcpyHostToDevice ) ); 
        checkCudaErrors( cudaMalloc( ( void ** ) &(pci->synapseIndexMapDeviceSInput), sizeof( SynapseIndexMap ) ) );
        checkCudaErrors( cudaMemcpy ( pci->synapseIndexMapDeviceSInput, &synapseIndexMap, sizeof( SynapseIndexMap ), cudaMemcpyHostToDevice ) );

        delete[] incomingSynapseIndexMap;

        // allocate memory for masks for stimulus input and initialize it
        checkCudaErrors( cudaMalloc ( &(pci->masks_d), neuron_count * sizeof( bool ) ) );
        checkCudaErrors( cudaMemcpy ( pci->masks_d, &m_masks[beginIdx], neuron_count * sizeof( bool ), cudaMemcpyHostToDevice ) ); 

        // setup seeds
        setupSeeds <<< blocksPerGrid, threadsPerBlock >>> ( neuron_count, pci->devStates_d, time(NULL) );

      // create an AllSynapses class object in device
      pci->synapsesSInput->createAllSynapsesInDevice(&(pci->synapsesDeviceSInput), pci->synapsesPropsDeviceSInput);
    }
}

/*
 * Dellocate GPU device memory
 *
 * @param[in] vtClrInfo  Vector of ClusterInfo.
 */
void GpuSInputPoisson::deleteDeviceValues(vector<ClusterInfo *> &vtClrInfo )
{
    // for each cluster
    for (CLUSTER_INDEX_TYPE iCluster = 0; iCluster < vtClrInfo.size(); iCluster++)
    {
        ClusterInfo *pci = vtClrInfo[iCluster];

        // Set device ID
        checkCudaErrors( cudaSetDevice( pci->deviceId ) );

        checkCudaErrors( cudaFree( pci->nISIs_d ) );
        checkCudaErrors( cudaFree( pci->devStates_d ) );
        checkCudaErrors( cudaFree( pci->masks_d ) );

        AllSynapsesProps *pSynapsesProps = dynamic_cast<AllSynapses*>(pci->synapsesSInput)->m_pSynapsesProps;
        pSynapsesProps->cleanupSynapsesDeviceProps( pci->synapsesPropsDeviceSInput );

        // deallocate memory for synapse index map
        SynapseIndexMap synapseIndexMap;
        checkCudaErrors( cudaMemcpy ( &synapseIndexMap, pci->synapseIndexMapDeviceSInput, sizeof( SynapseIndexMap ), cudaMemcpyDeviceToHost ) );
        checkCudaErrors( cudaFree( synapseIndexMap.incomingSynapseIndexMap ) );
        checkCudaErrors( cudaFree( pci->synapseIndexMapDeviceSInput ) );

        // delete an AllSynapses class object in device
        pci->synapsesSInput->deleteAllSynapsesInDevice(pci->synapsesDeviceSInput);
    }
}

// CUDA code for -----------------------------------------------------------------------

/*
 * Device code for adding input values to the summation map.
 *
 * @param[in] nISIs_d            Pointer to the interval counter.
 * @param[in] masks_d            Pointer to the input stimulus masks.
 * @param[in] deltaT             Time step of the simulation in second.
 * @param[in] lambda             Iinverse firing rate.
 * @param[in] devStates_d        Curand global state
 * @param[in] allSynapsesProps  Pointer to Synapse structures in device memory.
 * @param[in] clusterID          Cluster ID.
 * @param[in] iStepOffset        Offset from the current simulation step.
 */
__global__ void inputStimulusDevice( int n, int* nISIs_d, bool* masks_d, BGFLOAT deltaT, BGFLOAT lambda, curandState* devStates_d, AllDSSynapsesProps* allSynapsesProps, CLUSTER_INDEX_TYPE clusterID, int iStepOffset )
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if ( idx >= n )
        return;

    if (masks_d[idx] == false)
        return;

    BGSIZE iSyn = idx;

    int rnISIs = nISIs_d[idx];    // load the value to a register
    if (--rnISIs <= 0)
    {
        // add a spike
        allSynapsesProps->preSpikeQueue->addAnEvent(iSyn, clusterID, iStepOffset);

        // update interval counter (exponectially distribution ISIs, Poisson)
        curandState localState = devStates_d[idx];

        BGFLOAT isi = -lambda * log(curand_uniform( &localState ));
        // delete isi within refractoriness
        while (curand_uniform( &localState ) <= exp(-(isi*isi)/32))
            isi = -lambda * log(curand_uniform( &localState ));
        // convert isi from msec to steps
        rnISIs = static_cast<int>( (isi / 1000) / deltaT + 0.5 );
        devStates_d[idx] = localState;
    }
    nISIs_d[idx] = rnISIs;
}

/*
 * CUDA code for update summation point
 *
 * @param[in] n                  Number of neurons.
 * @param[in] summationPoint_d   SummationPoint
 * @param[in] allSynapsesProps  Pointer to Synapse structures in device memory.
 */
__global__ void applyI2SummationMap( int n, BGFLOAT* summationPoint_d, AllDSSynapsesProps* allSynapsesProps ) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if ( idx >= n )
            return;

    summationPoint_d[idx] += allSynapsesProps->psr[idx];
}

/*
 * CUDA code for setup curand seed
 *
 * @param[in] n                  Number of neurons.
 * @param[in] devStates_d        Curand global state
 * @param[in] seed               Seed
 */
__global__ void setupSeeds( int n, curandState* devStates_d, unsigned long seed )
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if ( idx >= n )
            return;

    curand_init( seed, idx, 0, &devStates_d[idx] );
} 

/*
 * Adds a synapse to the network.  Requires the locations of the source and
 * destination neurons.
 *
 * @param synapsesDevice         Pointer to the Synapses object in device memory.
 * @param allSynapsesProps       Pointer to the Synapse structures in device memory.
 * @param pSummationMap          Pointer to the summation point.
 * @param width                  Width of neuron map (assumes square).
 * @param deltaT                 The simulation time step size.
 * @param weight                 Synapse weight.
 */
__global__ void initSynapsesDevice( IAllSynapses* synapsesDevice, int n, AllDSSynapsesProps* allSynapsesProps, BGFLOAT *pSummationMap, int width, const BGFLOAT deltaT, BGFLOAT weight )
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if ( idx >= n )
        return;

    // create a synapse
    int neuron_index = idx;
    BGFLOAT* sum_point = &( pSummationMap[neuron_index] );
    synapseType type = allSynapsesProps->type[neuron_index];

    BGSIZE iSyn = allSynapsesProps->maxSynapsesPerNeuron * neuron_index;
    synapsesDevice->createSynapse(iSyn, 0, neuron_index, sum_point, deltaT, type);
    allSynapsesProps->W[neuron_index] = weight * AllSynapses::SYNAPSE_STRENGTH_ADJUSTMENT;
}

