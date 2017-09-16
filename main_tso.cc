//Reddem Karan Reddy
//Part-3 with test set and runtime optimization

#include <iostream>
#include <fstream> 
#include <vector>
#include <queue>
#include <time.h>
#include <stdio.h>
#include "parse_bench.tab.h"
#include "ClassCircuit.h"
#include "ClassGate.h"
#include <limits>
#include <stdlib.h>
#include <time.h>

using namespace std;

/**  @brief Just for the parser*/
extern "C" int yyparse();

/**  Input file for parser.*/
extern FILE *yyin;

/** Our circuit*/
extern Circuit* myCircuit;

//--------------------------
// Helper functions
void printUsage();
vector<char> constructInputLine(string line);
bool checkTest(Circuit* myCircuit);
string printPIValue(char v);
//--------------------------

//----------------------------
// Functions for logic simulation
void simFullCircuit(Circuit* myCircuit);
void simGateRecursive(Gate* g);
void eventDrivenSim(Circuit* myCircuit, queue<Gate*> q);
char simGate(Gate* g);
char evalGate(vector<char> in, int c, int i);
char EvalXORGate(vector<char> in, int inv);
int LogicNot(int logicVal);
void setValueCheckFault(Gate* g, char gateValue);
//-----------------------------

//----------------------------
// Functions for PODEM:
bool podemRecursion(Circuit* myCircuit);
bool getObjective(Gate* &g, char &v, Circuit* myCircuit);
void updateDFrontier(Circuit* myCircuit);
void backtrace(Gate* &pi, char &piVal, Gate* objGate, char objVal, Circuit* myCircuit);
char getNonControllingValue(Gate *g);
void eventDrivenSimPart3(Circuit* myCircuit, queue<Gate*> q);
//-----------------------------


///////////////////////////////////////////////////////////
// Global variables
/** Global variable: a vector of Gate pointers for storing the D-Frontier. */
vector<Gate*> dFrontier;

/** Global variable: holds a pointer to the gate with stuck-at fault on its output location. */
Gate* faultLocation;     

/** Global variable: holds the logic value to activate the stuck-at fault. */
char faultActivationVal;

/** Global variable to store all the gates with output D or DBAR in the current circuit state. */
vector<Gate*> gatesWithDetectedFaults;

///////////////////////////////////////////////////////////

// A struct to represent a fault.
// loc is a pointer to the gate output where this fault is located
// val is 0 or 1 for SA0 or SA1
struct faultStruct {
  Gate* loc;
  char val;
};

int main(int argc, char* argv[]) {

  // Check the command line input and usage
  if (argc != 5) {
    printUsage();    
    return 1;
  }
  
  int mode = atoi(argv[1]);
  if ((mode < 0) || (mode > 1)) {
    printUsage();    
    return 1;   
  }


  // Parse the bench file and initialize the circuit. (Using C style for our parser.)
  FILE *benchFile = fopen(argv[2], "r");
  if (benchFile == NULL) {
    cout << "ERROR: Cannot read file " << argv[2] << " for input" << endl;
    return 1;
  }
  yyin=benchFile;
  yyparse();
  fclose(benchFile);

  myCircuit->setupCircuit(); 
  cout << endl;

  // Setup the output text file
  ofstream outputStream;
  outputStream.open(argv[3]);
  if (!outputStream.is_open()) {
    cout << "ERROR: Cannot open file " << argv[3] << " for output" << endl;
    return 1;
  }
   
  // Open the fault file.
  ifstream faultStream;
  string faultLocStr;
  faultStream.open(argv[4]);
  if (!faultStream.is_open()) {
    cout << "ERROR: Cannot open fault file " << argv[4] << " for input" << endl;
    return 1;
  }
  
  vector<faultStruct> faultList;

  // Go through each lone in the fault file, and add the faults to faultList.
  while(getline(faultStream, faultLocStr)) {
    string faultTypeStr;
      
    if (!(getline(faultStream, faultTypeStr))) {
      break;
    }
      
    char faultType = atoi(faultTypeStr.c_str());
    Gate* fl = myCircuit->findGateByName(faultLocStr);

    faultStruct fs = {fl, faultType};
    faultList.push_back(fs);
  }
  faultStream.close();

  if (mode == 0) {   // mode 0 means the system will run PODEM for each fault in the faultList.
      for (int faultNum = 0; faultNum < faultList.size(); faultNum++) {

       // Clear the old fault
       myCircuit->clearFaults();
         
       // set up the fault we are trying to detect
       faultLocation = faultList[faultNum].loc;
       char faultType = faultList[faultNum].val;
       faultLocation->set_faultType(faultType);      
       faultActivationVal = (faultType == FAULT_SA0) ? LOGIC_ONE : LOGIC_ZERO;
         
       // set all gate values to X
       for (int i=0; i < myCircuit->getNumberGates(); i++) {
         myCircuit->getGate(i)->setValue(LOGIC_X);
       }

       // initialize the D frontier.
       dFrontier.clear();
       
       // call PODEM recursion function
       bool res = podemRecursion(myCircuit);

       // If we succeed, print the test we found to the output file.
       if (res == true) {
         vector<Gate*> piGates = myCircuit->getPIGates();
         for (int i=0; i < piGates.size(); i++)
           outputStream << printPIValue(piGates[i]->getValue());
         outputStream << endl;
       }

       // If we failed to find a test, print a message to the output file
       else {
         outputStream << "none found" << endl;
       }

       if (res == true) {
         if (!checkTest(myCircuit)) {
           cout << "ERROR: PODEM returned true, but generated test does not detect fault on PO." << endl;
           myCircuit->printAllGates();
           assert(false);
         }
       }
             
       cout << "Fault = " << faultLocation->get_outputName() << " / " << (int)(faultType) << ";";
       if (res == true)
         cout << " test found; " << endl;
       else
         cout << " no test found; " << endl;
     }
  }
  else {   // mode 1 - trying to optimize the test set size (Part 3).
	//A vector to store all indexes of all the faults in the faultList that have already been detected
	vector<int> indexesToSkip;
	for (int faultNum = 0; faultNum < faultList.size(); faultNum++) {

		faultsDetected.clear();
		
		bool skipFault = false;
	       	for(int i = 0; i < indexesToSkip.size(); i++){
			if(faultNum == indexesToSkip[i]){
				skipFault = true;
				break;
			}
	       	}
		if(skipFault) continue;

	       // Clear the old fault
	       myCircuit->clearFaults();
		 
	       // set up the fault we are trying to detect
	       faultLocation = faultList[faultNum].loc;
	       char faultType = faultList[faultNum].val;
	       faultLocation->set_faultType(faultType);      
	       faultActivationVal = (faultType == FAULT_SA0) ? LOGIC_ONE : LOGIC_ZERO;
		 
	       // set all gate values to X
	       for (int i=0; i < myCircuit->getNumberGates(); i++) {
		 myCircuit->getGate(i)->setValue(LOGIC_X);
	       }

	       // initialize the D frontier.
	       dFrontier.clear();
	       
	       // call PODEM recursion function
	       bool res = podemRecursion(myCircuit);

	       // If we succeed, print the test we found to the output file.
	       if (res == true) {
		 vector<Gate*> piGates = myCircuit->getPIGates();
		 for (int i=0; i < piGates.size(); i++)
		   outputStream << printPIValue(piGates[i]->getValue());
		 outputStream << endl;
	       }

	       // If we failed to find a test, print a message to the output file
	       else {
		 outputStream << "none found" << endl;
	       }


		cout << "Fault = " << faultLocation->get_outputName() << " / " << (int)(faultType) << ";";
		if (res == true)
		 cout << " test found; " << endl;
		else
		 cout << " no test found; " << endl;

		vector<int> indexesToDelete;
		indexesToDelete.clear();
	       if (res == true) {
		 if (!checkTest(myCircuit)) {
		   cout << "ERROR: PODEM returned true, but generated test does not detect fault on PO." << endl;
		   myCircuit->printAllGates();
		   assert(false);
		 }
		 for (int j = 0; j < faultsDetected.size(); j++) {
		       myCircuit->clearFaults();
		       faultLocation = faultsDetected[j].loc;
		       faultType = faultsDetected[j].val;
		       faultLocation->set_faultType(faultType);  
		       vector<Gate*> tempPIs = myCircuit->getPIGates();
			if (!checkTest(myCircuit)) {
				bool checkAfterXFlip = false;
				for(int tempPiIndex = 0; tempPiIndex < tempPIs.size(); tempPiIndex++){
					if(tempPIs[tempPiIndex]->getValue() == LOGIC_X){
						tempPIs[tempPiIndex]->setValue(LOGIC_ONE);
					}
					checkAfterXFlip = checkTest(myCircuit);
					if(checkAfterXFlip) break;
					else{
						tempPIs[tempPiIndex]->setValue(LOGIC_ZERO);
				
					}
					checkAfterXFlip = checkTest(myCircuit);
					if(checkAfterXFlip) break;
				}
				if(checkAfterXFlip){
					for (int tempPiIndex=0; tempPiIndex < tempPIs.size(); tempPiIndex++)
					   outputStream << printPIValue(tempPIs[tempPiIndex]->getValue());
					 outputStream << endl;
				} else{
					indexesToDelete.push_back(j);
					/*cout << "ERROR: PODEM returned true, but generated test does not detect fault on PO for " << faultsDetected[j].loc->get_outputName() << "/" << (int)faultsDetected[j].val << endl;
					myCircuit->printAllGates();
					assert(false);*/
				}
			}
		 }
	       }

		for(int idx = indexesToDelete.size(); idx > indexesToDelete.size(); idx--){
			faultsDetected.erase(faultsDetected.begin() + indexesToDelete[idx]);
		}

	       // If a test is found, check whether it detects other faults in the fault list as well
	       if (res == true) {
		 for (int i = faultNum+1; i < faultList.size(); i++){
		   for(int j = 0; j < faultsDetected.size(); j++){
			if((faultList[i].loc->get_outputName() == faultsDetected[j].loc->get_outputName()) && (faultList[i].val == faultsDetected[j].val)){
				indexesToSkip.push_back(i);
			}
		   }
		 }
	       }
     }
  }
  
  outputStream.close();

    
  return 0;
}

void printUsage() {
  cout << "Usage: ./atpg [mode] [bench_file] [output_loc] [fault_file]" << endl << endl;
  cout << "   mode:          0 for normal mode, 1 to minimize test set size" << endl;
  cout << "   bench_file:    the target circuit in .bench format" << endl;
  cout << "   output_loc:    location for output file" << endl;
  cout << "   fault_file:    faults to be considered" << endl;
  cout << endl;
  cout << "   The system will generate a test pattern for each fault listed" << endl;
  cout << "   in fault_file and store the result in output_loc." << endl;
  cout << endl;	
}


vector<char> constructInputLine(string line) {
  
  vector<char> inputVals;
  
  for (int i=0; i<line.size(); i++) {
    if (line[i] == '0') 
      inputVals.push_back(LOGIC_ZERO);
    
    else if (line[i] == '1') 
      inputVals.push_back(LOGIC_ONE);

    else if ((line[i] == 'X') || (line[i] == 'x')) {
      inputVals.push_back(LOGIC_X);
    }
   
    else {
      cout << "ERROR: Do not recognize character " << line[i] << " in line " << i+1 << " of input vector file." << endl;
      assert(false);
      //inputVals.push_back(LOGIC_X);
    }
  }  
  return inputVals;
}
  
  vector<char> inputVals;
  
  for (int i=0; i<line.size(); i++) {
    if (line[i] == '0') 
      inputVals.push_back(LOGIC_ZERO);
    
    else if (line[i] == '1') 
      inputVals.push_back(LOGIC_ONE);

    else if ((line[i] == 'X') || (line[i] == 'x')) {
      inputVals.push_back(LOGIC_X);
    }
   
    else {
      cout << "ERROR: Do not recognize character " << line[i] << " in line " << i+1 << " of input vector file." << endl;
      assert(false);
      //inputVals.push_back(LOGIC_X);
    }
  }  
  return inputVals;
}

/** @brief Uses the simulator to check validity of the test.
 * 
 * This function gets called after the PODEM algorithm finishes.
 * If this is enabled, it will clear the circuit's internal values,
 * and re-simulate the vector PODEM found to test your result.
*/
bool checkTest(Circuit* myCircuit) {

  simFullCircuit(myCircuit);

  // look for D or D' on an output
  vector<Gate*> poGates = myCircuit->getPOGates();
  for (int i=0; i<poGates.size(); i++) {
    char v = poGates[i]->getValue();
    if ((v == LOGIC_D) || (v == LOGIC_DBAR)) {
      return true;
    }
  }

  // If we didn't find D or D' on any PO, then our test was not successful.
  return false;

}

/** @brief Prints a PI value. 
 * 
 */
string printPIValue(char v) {
  switch(v) {
  case LOGIC_ZERO: return "0";
  case LOGIC_ONE: return "1";
  case LOGIC_UNSET: return "U";
  case LOGIC_X: return "X";
  case LOGIC_D: return "1";
  case LOGIC_DBAR: return "0";
  }
  return "";
}

/** @brief Runs full circuit simulation
 *
 * Full-circuit simulation: set all non-PI gates to LOGIC_UNSET
 * and call the recursive simulate function on all PO gates.
 */
void simFullCircuit(Circuit* myCircuit) {
  for (int i=0; i<myCircuit->getNumberGates(); i++) {
    Gate* g = myCircuit->getGate(i);
    if (g->get_gateType() != GATE_PI)
      g->setValue(LOGIC_UNSET);      
  }  
  vector<Gate*> circuitPOs = myCircuit->getPOGates();
  for (int i=0; i < circuitPOs.size(); i++) {
    simGateRecursive(circuitPOs[i]);
  }
}

// Recursive function to find and set the value on Gate* g.
// This function calls simGate and setValueCheckFault.
/** @brief Recursive function to find and set the value on Gate* g.
 * \param g The gate to simulate.
 * This function prepares Gate* g to to be simulated by recursing
 * on its inputs (if needed).
 * 
 * Then it calls \a simGate(g) to calculate the new value.
 * 
 * Lastly, it will set the Gate's output value based on
 * the calculated value.
 */
void simGateRecursive(Gate* g) {

  // If this gate has an already-set value, you are done.
  if (g->getValue() != LOGIC_UNSET)
    return;
  
  // Recursively call this function on this gate's predecessors to
  // ensure that their values are known.
  vector<Gate*> pred = g->get_gateInputs();
  for (int i=0; i<pred.size(); i++) {
    simGateRecursive(pred[i]);
  }
  
  char gateValue = simGate(g);

  // After I have calculated this gate's value, check to see if a fault changes it and set.
  setValueCheckFault(g, gateValue);
}


/** @brief Perform event-driven simulation.
 * This function takes as input the Circuit* and a queue<Gate*>
 * indicating the remaining gates that need to be evaluated.
 */
void eventDrivenSim(Circuit* myCircuit, queue<Gate*> q) {
	while(!q.empty()){
		Gate* g = q.front();
		vector<Gate*> gateOutputs = g->get_gateOutputs();
		for(int i=0; i < gateOutputs.size(); i++){
			char prevValue = gateOutputs[i]->getValue();
			char newValue = simGate(gateOutputs[i]);
			setValueCheckFault(gateOutputs[i], newValue);
			if(prevValue != gateOutputs[i]->getValue()){
				q.push(gateOutputs[i]);
			}
		}
		q.pop();
	}
}


/** @brief Simulate the value of the given Gate.
 *
 * This is a gate simulation function -- it will simulate the gate g
 * with its current input values and return the output value.
 * This function does not deal with the fault. *
 */
char simGate(Gate* g) {
  // For convenience, create a vector of the values of this
  // gate's inputs.
  vector<Gate*> pred = g->get_gateInputs();
  vector<char> inputVals;   
  for (int i=0; i<pred.size(); i++) {
    inputVals.push_back(pred[i]->getValue());      
  }

  char gateType = g->get_gateType();
  char gateValue;
  // Now, set the value of this gate based on its logical function and its input values
  switch(gateType) {   
  case GATE_NAND: { gateValue = evalGate(inputVals, 0, 1); break; }
  case GATE_NOR: { gateValue = evalGate(inputVals, 1, 1); break; }
  case GATE_AND: { gateValue = evalGate(inputVals, 0, 0); break; }
  case GATE_OR: { gateValue = evalGate(inputVals, 1, 0); break; }
  case GATE_BUFF: { gateValue = inputVals[0]; break; }
  case GATE_NOT: { gateValue = LogicNot(inputVals[0]); break; }
  case GATE_XOR: { gateValue = EvalXORGate(inputVals, 0); break; }
  case GATE_XNOR: { gateValue = EvalXORGate(inputVals, 1); break; }
  case GATE_FANOUT: {gateValue = inputVals[0]; break; }
  default: { cout << "ERROR: Do not know how to evaluate gate type " << gateType << endl; assert(false);}
  }    

  return gateValue;
}

/** @brief Evaluate a NAND, NOR, AND, or OR gate.
 * \param in The logic value's of this gate's inputs.
 * \param c The controlling value of this gate type (e.g. c==0 for an AND or NAND gate)
 * \param i The inverting value for this gate (e.g. i==0 for AND and i==1 for NAND)
 * \returns The logical value produced by this gate (not including a possible fault on this gate).
 */
char evalGate(vector<char> in, int c, int i) {

  // Are any of the inputs of this gate the controlling value?
  bool anyC = find(in.begin(), in.end(), c) != in.end();
  
  // Are any of the inputs of this gate unknown?
  bool anyUnknown = (find(in.begin(), in.end(), LOGIC_X) != in.end());

  int anyD    = find(in.begin(), in.end(), LOGIC_D)    != in.end();
  int anyDBar = find(in.begin(), in.end(), LOGIC_DBAR) != in.end();


  // if any input is c or we have both D and D', then return c^i
  if ((anyC) || (anyD && anyDBar))
    return (i) ? LogicNot(c) : c;
  
  // else if any input is unknown, return unknown
  else if (anyUnknown)
    return LOGIC_X;

  // else if any input is D, return D^i
  else if (anyD)
    return (i) ? LOGIC_DBAR : LOGIC_D;

  // else if any input is D', return D'^i
  else if (anyDBar)
    return (i) ? LOGIC_D : LOGIC_DBAR;

  // else return ~(c^i)
  else
    return LogicNot((i) ? LogicNot(c) : c);
}

/** @brief Evaluate an XOR or XNOR gate.
 * \param in The logic value's of this gate's inputs.
 * \param inv The inverting value for this gate (e.g. i==0 for XOR and i==1 for XNOR)
 * \returns The logical value produced by this gate (not including a possible fault on this gate).
 */
char EvalXORGate(vector<char> in, int inv) {

  // if any unknowns, return unknown
  bool anyUnknown = (find(in.begin(), in.end(), LOGIC_X) != in.end());
  if (anyUnknown)
    return LOGIC_X;

  // Otherwise, let's count the numbers of ones and zeros for faulty and fault-free circuits.
  // This is not required for your project, but this would work  with XOR and XNOR with > 2 inputs.
  int onesFaultFree = 0;
  int onesFaulty = 0;

  for (int i=0; i<in.size(); i++) {
    switch(in[i]) {
    case LOGIC_ZERO: {break;}
    case LOGIC_ONE: {onesFaultFree++; onesFaulty++; break;}
    case LOGIC_D: {onesFaultFree++; break;}
    case LOGIC_DBAR: {onesFaulty++; break;}
    default: {cout << "ERROR: Do not know how to process logic value " << in[i] << " in Gate::EvalXORGate()" << endl; return LOGIC_X;}
    }
  }
  
  int XORVal;

  if ((onesFaultFree%2 == 0) && (onesFaulty%2 ==0))
    XORVal = LOGIC_ZERO;
  else if ((onesFaultFree%2 == 1) && (onesFaulty%2 ==1))
    XORVal = LOGIC_ONE;
  else if ((onesFaultFree%2 == 1) && (onesFaulty%2 ==0))
    XORVal = LOGIC_D;
  else
    XORVal = LOGIC_DBAR;

  return (inv) ? LogicNot(XORVal) : XORVal;

}


/** @brief Perform a logical NOT operation on a logical value using the LOGIC_* macros
 */
int LogicNot(int logicVal) {
  if (logicVal == LOGIC_ONE)
    return LOGIC_ZERO;
  if (logicVal == LOGIC_ZERO)
    return LOGIC_ONE;
  if (logicVal == LOGIC_D)
    return LOGIC_DBAR;
  if (logicVal == LOGIC_DBAR)
    return LOGIC_D;
  if (logicVal == LOGIC_X)
    return LOGIC_X;
      
  cout << "ERROR: Do not know how to invert " << logicVal << " in LogicNot(int logicVal)" << endl;
  return LOGIC_UNSET;
}

/** @brief Set the value of Gate* g to value gateValue, accounting for any fault on g.
 */
void setValueCheckFault(Gate* g, char gateValue) {
  if ((g->get_faultType() == FAULT_SA0) && (gateValue == LOGIC_ONE)) 
  	g->setValue(LOGIC_D);
  else if ((g->get_faultType() == FAULT_SA0) && (gateValue == LOGIC_DBAR)) 
  	g->setValue(LOGIC_ZERO);
  else if ((g->get_faultType() == FAULT_SA1) && (gateValue == LOGIC_ZERO)) 
  	g->setValue(LOGIC_DBAR);
  else if ((g->get_faultType() == FAULT_SA1) && (gateValue == LOGIC_D)) 
  	g->setValue(LOGIC_ONE);
  else
  	g->setValue(gateValue);
}

bool podemRecursion(Circuit* myCircuit) {
	vector<Gate*> poGates = myCircuit->getPOGates();
	for (int i=0; i < poGates.size(); i++) {
		char poValue = poGates[i]->getValue();
		if ((poValue == LOGIC_D) || (poValue == LOGIC_DBAR))
		    return true;
	}

	Gate* objGate;
	char objValue;
	bool result = getObjective(objGate, objValue, myCircuit);
	if(!result) return false;

	Gate* piGate;
	char piValue;
	queue<Gate*> gateQueue1;
	queue<Gate*> gateQueue2;
	queue<Gate*> gateQueue3;

	backtrace(piGate, piValue, objGate, objValue, myCircuit);
	//part-1
	//simFullCircuit(myCircuit); // for part-1
	//part-2
	gateQueue1.push(piGate);
	//eventDrivenSim(myCircuit, gateQueue1); // for part-2
	//part-3
	eventDrivenSimPart3(myCircuit, gateQueue1); //for part-3
	result = podemRecursion(myCircuit);
	if(result) return true;

	piGate->setValue(LogicNot(piValue));
	//part-1
	//simFullCircuit(myCircuit); // for part-1
	//part-2
	gateQueue2.push(piGate);
	//eventDrivenSim(myCircuit, gateQueue2); // for part-2
	//part-3
	eventDrivenSimPart3(myCircuit, gateQueue2); // for part-3
	result = podemRecursion(myCircuit);
	if(result) return true;

	piGate->setValue(LOGIC_X);
	//part-1
	//simFullCircuit(myCircuit);
	//part-2
	gateQueue3.push(piGate);
	//eventDrivenSim(myCircuit, gateQueue3); // for part-2
	//part-3
	eventDrivenSimPart3(myCircuit, gateQueue3); // for part-3
	return false;
}

/** @brief PODEM objective function.
 *  \param g Use this pointer to store the objective Gate your function picks.
 *  \param v Use this char to store the objective value your function picks.
 *  \returns True if the function is able to determine an objective, and false if it fails.
 */
bool getObjective(Gate* &g, char &v, Circuit* myCircuit) {

  // First you will need to check if the fault is activated yet.
  // Note that in the setup above we set up a global variable
  // Gate* faultLocation which represents the gate with the stuck-at
  // fault on its output. Use that when you check if the fault is
  // excited.

  // Another note: if the fault is not excited but the fault 
  // location value is not X, then we have failed to activate 
  // the fault. In this case getObjective should fail and Return false.  
  // Otherwise, use the D-frontier to find an objective.

  // Don't forget to call updateDFrontier to make sure your dFrontier (a global variable)
  // is up to date.

  // Remember, for Parts 1/2 if you want to match my reference solution exactly,
  // you should choose the first gate in the D frontier (dFrontier[0]), and pick
  // its first approrpriate input.
  
  // If you can successfully set an objective, return true.

	if(faultLocation->getValue() != LOGIC_D && faultLocation->getValue() != LOGIC_DBAR){
		if(faultLocation->getValue() == LOGIC_ZERO || faultLocation->getValue() == LOGIC_ONE){
			return false;
		}
		else {	
			g = faultLocation;
			v = faultActivationVal;
			return true;
		}
	}
	//updateDFrontier(myCircuit); // for part-1
	if(dFrontier.empty())return false;
	Gate* d = dFrontier[0];
	if(d->get_gateType() == GATE_PI)
		g = d;
	else {
		vector<Gate*> dGateInputs = d->get_gateInputs();
		for(int i=0; i < dGateInputs.size(); i++){
			if(dGateInputs[i]->getValue() == LOGIC_X){
				g = dGateInputs[i];
				break;
			}
		}
	}
	v = getNonControllingValue(d);
	return true;
}


/** @brief A method to compute the set of gates on the D frontier.
 */
void updateDFrontier(Circuit* myCircuit) {

  // Procedure:
  //  - clear the dFrontier vector (stored as the global variable dFrontier -- see the top of the file)
  //  - loop over all gates in the circuit; for each gate, check if it should be on D-frontier; if it is,
  //    add it to the dFrontier vector.

  // One way to improve speed for Part 3 would be to improve D-frontier management. You can add/remove
  // gates from the D frontier during simulation, instead of adding an entire pass over all the gates
  // like this.
	dFrontier.clear();
	for (int i=0; i<myCircuit->getNumberGates(); i++) {
		Gate* g = myCircuit->getGate(i);
		if (g->getValue() == LOGIC_X && (g->get_gateType() != GATE_PI || g->get_gateType() != GATE_FANOUT || g->get_gateType() != GATE_NOT || g->get_gateType() != GATE_BUFF)){
			vector<Gate*> gateInputs = g->get_gateInputs();
			for(int j=0; j < gateInputs.size(); j++){
				if(gateInputs[j]->getValue() == LOGIC_D || gateInputs[j]->getValue() == LOGIC_DBAR){
					dFrontier.push_back(g);
					break;
				}
			}
		}
	}

}

/** @brief PODEM backtrace function
 * \param pi Output: A Gate pointer to the primary input your backtrace function found.
 * \param piVal Output: The value you want to set that primary input to
 * \param objGate Input: The objective Gate (computed by getObjective)
 * \param objVal Input: the objective value (computed by getObjective)
 */
void backtrace(Gate* &pi, char &piVal, Gate* objGate, char objVal, Circuit* myCircuit) {
	Gate* temp = objGate;
	int numInversions = 0;
	while(temp->get_gateType() != GATE_PI){
		switch(temp->get_gateType()){
			case GATE_NOT : {numInversions++; break;}
			case GATE_NAND : {numInversions++; break;}
			case GATE_NOR : {numInversions++; break;}
			case GATE_XOR : {vector<Gate*> xorInputs = temp->get_gateInputs();
					  for(int i=0; i < xorInputs.size(); i++){
						if(xorInputs[i]->getValue() == LOGIC_ONE){
							numInversions++;
							break;
						}
					  }
					  break;
					}
			case GATE_XNOR : {vector<Gate*> xnorInputs = temp->get_gateInputs();
					  for(int i=0; i < xnorInputs.size(); i++){
						if(xnorInputs[i]->getValue() == LOGIC_ZERO){
							numInversions++;
							break;
						}
					  }
					  break;
					}
			default : break;
		}
				
		vector<Gate*> tempGateInputs = temp->get_gateInputs();
		for(int i=0; i < tempGateInputs.size(); i++){
			if(tempGateInputs[i]->getValue() == LOGIC_X){
				temp = tempGateInputs[i];
				break;
			}
		}
	}

	piVal = ((numInversions % 2) == 0) ? objVal : LogicNot(objVal);
	if((temp->get_faultType() == FAULT_SA0) && (piVal == LOGIC_ONE)){
		pi = temp;
		pi->setValue(LOGIC_D);
	} else if((temp->get_faultType() == FAULT_SA1) && (piVal == LOGIC_ZERO)){
		pi = temp;
		pi->setValue(LOGIC_DBAR);
	} else {	
		pi = temp;
		pi->setValue(piVal);
	}
}

//function returns the non-controlling value for the gate.
char getNonControllingValue(Gate *g){
	char gateType = g->get_gateType();
	switch(gateType){
		case GATE_AND: return LOGIC_ONE;
		case GATE_OR: return LOGIC_ZERO;
		case GATE_XOR: return LOGIC_ZERO;
		case GATE_NAND: return LOGIC_ONE;
		case GATE_NOR: return LOGIC_ZERO;
		case GATE_XNOR: return LOGIC_ZERO;
	}
}

//Circuit simulator for part-3
void eventDrivenSimPart3(Circuit* myCircuit, queue<Gate*> q) {
	while(!q.empty()){
		Gate* g = q.front();
		vector<Gate*> gateOutputs = g->get_gateOutputs();
		for(int i=0; i < gateOutputs.size(); i++){
			char prevValue = gateOutputs[i]->getValue();
			char newValue = simGate(gateOutputs[i]);
			setValueCheckFault(gateOutputs[i], newValue);
			if(prevValue != gateOutputs[i]->getValue()){
				q.push(gateOutputs[i]);
			}

			//To check if gate (gateOutpus[i]) should be added to the D-Frontier
			bool isDFrontierGate = false;
			if (gateOutputs[i]->getValue() == LOGIC_X && 
				(gateOutputs[i]->get_gateType() != GATE_PI || gateOutputs[i]->get_gateType() != GATE_FANOUT || 
				gateOutputs[i]->get_gateType() != GATE_NOT || gateOutputs[i]->get_gateType() != GATE_BUFF)){
				
				vector<Gate*> gateInputs = gateOutputs[i]->get_gateInputs();
				for(int j=0; j < gateInputs.size(); j++){
					//Gate is on the D-frontier, check whether it should be added to the D-frontier list
					if(gateInputs[j]->getValue() == LOGIC_D || gateInputs[j]->getValue() == LOGIC_DBAR){
						isDFrontierGate = true;
						bool presentInDFrontier = false;
						//Check whether the gate already exists on the D-frontier
						for(int k=0; k < dFrontier.size(); k++){
							if(gateOutputs[i]->get_outputName() == dFrontier[k]->get_outputName()){
								presentInDFrontier = true;
								break;
							}
						}
						//If the gate is not present in the D-frontier list, add it to the D-frontier list
						if(!presentInDFrontier){
							dFrontier.push_back(gateOutputs[i]);
							break;
						}
						break;
					}
				}
			}
			//Remove from dFrontier vector (if present in it) as it is not a D-Frontier gate
			if(!isDFrontierGate){
				for(int k=0; k < dFrontier.size(); k++){
					if(gateOutputs[i]->get_outputName() == dFrontier[k]->get_outputName()){
						dFrontier.erase(dFrontier.begin()+k);
						break;
					}
				}
			}

			//Single stuck-at fault is detected, add it to the faultsDetected vector
			if(gateOutputs[i]->getValue() == LOGIC_D || gateOutputs[i]->getValue() == LOGIC_DBAR){
				//Fault is detected, check whether we need to add it to the faultsDetected vector
				bool faultAlreadyDetected = false;
				for(int i = 0; i < faultsDetected.size(); i++){
					if(faultsDetected[i].loc/*->get_outputName()*/ == gateOutputs[i]/*->get_outputName()*/){
						faultAlreadyDetected = true;
						//Fault already in faultsDetected vector, don't have to add it to the vector
						if(faultsDetected[i].val == FAULT_SA0 && gateOutputs[i]->getValue() == LOGIC_D){
							break;
						} else if(faultsDetected[i].val ==  FAULT_SA1 && gateOutputs[i]->getValue() == LOGIC_DBAR){
							break;
						}
						//Fault is not already present in faultsDetected vetor, add it to the vector
						else if(faultsDetected[i].val == FAULT_SA0 && gateOutputs[i]->getValue() == LOGIC_DBAR){
							faultsDetected[i].val =  FAULT_SA1;
							break;
						}  else if(faultsDetected[i].val ==  FAULT_SA1 && gateOutputs[i]->getValue() == LOGIC_D){
							faultsDetected[i].val = FAULT_SA0;
							break;
						}
					}
				}
				if(!faultAlreadyDetected){
					if(gateOutputs[i]->getValue() == LOGIC_D){
						faultsDetected.push_back({gateOutputs[i], FAULT_SA0});
					} else if(gateOutputs[i]->getValue() == LOGIC_DBAR){
						faultsDetected.push_back({gateOutputs[i], FAULT_SA1});
					}
				}
			} else {
				//Fault is not detected at this gate, so we need to remove it from the faultsDetected vector if it is present in the vector
				for(int k=0; k < faultsDetected.size(); k++){
					if(faultsDetected[k].loc/*->get_outputName()*/ == gateOutputs[i]/*->get_outputName()*/){
						faultsDetected.erase(faultsDetected.begin()+k);
						break;
					}
				}
				//Remove inputs if they are present in the faultsDetected vector
				vector<Gate*> tempInputs = gateOutputs[i]->get_gateInputs();
				for(int tempIndex = 0; tempIndex < tempInputs.size(); tempIndex++){
					for(int faultIndex = 0; faultIndex < faultsDetected.size(); faultIndex++){
						if(tempInputs[tempIndex] == faultsDetected[faultIndex].loc){
							faultsDetected.erase(faultsDetected.begin() + faultIndex);
							break;
						}
					}
				}
			}

		}
		q.pop();
	}
}