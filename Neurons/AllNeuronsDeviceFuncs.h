#pragma once

#include "AllIZHNeurons.h"
#include "AllSTDPSynapses.h"

#if defined(__CUDACC__)

/**
 *  CUDA code for advancing LIF neurons
 *
 *  @param[in] totalNeurons          Number of neurons.
 *  @param[in] maxSynapses           Maximum number of synapses per neuron.
 *  @param[in] maxSpikes             Maximum number of spikes per neuron per epoch.
 *  @param[in] deltaT                Inner simulation step duration.
 *  @param[in] simulationStep        The current simulation step.
 *  @param[in] randNoise             Pointer to device random noise array.
 *  @param[in] allNeuronsProps       Pointer to Neuron structures in device memory.
 *  @param[in] allSynapsesProps      Pointer to Synapse structures in device memory.
 *  @param[in] synapseIndexMap       Inverse map, which is a table indexed by an input neuron and maps to the synapses that provide input to that neuron.
 *  @param[in] fAllowBackPropagation True if back propagaion is allowed.
 *  @param[in] iStepOffset           Offset from the current simulation step.
 */
extern __global__ void advanceLIFNeuronsDevice( int totalNeurons, int maxSynapses, int maxSpikes, const BGFLOAT deltaT, uint64_t simulationStep, float* randNoise, AllIFNeuronsProps* allNeuronsProps, AllSpikingSynapsesProps* allSynapsesProps, SynapseIndexMap* synapseIndexMapDevice, bool fAllowBackPropagation, int iStepOffset );

/**
 *  CUDA code for advancing izhikevich neurons
 *
 *  @param[in] totalNeurons          Number of neurons.
 *  @param[in] maxSynapses           Maximum number of synapses per neuron.
 *  @param[in] maxSpikes             Maximum number of spikes per neuron per epoch.
 *  @param[in] deltaT                Inner simulation step duration.
 *  @param[in] simulationStep        The current simulation step.
 *  @param[in] randNoise             Pointer to device random noise array.
 *  @param[in] allNeuronsProps       Pointer to Neuron structures in device memory.
 *  @param[in] allSynapsesProps      Pointer to Synapse structures in device memory.
 *  @param[in] synapseIndexMap       Inverse map, which is a table indexed by an input neuron and maps to the synapses that provide input to that neuron.
 *  @param[in] fAllowBackPropagation True if back propagaion is allowed.
 *  @param[in] iStepOffset           Offset from the current simulation step.
 */
extern __global__ void advanceIZHNeuronsDevice( int totalNeurons, int maxSynapses, int maxSpikes, const BGFLOAT deltaT, uint64_t simulationStep, float* randNoise, AllIZHNeuronsProps* allNeuronsProps, AllSpikingSynapsesProps* allSynapsesProps, SynapseIndexMap* synapseIndexMapDevice, bool fAllowBackPropagation, int iStepOffset );

#endif
