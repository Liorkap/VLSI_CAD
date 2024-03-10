#include <errno.h>
#include <signal.h>
#include <sstream>
#include <fstream>
#include "hcm.h"
#include "flat.h"
#include <list>
using namespace std;

typedef struct{
  string nodeName;
  int depth;
} nodeInfo;

int countInsts(hcmCell *rootCell, string cellName, set<string> *visitedCells);
map<string,hcmNode*> getCellNonGlobalNodes(hcmCell* cell, set<string>* globalNodes);
int maxNodeReach(hcmCell *rootCell, set<string>* globalNodes);
int calcNodeReach(hcmNode* node, map<string,vector<nodeInfo>>* knwonCells);
void getPrimitiveCellsNodesPaths(hcmCell* cell, set<string>* globalNodes, int deepestReach, list<string>* deepestNodesPaths, string path = "");
long unsigned int countHierarchies(string path);


bool verbose = false;

int main(int argc, char **argv) {
  int argIdx = 1;
  int anyErr = 0;
  unsigned int i;
  vector<string> vlgFiles;
  
  if (argc < 3) {
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
  
  /*direct to file*/
  string fileName = cellName + string(".stat");
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
  
  hcmCell *flatCell = hcmFlatten(cellName + string("_flat"), topCell, globalNodes);
  cout << "-I- Top cell flattened" << endl;
  
  fv << "file name: " << fileName << endl;
  
	// section a: find all nodes in top module. excluding global nodes (VSS and VDD).
  /* assign your answer for section a to topLevelNodeCounter */
  int topLevelNodeCounter = 0;
  //---------------------------------------------------------------------------------//
  map<string,hcmNode*> topCellNodes = topCell->getNodes();
  
  topLevelNodeCounter = getCellNonGlobalNodes(topCell, &globalNodes).size();
  //---------------------------------------------------------------------------------//
	fv << "a: " << topLevelNodeCounter << endl;

	// section b: find number of instances in top level cell.
  /* assign your answer for section b to topLevelInstanceCounter */
  int topLevelInstanceCounter = 0;
  //---------------------------------------------------------------------------------//
  map<string,hcmInstance*> topCellInstances = topCell->getInstances();
  topLevelInstanceCounter = topCellInstances.size();
  //---------------------------------------------------------------------------------//
	fv << "b: " << topLevelInstanceCounter << endl;

	//section c: find cellName instances in folded model.
  /* assign your answer for section c to cellNameFoldedCounter */
  int cellNameFoldedCounter = 0;
  //---------------------------------------------------------------------------------//
  set<string> visitedCellsFolded;
  cellNameFoldedCounter = countInsts(topCell, "and4", &visitedCellsFolded);
  //---------------------------------------------------------------------------------//
	fv << "c: " << cellNameFoldedCounter << endl;

	//section d: find cellName instances in entire hierarchy, using flat cell.
  /* assign your answer for section d to cellNameFlatendCounter */
  int cellNameFlatendCounter = 0;
  //---------------------------------------------------------------------------------//
  set<string> visitedCellsFlat;
  cellNameFlatendCounter = countInsts(flatCell, "and4", &visitedCellsFlat);
  //---------------------------------------------------------------------------------//
	fv << "d: " << cellNameFlatendCounter << endl;

	//section e: find the deepest reach of a top level node.
  /* assign your answer for section e to deepestReach */
  int deepestReach = 1;
  //---------------------------------------------------------------------------------//
  deepestReach = maxNodeReach(topCell, &globalNodes);
  //---------------------------------------------------------------------------------//
	fv << "e: " << deepestReach << endl;

	//section f: find hierarchical names of deepest reaching nodes.
  /* assign your answer for section f to listOfHierarchicalNameOfDeepestReachingNodes */
  list<string> listOfHierarchicalNameOfDeepestReachingNodes;
  //---------------------------------------------------------------------------------//
  getPrimitiveCellsNodesPaths(topCell, &globalNodes, deepestReach, &listOfHierarchicalNameOfDeepestReachingNodes);
  int maxDepth = 0;
  for(auto it = listOfHierarchicalNameOfDeepestReachingNodes.begin(); it != listOfHierarchicalNameOfDeepestReachingNodes.end();it++){
    int depth = countHierarchies(*it);
    maxDepth = (depth > maxDepth) ? depth : maxDepth;
  }
  for(auto it = listOfHierarchicalNameOfDeepestReachingNodes.begin(); it != listOfHierarchicalNameOfDeepestReachingNodes.end();){
    int depth = countHierarchies(*it);
    if(depth < maxDepth){
      it = listOfHierarchicalNameOfDeepestReachingNodes.erase(it);
    }
    else{
      it++;
    }
  }
  listOfHierarchicalNameOfDeepestReachingNodes.sort();
  //---------------------------------------------------------------------------------//
  for (auto it : listOfHierarchicalNameOfDeepestReachingNodes) {
    // erase the '/' in the beginning of the hierarchical name. 
    // i.e. the name in listOfHierarchicalNameOfDeepestReachingNodes should be "/i1/i2/i3/i5/N1", 
    // and the printed name should be "i1/i2/i3/i5/N1".
    it.erase(0,1); 
    fv << it << endl;
  }

  return(0);
}

map<string,hcmNode*> getCellNonGlobalNodes(hcmCell* cell, set<string>* globalNodes){
  map<string,hcmNode*> cellNodes = cell->getNodes();
  for(auto it = cellNodes.begin(); it != cellNodes.end();){
    if(globalNodes->count(it->first) == 1){
      it = cellNodes.erase(it);
    }
    else{
      it++;
    }
  }
  return cellNodes;
}


int countInsts(hcmCell *rootCell, string cellName, set<string> *visitedCells){
  map<string,hcmInstance*> rootCellInstances = rootCell->getInstances();
  int instCounter = 0;
  if(rootCellInstances.empty() || visitedCells->count(rootCell->getName()) == 1){
    return 0;
  }
  for(auto it = rootCellInstances.begin(); it != rootCellInstances.end(); it++){
    if(it->second->masterCell()->getName() == cellName){
      instCounter++;
    }
    else{
      instCounter += countInsts(it->second->masterCell(), cellName, visitedCells); 
      visitedCells->insert(it->second->masterCell()->getName());
    }
  }
  return instCounter;
}

int maxNodeReach(hcmCell *rootCell, set<string>* globalNodes){


  map<string,vector<nodeInfo>> knownCells;
  map<string,hcmNode*> rootCellNodes = getCellNonGlobalNodes(rootCell, globalNodes);
  int maxReach = 0;

  for(auto it = rootCellNodes.begin(); it != rootCellNodes.end(); it++){
    int nodeReach = calcNodeReach(it->second, &knownCells);
    maxReach = (nodeReach > maxReach) ? nodeReach : maxReach;
  }

  return maxReach;
}

int calcNodeReach(hcmNode* node, map<string,vector<nodeInfo>>* knownCells){
  map<string,hcmInstPort*> nodeInstPorts = node->getInstPorts();
  hcmCell* nodeOwner = node->owner();
  nodeInfo thisNodeInfo;

  thisNodeInfo.nodeName = node->getName();

  if(nodeInstPorts.empty()){
    return 1;
  }

  if(knownCells->count(nodeOwner->getName()) == 1){
    vector<nodeInfo> knownCellNodes = knownCells->find(nodeOwner->getName())->second;
    for(auto it = knownCellNodes.begin(); it != knownCellNodes.end(); it++){
      if(it->nodeName == thisNodeInfo.nodeName){
        return it->depth;
      }
    }
  }

  int maxDepth = 0;
  for(auto it = nodeInstPorts.begin(); it != nodeInstPorts.end(); it++){
    int nodeReach = calcNodeReach(it->second->getPort()->owner(), knownCells);
    maxDepth = (nodeReach > maxDepth) ? nodeReach : maxDepth;
  }

  thisNodeInfo.depth = maxDepth + 1;
  if(knownCells->count(nodeOwner->getName()) == 1){
    (*knownCells)[nodeOwner->getName()].push_back(thisNodeInfo);
  }
  else{
    vector<nodeInfo> ownerCellKnownNodes;
    ownerCellKnownNodes.push_back(thisNodeInfo);
    knownCells->insert({nodeOwner->getName(), ownerCellKnownNodes});
  }

  return maxDepth + 1;
  
}

void getPrimitiveCellsNodesPaths(hcmCell* cell, set<string>* globalNodes, int deepestReach, list<string>* deepestNodesPaths, string path /*= ""*/){
  map<string,hcmInstance*> cellInsts = cell->getInstances();
  if(cellInsts.size() == 0){
    for(auto it = cell->getNodes().begin(); it != cell->getNodes().end(); it++){
      if(globalNodes->count(it->first) == 0){
        deepestNodesPaths->push_back(path + "/" + it->first);
      }
    }
  }

  for(auto it = cellInsts.begin(); it != cellInsts.end(); it++){
    getPrimitiveCellsNodesPaths(it->second->masterCell(), globalNodes, deepestReach, deepestNodesPaths, path + "/" + it->second->getName());
  }
}

long unsigned int countHierarchies(string path){
  int hierCnt = 0;
  for(long unsigned int i = 0; i < path.size(); i++){
    if(path[i] == '/'){
      hierCnt++;
    }
  }
  return hierCnt;
}
