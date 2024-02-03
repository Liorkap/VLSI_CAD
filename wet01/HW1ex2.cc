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
bool isRealPort (hcmPort *port);
map <hcmInstance *, int> rankmap;
map <hcmInstance *, string> namemap;
vector <hcmPort *> designPorts;
vector <string> designPortNames;
map <string,hcmNode *> designNodes;
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
  

  /* assign your answer for HW1ex2 to maxRankVector 
   * maxRankVector is a vector of pairs of type <int, string>,
   * int is the rank of the instance,
   * string is the name of the instance
   * Note - maxRankVector should be sorted.
  */
  vector<pair<int, string>> maxRankVector;
  hcmInstance *temp;
  map <string,hcmInstance *> instmap = topCell->getInstances();
  map <string,hcmInstance *> instmap2;
   designPorts = topCell->getPorts();
    designNodes = topCell->getNodes();
  for(int i =0; i< designPorts.size(); i++){
		// cout << designPorts[i]->getName() << " is a des port " << designPorts[i] << endl;
    designPortNames.push_back(designPorts[i]->getName());
	}
  for(auto itr = instmap.begin(); itr != instmap.end(); itr++){
    temp = topCell->getInst(itr->first);
    getFullName(itr->second, itr->first + '/');
    // instmap2 =(temp->masterCell()->getInstances()); // to run on m2
	}
  instmap2 = instmap.begin()->second->masterCell()->getInstances(); // to run on M1
  for(auto itr = instmap2.begin(); itr != instmap2.end(); itr++){
    rankmap[itr->second] = getInputPorts(itr->second);
    // fv << "and it has " << getInputPorts(itr->second) << " input ports" << endl; 
	}
   pair<int,string> tempPair;
  for(auto itr = rankmap.begin(); itr != rankmap.end(); itr++){
		// cout << "inst : " << namemap[itr->first] << " rank : " << itr->second << endl;
    tempPair = make_pair(itr->second,namemap[itr->first]);
    maxRankVector.push_back(tempPair);
    // fv << "and it has " << getInputPorts(itr->second) << " input ports" << endl; 
	}
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

int getInputPorts (hcmInstance *inst){
    map <string,hcmInstPort *> instports;
    instports = inst->getInstPorts();
    // cout << "arrive here4" << endl;
    for(auto itr = instports.begin(); itr != instports.end();){
      if (itr->second->getPort()->getDirection() == 2){
        itr = instports.erase(itr);
      }
      else {
        // cout << itr->second << "issue" << endl;
        // cout << "inst conncted to inst : " << getPerviousPort(itr->second->getNode()) << endl;
        itr++;
      }
    // cout << "arrive here" << endl;

	}
    int maxrank =-1;
    int temprank;
    for(auto itr = instports.begin(); itr != instports.end();itr++){
      temprank = getPerviousPort(itr->second->getNode());
      if (temprank > maxrank){
        maxrank = temprank;
      }

	}
  
  return maxrank;
}

int getPerviousPort(hcmNode *node ) {

  // hcmNode *node = instport->getNode();
  map <string,hcmInstPort *> instports = node->getInstPorts();
  // instports.erase(instport->getName());
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
          return 1 + rankmap[instports.begin()->second->getInst()];
        }
        else {
          return 1 + getInputPorts(instports.begin()->second->getInst());
        }

      }
      else {
        // cout << " this rank is zero" << endl;
        // auto it = find(designPorts.begin(), designPorts.end(), node->getPort());
        // if (it != designPorts.end() ){

        //   return instports.size();
        // }
        // else {
        //   return 1 + getPerviousPort(node->getPort()->owner() );
        // }
        // cout << node->getPort()->owner()->getPort()->owner()->getPort()->owner()->getName() << " connected to port : " << node->getPort() << endl;
        // if(isRealPort(node->getPort()->owner()->getPort()->owner()->getPort() )){
        //   cout << "this is real port" << endl;
        // }
        // else {
        //   // cout << "num inst ports for this node "<<node->getInstPorts().size()<< endl;
        //   // return 1 + getPerviousPort(node->getPort()->owner());
        // }
        return instports.size();
      }

  
}


bool hasRank (hcmInstance *inst){
  auto it = rankmap.find(inst);
  return it != rankmap.end();

}

void getFullName(hcmInstance *inst, string relName){
  if (isPrimitive(inst)){
    // cout << inst->getName() << "rel is : " << relName << endl; 
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

bool isPrimitive (hcmInstance *inst){
  int size = inst->masterCell()->getInstances().size() ;
  // cout <<" size is : " << size << endl;
  return (size == 0);

}

bool isRealPort (hcmPort *port){
    

   auto it =find(designPortNames.begin(), designPortNames.end(), port->getName());

    return (it != designPortNames.end()) ;

}