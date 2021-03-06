/**
 *      @file SInputPoisson.h
 *
 *      @brief A class that performs stimulus input (implementation Poisson).
 */

/**
 **
 ** \class SInputPoisson SInputPoisson.h "SInputPoisson.h"
 **
 ** \latexonly  \subsubsection*{Implementation} \endlatexonly
 ** \htmlonly   <h3>Implementation</h3> \endhtmlonly
 **
 ** The SInputPoisson performs providing stimulus input to the network for each time step.
 ** In this version, a layer of synapses are added, which accept external spike trains. 
 ** Each synapse gets an indivisual spike train (Poisson distribution) characterized 
 ** by mean firing rate, and each synapse has individual weight value. 
 **
 ** This class is the base class of GpuSInputPoisson and HostSInputPoisson.
 **
 ** \latexonly  \subsubsection*{Credits} \endlatexonly
 ** \htmlonly   <h3>Credits</h3> \endhtmlonly
 **
 ** Some models in this simulator is a rewrite of CSIM (2006) and other 
 ** work (Stiber and Kawasaki (2007?))
 **
 **
 **     @author Fumitaka Kawasaki
 **/

#pragma once

#ifndef _SINPUTPOISSON_H_
#define _SINPUTPOISSON_H_

#include "ISInput.h"
#include "AllDSSynapses.h"

class SInputPoisson : public ISInput
{
public:
    //! The constructor for SInputPoisson.
    SInputPoisson(SimulationInfo* psi, TiXmlElement* parms);
    ~SInputPoisson();

    //! Initialize data.
    virtual void init(SimulationInfo* psi, vector<ClusterInfo *> &vtClrInfo);

    //! Terminate process.
    virtual void term(SimulationInfo* psi, vector<ClusterInfo *> &vtClrInfo);

protected:
    //! True if stimuls input is on.
    bool m_fSInput;

    //! synapse weight
    BGFLOAT m_weight;

    //! inverse firing rate
    BGFLOAT m_lambda;

    //! interval counter
    int* m_nISIs;

    //! Masks for stimulus input
    bool* m_masks;

    //! Maximum number of synapses per neuron (will be 1)
    int m_maxSynapsesPerNeuron;
};

#endif // _SINPUTPOISSON_H_
