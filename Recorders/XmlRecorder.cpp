/*
 *      @file XmlRecorder.cpp
 *
 *      @brief An implementation for recording spikes history on xml file
 */
//! An implementation for recording spikes history on xml file

#include "XmlRecorder.h"
#include "AllIFNeurons.h"      // TODO: remove LIF model specific code
#include "ConnGrowth.h"

//! THe constructor and destructor
XmlRecorder::XmlRecorder(const SimulationInfo* sim_info) :
        burstinessHist(MATRIX_TYPE, MATRIX_INIT, 1, static_cast<int>(sim_info->epochDuration * sim_info->maxSteps), 0),
        spikesHistory(MATRIX_TYPE, MATRIX_INIT, 1, static_cast<int>(sim_info->epochDuration * sim_info->maxSteps * 100), 0),
        m_sim_info(sim_info),
        m_model(dynamic_cast<Model*> (sim_info->model))
{
}

XmlRecorder::~XmlRecorder()
{
}

/*
 * Initialize data
 * Create a new xml file.
 *
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

/*
 * Terminate process
 */
void XmlRecorder::term()
{
    stateOut.close();
}

/*
 * Compile history information in every epoch
 *
 * @param[in] neurons 	The entire list of neurons.
 * @param[in] clr_info  ClusterInfo class to read information from.
 */
void XmlRecorder::compileHistories(vector<Cluster *> &vtClr, vector<ClusterInfo *> &vtClrInfo)
{
    int max_spikes = (int) ((m_sim_info->epochDuration * m_sim_info->maxFiringRate));

    for (CLUSTER_INDEX_TYPE iCluster = 0; iCluster < vtClr.size(); iCluster++)
    { 
        AllSpikingNeurons *neurons = dynamic_cast<AllSpikingNeurons*>(vtClr[iCluster]->m_neurons);
        AllSpikingNeuronsProps *pNeuronsProps = dynamic_cast<AllSpikingNeuronsProps*>(neurons->m_pNeuronsProps);

        // output spikes
        int neuronLayoutIndex = vtClrInfo[iCluster]->clusterNeuronsBegin;
        int totalClusterNeurons = vtClrInfo[iCluster]->totalClusterNeurons;
        for (int iNeuron = 0; iNeuron < totalClusterNeurons; iNeuron++, neuronLayoutIndex++)
        {
            uint64_t* pSpikes = pNeuronsProps->spike_history[iNeuron];

            int& spike_count = pNeuronsProps->spikeCount[iNeuron];
            int& offset = pNeuronsProps->spikeCountOffset[iNeuron];
            for (int i = 0, idxSp = offset; i < spike_count; i++, idxSp++)
            {
                // Single precision (float) gives you 23 bits of significand, 8 bits of exponent, 
                // and 1 sign bit. Double precision (double) gives you 52 bits of significand, 
                // 11 bits of exponent, and 1 sign bit. 
                // Therefore, single precision can only handle 2^23 = 8,388,608 simulation steps 
                // or 8 epochs (1 epoch = 100s, 1 simulation step = 0.1ms).

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
        pNeuronsProps->clearSpikeCounts(m_sim_info, vtClrInfo[iCluster], vtClr[iCluster]);
    }
}

/*
 * Writes simulation results to an output destination.
 *
 * @param  neurons the Neuron list to search from.
 **/
void XmlRecorder::saveSimData(vector<Cluster *> &vtClr, vector<ClusterInfo *> &vtClrInfo)
{
    // create Neuron Types matrix
    VectorMatrix neuronTypes(MATRIX_TYPE, MATRIX_INIT, 1, m_sim_info->totalNeurons, EXC);
    for (int i = 0; i < m_sim_info->totalNeurons; i++) {
        neuronTypes[i] = m_model->getLayout()->neuron_type_map[i];
    }

    // create neuron threshold matrix
    VectorMatrix neuronThresh(MATRIX_TYPE, MATRIX_INIT, 1, m_sim_info->totalNeurons, 0);
    for (CLUSTER_INDEX_TYPE iCluster = 0; iCluster < vtClr.size(); iCluster++) {
        AllIFNeurons *neurons = dynamic_cast<AllIFNeurons*>(vtClr[iCluster]->m_neurons);
        AllIFNeuronsProps *pNeuronsProps = dynamic_cast<AllIFNeuronsProps*>(neurons->m_pNeuronsProps);

        int neuronLayoutIndex = vtClrInfo[iCluster]->clusterNeuronsBegin;
        int totalClusterNeurons = vtClrInfo[iCluster]->totalClusterNeurons;
        for (int iNeurons = 0; iNeurons < totalClusterNeurons; iNeurons++, neuronLayoutIndex++) {
            neuronThresh[neuronLayoutIndex] = pNeuronsProps->Vthresh[iNeurons];
        }
    }

    // Write XML header information:
    stateOut << "<?xml version=\"1.0\" standalone=\"no\"?>\n" << "<!-- State output file for the DCT growth modeling-->\n";
    //stateOut << version; TODO: version

    // Write the core state information:
    VectorMatrix* xloc = new VectorMatrix(MATRIX_TYPE, MATRIX_INIT, 1, m_sim_info->totalNeurons);
    VectorMatrix* yloc = new VectorMatrix(MATRIX_TYPE, MATRIX_INIT, 1, m_sim_info->totalNeurons);
    for (int i = 0; i < m_sim_info->totalNeurons; i++) {
        (*xloc)[i] = m_model->getLayout()->xloc[i];
        (*yloc)[i] = m_model->getLayout()->yloc[i];
    }

    stateOut << "<SimState>\n";
    stateOut << "   " << burstinessHist.toXML("burstinessHist") << endl;
    stateOut << "   " << spikesHistory.toXML("spikesHistory") << endl;
    stateOut << "   " << xloc->toXML("xloc") << endl;
    stateOut << "   " << yloc->toXML("yloc") << endl;
    stateOut << "   " << neuronTypes.toXML("neuronTypes") << endl;

    delete xloc;
    delete yloc;

    // create starter nuerons matrix
    int num_starter_neurons = static_cast<int>(m_model->getLayout()->num_endogenously_active_neurons);
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

/*
 *  Get starter Neuron matrix.
 *
 *  @param  matrix      Starter Neuron matrix.
 *  @param  starter_map Bool map to reference neuron matrix location from.
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
