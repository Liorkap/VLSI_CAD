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
void getRankMap (hcmCell *flatCell);
int getInputPorts (hcmInstance *inst);
int getPerviousPort(hcmNode *node );
bool hasRank (hcmInstance *inst);
bool isPrimitive (hcmInstance *inst);
vector<pair<int,hcmInstance *>> maxRankVector;
map <hcmInstance *, int> rankmap;
map <string, int> cellRank;
queue<string> nodeQueue;


// Custom comparator for sorting in ascending order based on the int component
auto comparator = [](const std::pair<int, std::string>& a, const std::pair<int, std::string>& b) {
    return a.first > b.first;  // Change to '<' for descending order
};
std::priority_queue<std::pair<int, std::string>, std::vector<std::pair<int, std::string>>, decltype(comparator)> cellQueue(comparator);
// Function to insert pairs into the priority queue if the string is not already present
void insertIntoPriorityQueue(std::priority_queue<std::pair<int, std::string>, std::vector<std::pair<int, std::string>>, decltype(comparator)>& pq, std::unordered_set<std::string>& uniqueStrings, int key, const std::string& value) {
    if (uniqueStrings.find(value) == uniqueStrings.end()) {
        cellQueue.push({key, value});
        uniqueStrings.insert(value);
    }
}
void addCellsToQueue(std::priority_queue<std::pair<int, std::string>, std::vector<std::pair<int, std::string>>, decltype(comparator)>& pq, std::unordered_set<std::string>& uniqueStrings,hcmNode *node);
// Function to pop a single pair with a specific int from the priority queue
string popSinglePairWithSpecificInt(std::priority_queue<std::pair<int, std::string>, std::vector<std::pair<int, std::string>>, decltype(comparator)>& pq, int specificInt) {
    // while (!cellQueue.empty()) {
		if (!cellQueue.empty()){
        std::pair<int, std::string> topPair = cellQueue.top();
        if (topPair.first <= specificInt) {
			cellQueue.pop();
            return topPair.second;
        }

    }

    // If no pair matches the specific int, return a "null" pair
    return ""; // You can use any sentinel values or use std::optional or std::pair<int, std::string>* for a more explicit representation
}


int simAnd(vector<pair<string,int>> inputs){
	int result = 1;
	for(pair<string,int> input : inputs){
		result &= input.second;
	}
	return result;
}

int simOr(vector<pair<string,int>> inputs){
	int result = 0;
	for(pair<string,int> input : inputs){
		result |= input.second;
	}
	return result;
}

int simNand(vector<pair<string,int>> inputs){
	return !simAnd(inputs);
}

int simNor(vector<pair<string,int>> inputs){
	return !simOr(inputs);
}

int simXor(vector<pair<string,int>> inputs){
	int result = inputs[0].second ^ inputs[1].second;
	for(int i = 2; i < inputs.size(); i++){
		result ^= inputs[i].second;
	}
	return result;
}

int simBuffer(int input){
	return input;
}

int simInv(int input){
	return !input;
}

int simDff(vector<pair<string,int>> inputs, vector<pair<string,int>> oldInputs, int lastVal){
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



int simulateGate(string gate, vector<pair<string,int>> inputs, vector<pair<string,int>> oldInputs, int lastVal){

	if(inputs.size() == 0){
		std::cout << "Error(simulateGate): Gate should have at least one input, but found: " << inputs.size() << std::endl;
		exit(1);
	}

	if (lastVal ==-1){
		lastVal =0;
	}

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


pair<string,bool> simulate (hcmInstance *inst, queue<string> *nodeQueue,map <string,int> nodeLastVal, map <string,int> nodeNewVal);
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

	map <string,int> nodeLastVal;
	map <string,int> nodeNewVal;
	map <string, hcmNodeCtx * > nodeCtxMap;
	map <string,int> cellLastVal;
	list<const hcmInstance *> parents;
	// initializing the last values list to -1;
	map< string, hcmInstance * > cellList = flatCell->getInstances();
	map< string, hcmNode * > nodelist = flatCell->getNodes();
	for(auto itr = cellList.begin(); itr != cellList.end(); itr++){
    	cellLastVal[itr->first] = -1;
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
    		nodeLastVal[itr->first] = -1;
			nodeNewVal[itr->first] = -1;
			nodeCtxMap[itr->first] = new hcmNodeCtx(parents, itr->second);
		}
	}
	// for each cell i need a list of inputs and last values
	// list of which inputs connected to what output 
	// reading each vector
	getRankMap(flatCell);
	while (parser.readVector() == 0) {
		cout << "time is : " << time << endl; 
		std::unordered_set<std::string> uniqueStrings;
		// set the inputs to the values from the input vector.
    	for (const std::string& StrName : signals) {
			parser.getSigValue(StrName, val);
        	// std::cout << StrName <<" it's current value : " << val << " it's last val : " <<  nodeLastVal[StrName] << std::endl;
			// if there a change in the node's value, 
			if (val != nodeLastVal[StrName]){
				vcd.changeValue(nodeCtxMap[StrName],val);
				nodeNewVal[StrName] = val;

				addCellsToQueue(cellQueue,uniqueStrings,flatCell->getNode(StrName));
			}

    	}
		for (int currRank = 0; currRank < 50 ; currRank++){ // FIXME update to max rank
			string tempCell = popSinglePairWithSpecificInt(cellQueue,currRank);
			// cout << "tempCell is : " << tempCell <<endl;
			while (tempCell != ""){
				cout << "cell is " << tempCell << " and it's rank : " << cellRank[tempCell] << endl;
				pair <string, bool> temporNodeVal;
				temporNodeVal = simulate(flatCell->getInst(tempCell), &nodeQueue,nodeLastVal,nodeNewVal);
				
				nodeNewVal[temporNodeVal.first] = temporNodeVal.second;
				
				// cout << " temp node is  : " << temporNode << " curr rank is : " << currRank << endl;
				if(nodeLastVal[temporNodeVal.first]!=temporNodeVal.second){
					vcd.changeValue(nodeCtxMap[temporNodeVal.first],temporNodeVal.second);
				}
				cout << "after updatingNodeVAl2" << endl;
				tempCell = popSinglePairWithSpecificInt(cellQueue,currRank);
				cout << "tempCell is : " << tempCell <<endl;
			}
			// cout << "node queue is empty" << nodeQueue.empty() << endl;

			while(!nodeQueue.empty()){
				cout <<" in node queue" << endl;
				string tempNode = nodeQueue.front();

				nodeQueue.pop();
				addCellsToQueue(cellQueue,uniqueStrings,flatCell->getNode(tempNode));
			}

		}



		// simulate the vector 

		// go over all values and write them to the vcd.
		for(auto itr = nodelist.begin(); itr != nodelist.end(); itr++){
			cout << itr->first << " last value : " << nodeLastVal[itr->first]  << endl;
    		nodeLastVal[itr->first] = nodeNewVal[itr->first];
			cout << itr->first << " new value : " << nodeLastVal[itr->first]  << endl;
		}
		vcd.changeTime(time++);
		// currRank++;

	}

	//   for(auto itr = maxRankVector.begin(); itr != maxRankVector.end(); itr++){
	// 	cout << itr->first << " " << itr->second->getName() << endl;
	// }

	//-----------------------------------------------------------------------------------------//
}	
pair<string,bool> simulate (hcmInstance *inst, queue<string> *nodeQueue,map <string,int> nodeLastVal, map <string,int> nodeNewVal){
	map <string,hcmInstPort *> instports;
	vector <pair<string,int>> oldInputs;
	vector <pair<string,int>> newInputs;
	string outNode;
    instports = inst->getInstPorts();
    for(auto itr = instports.begin(); itr != instports.end();){
      if (itr->second->getPort()->getDirection() == 2){
		outNode = itr->second->getNode()->getName();
        itr = instports.erase(itr);
      }
      else {
		cout << " input : " << itr->second->getNode()->getName() << " val : " << nodeNewVal[itr->second->getNode()->getName()] << " last val : " << nodeLastVal[itr->second->getNode()->getName()];
		oldInputs.push_back({itr->second->getNode()->getName(),nodeLastVal[itr->second->getNode()->getName()]});
		newInputs.push_back({itr->second->getNode()->getName(),nodeNewVal[itr->second->getNode()->getName()]});
        itr++;
      }

	}
	int output =simulateGate(inst->masterCell()->getName() ,newInputs,oldInputs, nodeLastVal[outNode]);
	cout << "cell is " << inst->masterCell()->getName() << " output is " << output << " last output is" << nodeLastVal[outNode] << endl;
	if (output != nodeLastVal[outNode]){
		cout << "before is empty" << nodeQueue->empty() << endl;
		nodeQueue->push(outNode);
		cout << "after is empty" << nodeQueue->empty() << endl;

	}
	return make_pair(outNode, output ==1);
}


void addCellsToQueue(std::priority_queue<std::pair<int, std::string>, std::vector<std::pair<int, std::string>>, decltype(comparator)>& pq, std::unordered_set<std::string>& uniqueStrings,hcmNode *node){
// need to take care of output ports as well	
	map< string, hcmInstPort *> instPortMap = node->getInstPorts(); 
	for(auto itr = instPortMap.begin(); itr != instPortMap.end(); itr++){
    	// checks if ports in an input
		if (itr->second->getPort()->getDirection() ==1){
			 insertIntoPriorityQueue(pq, uniqueStrings, cellRank[itr->second->getInst()->getName()], itr->second->getInst()->getName());
			// cout << itr->second->getInst()->getName() << " was added to queue" << endl;
		}
	}
}

void getRankMap (hcmCell *flatCell){
	map <string,hcmInstance *> instmap = flatCell->getInstances();
  	for(auto itr = instmap.begin(); itr != instmap.end(); itr++){
    	getInputPorts(itr->second);
	}
   	pair<int,hcmInstance *> tempPair;
  	for(auto itr = rankmap.begin(); itr != rankmap.end(); itr++){
    	if(itr->second != -1) // if has -1 rank it's connected to VDD/VSS
    	{
			cout << itr->first->getName() << itr->second << endl;
      		cellRank[itr->first->getName()] = itr->second;
    	}
	}

}


// goes over the input ports of inst and return the max rank
int getInputPorts (hcmInstance *inst){
    map <string,hcmInstPort *> instports;
    instports = inst->getInstPorts();
    for(auto itr = instports.begin(); itr != instports.end();){
      if (itr->second->getPort()->getDirection() == 2){
        itr = instports.erase(itr);
      }
      else {
        itr++;
      }

	}
    int maxrank =-1;
    int temprank;
    for(auto itr = instports.begin(); itr != instports.end();itr++){
      temprank = getPerviousPort(itr->second->getNode());
      if (temprank > maxrank){
        maxrank = temprank;
      }

	}
  rankmap[inst] = maxrank;
  return maxrank;
}
// returns the rank of the node's output port/instport
int getPerviousPort(hcmNode *node ) {

  map <string,hcmInstPort *> instports = node->getInstPorts();
      for(auto itr = instports.begin(); itr != instports.end();){
      if (itr->second->getPort()->getDirection() == 1){
        itr = instports.erase(itr);
      }
      else {
        itr++;
      }
      }
      if( instports.size() == 1){
        if (hasRank(instports.begin()->second->getInst())){
          if (rankmap[instports.begin()->second->getInst()] == -1){
            return -1;
          }
          return 1 + rankmap[instports.begin()->second->getInst()];
        }
        else {
          return 1 + getInputPorts(instports.begin()->second->getInst());
        }

      }
      else {
        if (node->getName() == "VSS" || node->getName() == "VDD" || node->getPort() == 0) // if the port is VDD/VSS gives it -1 rank
        {
          return -1;
        }
        
        return instports.size();
      }

  
}

// check if the inst already has rank
bool hasRank (hcmInstance *inst){
  auto it = rankmap.find(inst);
  return it != rankmap.end();

}

// checks if the inst is primitive ( not needed for flat topology)
bool isPrimitive (hcmInstance *inst){
  int size = inst->masterCell()->getInstances().size() ;
  return (size == 0);

}