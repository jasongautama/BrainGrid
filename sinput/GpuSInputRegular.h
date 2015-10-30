/**
 ** \brief A class that performs stimulus input (implementation Regular).
 **
 ** \class GpuSInputRegular GpuSInputRegular.h "GpuSInputRegular.h"
 **
 ** \latexonly  \subsubsection*{Implementation} \endlatexonly
 ** \htmlonly   <h3>Implementation</h3> \endhtmlonly
 **
 ** The GpuSInputRegular performs providing stimulus input to the network for each time step on GPU.
 ** Inputs are series of current pulses, which are characterized by a duration, an interval
 ** and input values.
 **
 ** \latexonly  \subsubsection*{Credits} \endlatexonly
 ** \htmlonly   <h3>Credits</h3> \endhtmlonly
 **
 ** This simulator is a rewrite of CSIM (2006) and other work (Stiber and Kawasaki (2007?))
 **
 **
 **     @author Fumitaka Kawasaki
 **/

/**
 ** \file GpuSInputRegular.h
 **
 ** \brief Header file for GpuSInputRegular.
 **/

#pragma once

#ifndef _GPUSINPUTREGULAR_H_
#define _GPUSINPUTREGULAR_H_

#include "SInputRegular.h"

class GpuSInputRegular : public SInputRegular
{
public:
    //! The constructor for SInputRegular.
    GpuSInputRegular(SimulationInfo* psi, TiXmlElement* parms);
    ~GpuSInputRegular();

    //! Initialize data.
    virtual void init(IModel* model, AllNeurons &neurons, SimulationInfo* psi);

    //! Terminate process.
    virtual void term(IModel* model, SimulationInfo* psi);

    //! Process input stimulus for each time step.
    virtual void inputStimulus(IModel* model, SimulationInfo* psi, BGFLOAT* summationPoint);
};

//! Device function that processes input stimulus for each time step.
#if defined(__CUDACC__)
extern __global__ void inputStimulusDevice( int n, BGFLOAT* summationPoint_d, BGFLOAT* initValues_d, int* nShiftValues_d, int nStepsInCycle, int nStepsCycle, int nStepsDuration );
#endif

#endif // _GPUSINPUTREGULAR_H_