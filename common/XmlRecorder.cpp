/**
 *      @file XmlRecorder.cpp
 *
 *      @brief An implementation for recording spikes history on xml file
 */
//! An implementation for recording spikes history on xml file

#include "XmlRecorder.h"
#include "AllIFNeurons.h"      // TODO: remove LIF model specific code
#include "ConnGrowth.h"

//! THe constructor and destructor
XmlRecorder::XmlRecorder(IModel *model, SimulationInfo* sim_info) :
        burstinessHist(MATRIX_TYPE, MATRIX_INIT, 1, static_cast<int>(sim_info->epochDuration * sim_info->maxSteps), 0),
        spikesHistory(MATRIX_TYPE, MATRIX_INIT, 1, static_cast<int>(sim_info->epochDuration * sim_info->maxSteps * 100), 0),
        m_model(dynamic_cast<Model*> (model)),
        m_sim_info(sim_info)
{
}

XmlRecorder::~XmlRecorder()
{
}

/**
 * Initialize data
 * @param[in] stateOutputFileName	File name to save histories
 */
void XmlRecorder::init(const string& stateOutputFileName)
{
    stateOut.open( stateOutputFileName.c_str( ) );

}

/*
 * Init radii and rates history matrices with default values
 */
void XmlRecorder::initDefaultValues()
{
}

/*
 * Init radii and rates history matrices with current radii and rates
 */
void XmlRecorder::initValues()
{
}

/*
 * Get the current radii and rates values
 */
void XmlRecorder::getValues()
{
}

/**
 * Terminate process
 */
void XmlRecorder::term()
{
    stateOut.close();
}

/**
 * Compile history information in every epoch
 * @param[in] neurons 	The entire list of neurons.
 */
void XmlRecorder::compileHistories(AllNeurons &neurons)
{
    Connections* pConn = m_model->getConnections();

    AllSpikingNeurons &spNeurons = dynamic_cast<AllSpikingNeurons&>(neurons);
    int max_spikes = (int) ((m_sim_info->epochDuration * m_sim_info->maxFiringRate));

    // output spikes
    for (int iNeuron = 0; iNeuron < m_sim_info->totalNeurons; iNeuron++)
    {
        uint64_t* pSpikes = spNeurons.spike_history[iNeuron];

        int& spike_count = spNeurons.spikeCount[iNeuron];
        int& offset = spNeurons.spikeCountOffset[iNeuron];
        for (int i = 0, idxSp = offset; i < spike_count; i++, idxSp++)
        {
            if (idxSp >= max_spikes) idxSp = 0;
            // compile network wide burstiness index data in 1s bins
            int idx1 = static_cast<int>( static_cast<double>( pSpikes[idxSp] ) * m_sim_info->deltaT );
            burstinessHist[idx1] = burstinessHist[idx1] + 1.0;

            // compile network wide spike count in 10ms bins
            int idx2 = static_cast<int>( static_cast<double>( pSpikes[idxSp] ) * m_sim_info->deltaT * 100 );
            spikesHistory[idx2] = spikesHistory[idx2] + 1.0;
        }
    }

    // clear spike count
    spNeurons.clearSpikeCounts(m_sim_info);
}

/**
 * Save current simulation state to XML
 * @param  neurons the Neuron list to search from.
 **/
void XmlRecorder::saveSimState(const AllNeurons &neurons)
{
    // create Neuron Types matrix
    VectorMatrix neuronTypes(MATRIX_TYPE, MATRIX_INIT, 1, m_sim_info->totalNeurons, EXC);
    for (int i = 0; i < m_sim_info->totalNeurons; i++) {
        neuronTypes[i] = m_model->getLayout()->neuron_type_map[i];
    }

    // create neuron threshold matrix
    VectorMatrix neuronThresh(MATRIX_TYPE, MATRIX_INIT, 1, m_sim_info->totalNeurons, 0);
    for (int i = 0; i < m_sim_info->totalNeurons; i++) {
        neuronThresh[i] = dynamic_cast<const AllIFNeurons&>(neurons).Vthresh[i];
    }

    // Write XML header information:
    stateOut << "<?xml version=\"1.0\" standalone=\"no\"?>\n" << "<!-- State output file for the DCT growth modeling-->\n";
    //stateOut << version; TODO: version

    // Write the core state information:
    stateOut << "<SimState>\n";
    stateOut << "   " << burstinessHist.toXML("burstinessHist") << endl;
    stateOut << "   " << spikesHistory.toXML("spikesHistory") << endl;
    stateOut << "   " << m_model->getLayout()->xloc->toXML("xloc") << endl;
    stateOut << "   " << m_model->getLayout()->yloc->toXML("yloc") << endl;
    stateOut << "   " << neuronTypes.toXML("neuronTypes") << endl;

    // create starter nuerons matrix
    int num_starter_neurons = static_cast<int>(m_model->getLayout()->m_frac_starter_neurons * m_sim_info->totalNeurons);
    if (num_starter_neurons > 0)
    {
        VectorMatrix starterNeurons(MATRIX_TYPE, MATRIX_INIT, 1, num_starter_neurons);
        getStarterNeuronMatrix(starterNeurons, m_model->getLayout()->starter_map, m_sim_info);
        stateOut << "   " << starterNeurons.toXML("starterNeurons") << endl;
    }

    // Write neuron thresold
    stateOut << "   " << neuronThresh.toXML("neuronThresh") << endl;

    // write time between growth cycles
    stateOut << "   <Matrix name=\"Tsim\" type=\"complete\" rows=\"1\" columns=\"1\" multiplier=\"1.0\">" << endl;
    stateOut << "   " << m_sim_info->epochDuration << endl;
    stateOut << "</Matrix>" << endl;

    // write simulation end time
    stateOut << "   <Matrix name=\"simulationEndTime\" type=\"complete\" rows=\"1\" columns=\"1\" multiplier=\"1.0\">" << endl;
    stateOut << "   " << g_simulationStep * m_sim_info->deltaT << endl;
    stateOut << "</Matrix>" << endl;
    stateOut << "</SimState>" << endl;
}

/**
 *  Get starter Neuron matrix.
 *  @param  matrix   Starter Neuron matrix.
 *  @param  starter_map bool map to reference neuron matrix location from.
 *  @param  sim_info    SimulationInfo class to read information from.
 */
void XmlRecorder::getStarterNeuronMatrix(VectorMatrix& matrix, const bool* starter_map, const SimulationInfo *sim_info)
{
    int cur = 0;
    for (int i = 0; i < sim_info->totalNeurons; i++) {
        if (starter_map[i]) {
            matrix[cur] = i;
            cur++;
        }
    }
}
