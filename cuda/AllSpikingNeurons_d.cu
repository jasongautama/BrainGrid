/*
 * AllSpikingNeurons_d.cu
 *
 */

#include "AllSpikingNeurons.h"
#include "AllSpikingSynapses.h"
#include "Book.h"

void AllSpikingNeurons::copyDeviceSpikeHistoryToHost( AllSpikingNeurons& allNeurons, const SimulationInfo *sim_info ) 
{
        int numNeurons = sim_info->totalNeurons;
        uint64_t* pSpikeHistory[numNeurons];
        HANDLE_ERROR( cudaMemcpy ( pSpikeHistory, allNeurons.spike_history, numNeurons * sizeof( uint64_t* ), cudaMemcpyDeviceToHost ) );

        int max_spikes = static_cast<int> (sim_info->epochDuration * sim_info->maxFiringRate);
        for (int i = 0; i < numNeurons; i++) {
                HANDLE_ERROR( cudaMemcpy ( spike_history[i], pSpikeHistory[i],
                        max_spikes * sizeof( uint64_t ), cudaMemcpyDeviceToHost ) );
        }
}


void AllSpikingNeurons::copyDeviceSpikeCountsToHost( AllSpikingNeurons& allNeurons, const SimulationInfo *sim_info ) 
{
        int numNeurons = sim_info->totalNeurons;

        HANDLE_ERROR( cudaMemcpy ( spikeCount, allNeurons.spikeCount, numNeurons * sizeof( int ), cudaMemcpyDeviceToHost ) );
        HANDLE_ERROR( cudaMemcpy ( spikeCountOffset, allNeurons.spikeCountOffset, numNeurons * sizeof( int ), cudaMemcpyDeviceToHost ) );
}


void AllSpikingNeurons::clearDeviceSpikeCounts( AllSpikingNeurons& allNeurons, const SimulationInfo *sim_info ) 
{
        int numNeurons = sim_info->totalNeurons;

        HANDLE_ERROR( cudaMemset( allNeurons.spikeCount, 0, numNeurons * sizeof( int ) ) );
        HANDLE_ERROR( cudaMemcpy ( allNeurons.spikeCountOffset, spikeCountOffset, numNeurons * sizeof( int ), cudaMemcpyHostToDevice ) );
}

void AllSpikingNeurons::setAdvanceNeuronsDeviceParams(AllSynapses &synapses)
{
    AllSpikingSynapses &spSynapses = dynamic_cast<AllSpikingSynapses&>(synapses);
    m_fAllowBackPropagation = spSynapses.allowBackPropagation();

    m_fpPreSpikeHit_h = NULL;
    m_fpPostSpikeHit_h = NULL;
    spSynapses.getFpPreSpikeHit(m_fpPreSpikeHit_h);
    if (m_fAllowBackPropagation) {
        spSynapses.getFpPostSpikeHit(m_fpPostSpikeHit_h);
    }
}