/*
 * @file Simulator.cpp
 *
 * @author Derek McLean
 *
 * @brief Base class for model-independent simulators targeting different
 * platforms.
 */

#include "Simulator.h"

/*
 *  Constructor
 */
Simulator::Simulator() 
{
  g_simulationStep = 0;
}

/*
 * Destructor.
 */
Simulator::~Simulator()
{
  freeResources();
}

/*
 *  Initialize and prepare network for simulation.
 *
 *  @param  sim_info    parameters for the simulation.
 */
void Simulator::setup(SimulationInfo *sim_info)
{
#ifdef PERFORMANCE_METRICS
  // Start overall simulation timer
  cerr << "Starting main timer... ";
  t_host_initialization_layout = 0.0;
  t_host_initialization_clusters = 0.0;
  t_host_initialization_connections = 0.0;
  t_host_advance = 0.0;
  t_host_adjustSynapses = 0.0;
  t_host_createSynapseImap = 0.0;
  sim_info->timer.start();
  cerr << "done." << endl;
#endif

  DEBUG(cerr << "Initializing models in network... ";)
  sim_info->model->setupSim(sim_info);
  DEBUG(cerr << "\ndone init models." << endl;)
}

/*
 *  Begin terminating the simulator.
 *
 *  @param  sim_info    parameters for the simulation.
 */
void Simulator::finish(SimulationInfo *sim_info)
{
  // Terminate the simulator
  sim_info->model->cleanupSim(sim_info); // Can #term be removed w/ the new model architecture?  // =>ISIMULATION
} 

/**
 * Copy GPU Synapse data to CPU.
 *
 *  @param  sim_info    parameters for the simulation.
 */
void Simulator::copyGPUSynapseToCPU(SimulationInfo *sim_info) {
  sim_info->model->copyGPUSynapseToCPUSim(sim_info); 
}

/**
 * Copy CPU Synapse data to GPU.
 *
 *  @param  sim_info    parameters for the simulation.
 */
void Simulator::copyCPUSynapseToGPU(SimulationInfo *sim_info) {
  sim_info->model->copyCPUSynapseToGPUSim(sim_info); 
}

/*
 * Resets all of the maps.
 * Releases and re-allocates memory for each map, clearing them as necessary.
 *
 *  @param  sim_info    parameters for the simulation.
 */
void Simulator::reset(SimulationInfo *sim_info)
{
  DEBUG(cout << "\nEntering Simulator::reset()" << endl;)

  // Terminate the simulator
  sim_info->model->cleanupSim(sim_info);

  // Clean up objects
  freeResources();

  // Reset global simulation Step to 0
  g_simulationStep = 0;

  // Initialize and prepare network for simulation
  sim_info->model->setupSim(sim_info);

  DEBUG(cout << "\nExiting Simulator::reset()" << endl;)
}

/*
 *  Clean up objects.
 */
void Simulator::freeResources()
{
}

/*
 * Run simulation
 *
 *  @param  sim_info    parameters for the simulation.
 */
void Simulator::simulate(SimulationInfo *sim_info)
{
  // Main simulation loop - execute maxGrowthSteps
  for (int currentStep = 1; currentStep <= sim_info->maxSteps; currentStep++) {

    DEBUG(cout << endl << endl;)
      DEBUG(cout << "Performing simulation number " << currentStep << endl;)
      DEBUG(cout << "Begin network state:" << endl;)

      // Init SimulationInfo parameters
      sim_info->currentStep = currentStep;

#ifdef PERFORMANCE_METRICS
      // Start timer for advance
      sim_info->short_timer.start();
#endif
    // Advance simulation to next growth cycle
    advanceUntilGrowth(currentStep, sim_info);
#ifdef PERFORMANCE_METRICS
    // Time to advance
    t_host_advance += sim_info->short_timer.lap() / 1000000.0;
#endif

    DEBUG(cout << endl << endl;)
      DEBUG(
            cout << "Done with simulation cycle, beginning growth update "
	    << currentStep << endl;
	    )

      // Update the neuron network
#ifdef PERFORMANCE_METRICS
      // Start timer for connection update
      sim_info->short_timer.start();
#endif
    sim_info->model->updateConnections(sim_info);

    sim_info->model->updateHistory(sim_info);

#ifdef PERFORMANCE_METRICS
    // Times converted from microseconds to seconds
    // Time to update synapses
    t_host_adjustSynapses += sim_info->short_timer.lap() / 1000000.0;
    // Time since start of simulation
    double total_time = sim_info->timer.lap() / 1000000.0;

    cout << "\ntotal_time: " << total_time << " seconds" << endl;
    sim_info->model->printPerformanceMetrics(total_time, currentStep);
    cout << endl;
#endif
  }
}

/*
 * Helper for #simulate().
 * Advance simulation until it's ready for the next growth cycle. This should simulate all neuron and
 * synapse activity for one epoch.
 *
 *  @param currentStep the current epoch in which the network is being simulated.
 *  @param  sim_info    parameters for the simulation.
 */
void Simulator::advanceUntilGrowth(const int currentStep, SimulationInfo *sim_info)
{
  uint64_t count = 0;
  // Compute step number at end of this simulation epoch
  uint64_t endStep = g_simulationStep
    + static_cast<uint64_t>(sim_info->epochDuration / sim_info->deltaT);

  DEBUG_MID(sim_info->model->logSimStep(sim_info);) // Generic model debug call

    while (g_simulationStep < endStep) {
      // incremental step
      int iStep = endStep - g_simulationStep;
      iStep = (iStep < sim_info->minSynapticTransDelay) ? iStep : sim_info->minSynapticTransDelay;

      DEBUG_LOW(
		// Output status once every 10,000 steps
		if (count % 10000 < static_cast<uint64_t>(iStep))
		  {
		    cout << currentStep << "/" << sim_info->maxSteps
			 << " simulating time: "
			 << static_cast<int>(g_simulationStep * sim_info->deltaT) << endl;
		    count = 0;
		  }
		count += iStep;
		)

      // Advance the Network one time step
      sim_info->model->advance(sim_info, iStep);

      g_simulationStep += iStep;
    }
}

/*
 * Writes simulation results to an output destination.
 * 
 *  @param  sim_info    parameters for the simulation. 
 */
void Simulator::saveData(SimulationInfo *sim_info) const
{
  sim_info->model->saveData(sim_info);
}
