#include <errno.h>
#include <signal.h>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <queue>
#include <unordered_set>
#include "hcm.h"
#include "flat.h"
#include "hcmvcd.h"
#include "hcmsigvec.h"

using namespace std;

//globals:
bool verbose = false;
queue<string> nodeQueue;
queue<string> cellQueue;










bool simAnd(vector<pair<string,bool>> inputs){
	bool result = 1;
	for(pair<string,int> input : inputs){
		result &= input.second;
	}
	return result;
}

bool simOr(vector<pair<string,bool>> inputs){
	bool result = 0;
	for(pair<string,bool> input : inputs){
		result |= input.second;
	}
	return result;
}

bool simNand(vector<pair<string,bool>> inputs){
	return !simAnd(inputs);
}

bool simNor(vector<pair<string,bool>> inputs){
	return !simOr(inputs);
}

bool simXor(vector<pair<string,bool>> inputs){
	bool result = inputs[0].second ^ inputs[1].second;
	for(size_t i = 2; i < inputs.size(); i++){
		result ^= inputs[i].second;
	}
	return result;
}

bool simBuffer(bool input){
	return input;
}

bool simInv(bool input){
	return !input;
}

bool simDff(vector<pair<string,bool>> inputs, vector<pair<string,bool>> oldInputs, bool lastVal){
	if(inputs.size() != 2 || oldInputs.size() != 2){
		std::cout << "Error(simDff): Wrong number of inputs, inputs.size = " << inputs.size() << "inputs_names.size = " << oldInputs.size() << endl;
	}

	// in case D is first in the list
	if(inputs[0].first == "D"){
		if(inputs[1].second == 1 &&  oldInputs[1].second == 0){ //clk ganged 0->1
			return oldInputs[0].second; // on posedge of clk return D
		}
		return lastVal;
	}
	// in case CLK is first in the list (same but opposite indexes)
	else {
		if(inputs[0].second == 1 && oldInputs[0].second == 0){
			return oldInputs[1].second;
		}
		return lastVal;
	}
}



bool simulateGate(string gate, vector<pair<string,bool>> inputs, vector<pair<string,bool>> oldInputs, bool lastVal){

	if(inputs.size() == 0){
		std::cout << "Error(simulateGate): Gate should have at least one input, but found: " << inputs.size() << std::endl;
		exit(1);
	}

	// if (lastVal ==-1){
	// 	lastVal =0;
	// }

	 if(gate.find("nand") != string::npos){
		return simNand(inputs);
	}

	else if(gate.find("nor") != string::npos){
		return simNor(inputs);
	}

	else if(gate.find("xor") != string::npos){
		return simXor(inputs);
	}

	else if(gate.find("buffer") != string::npos){
		return simBuffer(inputs[0].second);
	}

	else if(gate.find("inv") != string::npos){
		return simInv(inputs[0].second);
	}

	else if(gate.find("not") != string::npos){
		return simInv(inputs[0].second);
	}
	else if(gate.find("and") != string::npos){
		return simAnd(inputs);
	}

	else if(gate.find("or") != string::npos){
		return simOr(inputs);
	}

	else if(gate.find("dff") != string::npos){
		return simDff(inputs, oldInputs, lastVal);
	}

	else {
		std::cout << "Error(simulateGate): Gate" << gate << "does not exist" << std::endl;
		exit(1);
	}
}

void addCellsToQueue(std::queue<std::string>& cellQueue, std::unordered_set<std::string>& uniqueStrings,hcmNode *node);
pair<string,bool> simulate (hcmInstance *inst, queue<string> *nodeQueue,map <string,bool> nodeLastVal, map <string,bool> nodeNewVal, int time);
int main(int argc, char **argv) {
	int argIdx = 1;
	int anyErr = 0;
	unsigned int i;
	vector<string> vlgFiles;
	
	if (argc < 5) {
		anyErr++;
	} 
	else {
		if (!strcmp(argv[argIdx], "-v")) {
			argIdx++;
			verbose = true;
		}
		for (;argIdx < argc; argIdx++) {
			vlgFiles.push_back(argv[argIdx]);
		}
		
		if (vlgFiles.size() < 2) {
			cerr << "-E- At least top-level and single verilog file required for spec model" << endl;
			anyErr++;
		}
	}

	if (anyErr) {
		cerr << "Usage: " << argv[0] << "  [-v] top-cell signal_file.sig.txt vector_file.vec.txt file1.v [file2.v] ... \n";
		exit(1);
	}
 
	set< string> globalNodes;
	globalNodes.insert("VDD");
	globalNodes.insert("VSS");
	
	hcmDesign* design = new hcmDesign("design");
	string cellName = vlgFiles[0];
	for (i = 3; i < vlgFiles.size(); i++) {
		printf("-I- Parsing verilog %s ...\n", vlgFiles[i].c_str());
		if (!design->parseStructuralVerilog(vlgFiles[i].c_str())) {
			cerr << "-E- Could not parse: " << vlgFiles[i] << " aborting." << endl;
			exit(1);
		}
	}

	hcmCell *topCell = design->getCell(cellName);
	if (!topCell) {
		printf("-E- could not find cell %s\n", cellName.c_str());
		exit(1);
	}
	
	hcmCell *flatCell = hcmFlatten(cellName + string("_flat"), topCell, globalNodes);
	cout << "-I- Top cell flattened" << endl;

	string signalTextFile = vlgFiles[1];
	string vectorTextFile = vlgFiles[2];

	
	// Genarate the vcd file
	//-----------------------------------------------------------------------------------------//
	// If you need to debug internal nodes, set debug_mode to true in order to see in 
	// the vcd file and waves the internal nodes.
	// NOTICE !!!
	// you need to submit your work with debug_mode = false
	// vcdFormatter vcd(cellName + ".vcd", flatCell, globalNodes, true);  <--- for debug only!
	//-----------------------------------------------------------------------------------------//
	vcdFormatter vcd(cellName + ".vcd", flatCell, globalNodes);
	if(!vcd.good()) {
		printf("-E- vcd initialization error.\n");
		exit(1);
	}

	// initiate the time variable "time" to 1 
	int time = 1;
	hcmSigVec parser(signalTextFile, vectorTextFile, verbose);
	set<string> signals;
	parser.getSignals(signals);

	//-----------------------------------------------------------------------------------------//
	//enter your code below


    // Priority queue with custom comparator
    

	bool valp = false;
	bool & val =valp;

	map <string,bool> nodeLastVal;
	map <string,bool> nodeNewVal;
	map <string, hcmNodeCtx * > nodeCtxMap;
	map <string,bool> cellLastVal;
	list<const hcmInstance *> parents;
	// initializing the last values list to -1;
	map< string, hcmInstance * > cellList = flatCell->getInstances();
	map< string, hcmNode * > nodelist = flatCell->getNodes();
	for(auto itr = cellList.begin(); itr != cellList.end(); itr++){
    	cellLastVal[itr->first] = 0;
	}
	for(auto itr = nodelist.begin(); itr != nodelist.end(); itr++){
		if (itr->first == "VDD"){
			nodeLastVal[itr->first] = 1;
			nodeNewVal[itr->first] = 1;
		}
		else if (itr->first == "VSS"){
	    	nodeLastVal[itr->first] = 0;
			nodeNewVal[itr->first] = 0;
		}
		else {
    		nodeLastVal[itr->first] = 0;
			nodeNewVal[itr->first] = 0;
			nodeCtxMap[itr->first] = new hcmNodeCtx(parents, itr->second);
		}
	}
	// for each cell i need a list of inputs and last values
	// list of which inputs connected to what output 
	// reading each vector
	while (parser.readVector() == 0) {
		cout << "time : " << time << endl; 
		// if (time ==15){
		// 	break;
		// }
		std::unordered_set<std::string> uniqueStrings;
		// set the inputs to the values from the input vector.
		// if (time == 1){
			addCellsToQueue(cellQueue,uniqueStrings,flatCell->getNode("VDD"));
			addCellsToQueue(cellQueue,uniqueStrings,flatCell->getNode("VSS"));
		// }
    	for (const std::string& StrName : signals) {
			parser.getSigValue(StrName, val);
        	// std::cout << StrName <<" it's current value : " << val << " it's last val : " <<  nodeLastVal[StrName] << std::endl;
			// if there a change in the node's value,
			// vcd.changeValue(nodeCtxMap[StrName],val); 
			if (val != nodeLastVal[StrName] || time == 1){

				nodeNewVal[StrName] = val;

				addCellsToQueue(cellQueue,uniqueStrings,flatCell->getNode(StrName));
			}

    	}
		while (!cellQueue.empty() || !nodeQueue.empty()){ // FIXME update to max rank
			string tempCell;
			// cout << "tempCell is : " << tempCell <<endl;
			while (!cellQueue.empty()){
				tempCell = cellQueue.front();
				cellQueue.pop();
				uniqueStrings.erase(tempCell);
				pair <string, bool> temporNodeVal;
				temporNodeVal = simulate(flatCell->getInst(tempCell), &nodeQueue,nodeLastVal,nodeNewVal, time);
				uniqueStrings.erase(tempCell);
				
				nodeNewVal[temporNodeVal.first] = temporNodeVal.second;

			}

			while(!nodeQueue.empty()){
				string tempNode = nodeQueue.front();
				nodeQueue.pop();
				addCellsToQueue(cellQueue,uniqueStrings,flatCell->getNode(tempNode));
			}

		}



		// simulate the vector 

		// go over all values and write them to the vcd.
		for(auto itr = nodelist.begin(); itr != nodelist.end(); itr++){
    		nodeLastVal[itr->first] = nodeNewVal[itr->first];

		}
		for(auto itr = nodeCtxMap.begin(); itr != nodeCtxMap.end(); itr++){
			vcd.changeValue(nodeCtxMap[itr->first],nodeNewVal[itr->first]);
		}
		vcd.changeTime(time++);

	}


	//-----------------------------------------------------------------------------------------//
}	
pair<string,bool> simulate (hcmInstance *inst, queue<string> *nodeQueue,map <string,bool> nodeLastVal, map <string,bool> nodeNewVal, int time){
	map <string,hcmInstPort *> instports;
	vector <pair<string,bool>> oldInputs;
	vector <pair<string,bool>> newInputs;
	string outNode;
    instports = inst->getInstPorts();
    for(auto itr = instports.begin(); itr != instports.end();){
      if (itr->second->getPort()->getDirection() == 2){
		outNode = itr->second->getNode()->getName();
        itr = instports.erase(itr);
      }
      else {
		oldInputs.push_back({itr->second->getNode()->getName(),nodeLastVal[itr->second->getNode()->getName()]});
		newInputs.push_back({itr->second->getNode()->getName(),nodeNewVal[itr->second->getNode()->getName()]});
        itr++;
      }

	}
	bool output =simulateGate(inst->masterCell()->getName() ,newInputs,oldInputs, nodeLastVal[outNode]);
	if (output != nodeNewVal[outNode] || time ==1){
		for(auto nodePair : nodeNewVal){
			cout << "node : " << nodePair.first << " val : " << nodePair.second << endl;
		}
		nodeQueue->push(outNode);
	}
	return make_pair(outNode, output);
}


void addCellsToQueue(std::queue<std::string>& cellQueue, std::unordered_set<std::string>& uniqueStrings,hcmNode *node){
// need to take care of output ports as well	
	map< string, hcmInstPort *> instPortMap = node->getInstPorts(); 
	for(auto itr = instPortMap.begin(); itr != instPortMap.end(); itr++){
    	// checks if ports in an input
		if (itr->second->getPort()->getDirection() ==1){
			  if (uniqueStrings.find(itr->second->getInst()->getName()) == uniqueStrings.end()) {
        		cellQueue.push(itr->second->getInst()->getName());
        		uniqueStrings.insert(itr->second->getInst()->getName());
 			   }
		}
	}
}