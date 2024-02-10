#include <errno.h>
#include <signal.h>
#include <sstream>
#include <fstream>
#include "hcm.h"
#include "flat.h"
#include <vector>
#include <algorithm>
using namespace std;

bool verbose = false;

int getInputPorts (hcmInstance *inst);
int getPerviousPort(hcmNode *node );
bool hasRank (hcmInstance *inst);
void getFullName(hcmInstance *inst, string relName);
bool isPrimitive (hcmInstance *inst);
map <hcmInstance *, int> rankmap;
map <hcmInstance *, string> namemap;
int main(int argc, char **argv) {
  int argIdx = 1;
  int anyErr = 0;
  unsigned int i;
  vector<string> vlgFiles;
  
  if (argc < 3) {
    anyErr++;
  } else {
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
    cerr << "Usage: " << argv[0] << "  [-v] top-cell file1.v [file2.v] ... \n";
    exit(1);
  }
 
  set< string> globalNodes;
  globalNodes.insert("VDD");
  globalNodes.insert("VSS");
  
  hcmDesign* design = new hcmDesign("design");
  string cellName = vlgFiles[0];
  for (i = 1; i < vlgFiles.size(); i++) {
    printf("-I- Parsing verilog %s ...\n", vlgFiles[i].c_str());
    if (!design->parseStructuralVerilog(vlgFiles[i].c_str())) {
      cerr << "-E- Could not parse: " << vlgFiles[i] << " aborting." << endl;
      exit(1);
    }
  }
  

  /*direct output to file*/
  string fileName = cellName + string(".rank");
  ofstream fv(fileName.c_str());
  if (!fv.good()) {
    cerr << "-E- Could not open file:" << fileName << endl;
    exit(1);
  }

  hcmCell *topCell = design->getCell(cellName);
  if (!topCell) {
    printf("-E- could not find cell %s\n", cellName.c_str());
    exit(1);
  }
  
  fv << "file name: " << fileName << endl;
  
  hcmCell *flatCell = hcmFlatten(cellName + string("_flat"), topCell, globalNodes);

  /* assign your answer for HW1ex2 to maxRankVector 
   * maxRankVector is a vector of pairs of type <int, string>,
   * int is the rank of the instance,
   * string is the name of the instance
   * Note - maxRankVector should be sorted.
  */
  vector<pair<int, string>> maxRankVector;
  map <string,hcmInstance *> instmap = flatCell->getInstances();
  for(auto itr = instmap.begin(); itr != instmap.end(); itr++){
    getFullName(itr->second, ""); // for flat inst
	}
  for(auto itr = instmap.begin(); itr != instmap.end(); itr++){
    getInputPorts(itr->second);
	}
   pair<int,string> tempPair;
  for(auto itr = rankmap.begin(); itr != rankmap.end(); itr++){
    if(itr->second != -1) // if has -1 rank it's connected to VDD/VSS
    {
      tempPair = make_pair(itr->second,namemap[itr->first]);
      maxRankVector.push_back(tempPair);
    }
	}
  // sorting the vector first by rank then by name
  sort(maxRankVector.begin(),maxRankVector.end(),[](const auto& a, const auto& b) {
        if (a.first != b.first) {
          return a.first < b.first;
        }
        return a.second < b.second;
    });
  //---------------------------------------------------------------------------------//
  //enter your code here 
  //---------------------------------------------------------------------------------//
  for(auto itr = maxRankVector.begin(); itr != maxRankVector.end(); itr++){
		fv << itr->first << " " << itr->second << endl;
	}

	
  return(0);
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
// returns the full name of the inst (not needed for flat topology)
void getFullName(hcmInstance *inst, string relName){
  if (isPrimitive(inst)){
    namemap[inst] = relName + inst->getName();
    
  }
  else{
    string relative = relName + inst->getName() + '/';
    map <string,hcmInstance *> relinsts = inst->masterCell()->getInstances();
    for(auto itr = relinsts.begin(); itr != relinsts.end();itr++){
      getFullName(itr->second,relName);
	}

  }
}
// checks if the inst is primitive ( not needed for flat topology)
bool isPrimitive (hcmInstance *inst){
  int size = inst->masterCell()->getInstances().size() ;
  return (size == 0);

}
