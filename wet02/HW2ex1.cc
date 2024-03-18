#include <errno.h>
#include <signal.h>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <queue>
#include "hcm.h"
#include "flat.h"
#include "hcmvcd.h"
#include "hcmsigvec.h"

using namespace std;

//globals:
bool verbose = false;

queue<hcmNode*> eventQ;

void simulate(hcmInstance* inst, int time);
bool simGate(hcmCell* gate, map<string,bool> inputs, bool currOutVal);
bool simDff(map<string,bool> inputs, bool currQ);
bool simInv(map<string,bool> inputs);
bool simBuffer(map<string,bool> inputs);
bool simXor(map<string,bool> inputs);
bool simNor(map<string,bool> inputs);
bool simAnd(map<string,bool> inputs);
bool simOr(map<string,bool> inputs);
bool simNand(map<string,bool> inputs);
void setNodeValue(hcmNode* node, bool value);
void updateFFsPrevVal(hcmCell* flatCell);




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
    map<string, hcmNode*> nodesMap = flatCell->getNodes();
    for(auto nodePair : nodesMap){
		if(nodePair.first == "VSS"){
        	setNodeValue(nodePair.second, 0);
		}
		else if(nodePair.first == "VDD"){
			setNodeValue(nodePair.second, 1);
		}
    }

	

	// reading each vector
	while (parser.readVector() == 0) {
		// set the inputs to the values from the input vector.
		eventQ.push(flatCell->getNode("VDD"));
		eventQ.push(flatCell->getNode("VSS"));
		for(string sig : signals){
            bool value;
			bool currValue;
            hcmNode* node = flatCell->getNode(sig);
            parser.getSigValue(sig, value);
            if(node->getProp("value", currValue) == NOT_FOUND || value != currValue){
				eventQ.push(node);
				setNodeValue(node, value);
			}
        }
		// simulate the vector
        while(!eventQ.empty()){
            hcmNode* node = eventQ.front();
            map<string, hcmInstPort*> nodeInstPorts = node->getInstPorts();
            for(auto nodeInstPortPair : nodeInstPorts){
				hcmInstance* instToSim = nodeInstPortPair.second->getInst();
                if(nodeInstPortPair.second->getPort()->getDirection() == IN){
					simulate(instToSim, time);
                }
            }
            eventQ.pop();
        }

		// go over all values and write them to the vcd.
		list<const hcmInstance *> parents;
		for(auto nodePair : nodesMap){
			if(nodePair.first != "VDD" && nodePair.first != "VSS"){
				const hcmNodeCtx nodeCtx(parents, nodePair.second);
				bool value;
				nodePair.second->getProp("value", value);
				vcd.changeValue(&nodeCtx, value);
			}
		}

		updateFFsPrevVal(flatCell);
		
		vcd.changeTime(time++);
	}

	//-----------------------------------------------------------------------------------------//
}

void updateFFsPrevVal(hcmCell* flatCell){
	map<string,hcmInstance*> instPairs = flatCell->getInstances();
	for(auto instPair : instPairs){
		if(instPair.second->masterCell()->getName() == "dff"){
			map<string,hcmInstPort*> instPortPairs = instPair.second->getInstPorts();
			for(auto instPortPair : instPortPairs){
				if(instPortPair.second->getPort()->getDirection() == IN){
					hcmNode* node = instPortPair.second->getNode();
					bool prevValue;
					node->getProp("value", prevValue);
					node->setProp("prevValue", prevValue);
				}	
			}
		}
	}
}

void setNodeValue(hcmNode* node, bool value){
	map<string, hcmInstPort*> nodeInstPorts = node->getInstPorts();
	bool prevValue;
	for(auto instPortPair : nodeInstPorts){
		if(instPortPair.second->getInst()->getName() == "dff" && (instPortPair.second->getPort()->getName() == "CLK" || instPortPair.second->getPort()->getName() == "D")){
			node->getProp("value", prevValue);
			node->setProp("prevValue", prevValue);
			break;
		}
	}
	node->setProp("value", value);
}

void simulate(hcmInstance* inst, int time){
	map<string, bool> inputs;
	map<string, hcmInstPort*> instPorts = inst->getInstPorts();
	hcmNode* outNode;
	bool newOutVal;
	bool currOutVal;
	for(auto instPortPair : instPorts){
		hcmPort* port = instPortPair.second->getPort();
		if(port->getDirection() == IN){
			instPortPair.second->getNode()->getProp("value", inputs[port->getName()]);
			if(inst->masterCell()->getName() == "dff"){
				if(port->getName() == "CLK"){
					instPortPair.second->getNode()->getProp("prevValue", inputs["PREV_CLK"]);
				}
				else if(port->getName() == "D"){
					instPortPair.second->getNode()->getProp("prevValue", inputs["PREV_D"]);
				}
			}
		}
		else if(port->getDirection() == OUT){
			outNode = instPortPair.second->getNode();
		}
	}
	bool currOutNotFound = false;
	if(outNode->getProp("value", currOutVal) == NOT_FOUND){
		currOutNotFound = true;
		currOutVal = false;
	}
	newOutVal = simGate(inst->masterCell(), inputs, currOutVal);
	if(currOutNotFound || newOutVal != currOutVal){
		eventQ.push(outNode);
		setNodeValue(outNode, newOutVal);
	}
}

bool simAnd(map<string,bool> inputs){
	bool result = 1;
	for(auto inputPair : inputs){
		result &= inputPair.second;
	}
	return result;
}

bool simOr(map<string,bool> inputs){
	bool result = 0;
	for(auto inputPair : inputs){
		result |= inputPair.second;
	}
	return result;
}

bool simNand(map<string,bool> inputs){
	return !simAnd(inputs);
}

bool simNor(map<string,bool> inputs){
	return !simOr(inputs);
}

bool simXor(map<string,bool> inputs){
	bool result = 0;
	for(auto inputPair : inputs){
		result ^= inputPair.second;
	}
	return result;
}

bool simBuffer(map<string,bool> inputs){
	return inputs.begin()->second;
}

bool simInv(map<string,bool> inputs){
	return !inputs.begin()->second;
}

bool simDff(map<string,bool> inputs, bool currQ){
	if(inputs["CLK"] == 1 && inputs["PREV_CLK"] == 0){
		return inputs["PREV_D"];
	}
	return currQ;
}



bool simGate(hcmCell* gate, map<string,bool> inputs, bool currOutVal){

	string gateName = gate->getName();

	if(gateName.find("nand") != string::npos){
		return simNand(inputs);
	}

	else if(gateName.find("nor") != string::npos){
		return simNor(inputs);
	}

	else if(gateName.find("xor") != string::npos){
		return simXor(inputs);
	}

	else if(gateName.find("buffer") != string::npos){
		return simBuffer(inputs);
	}

	else if(gateName.find("inv") != string::npos){
		return simInv(inputs);
	}

	else if(gateName.find("not") != string::npos){
		return simInv(inputs);
	}
	else if(gateName.find("and") != string::npos){
		return simAnd(inputs);
	}

	else if(gateName.find("or") != string::npos){
		return simOr(inputs);
	}

	else if(gateName.find("dff") != string::npos){
		return simDff(inputs, currOutVal);
	}
}