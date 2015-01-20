/**
 * @file MultiCM.cpp
 * Implements class MultiCM.
 */

#include "Forwards.hpp"

#include "Lib/Environment.hpp"
#include "Shell/UIHelper.hpp"
#include "Shell/Preprocess.hpp"
#include "Shell/Options.hpp"

#include "Kernel/MainLoopScheduler.hpp"
#include "Kernel/MainLoop.hpp"

#include "MultiCM.hpp"

// Defined separately from CASCMode as we might want different values in each place
#define SLOWNESS 1.1

namespace CASC
{

MultiCM::MultiCM()
{

  _prb = UIHelper::getInputProblem(*env -> options);
  // Problem has not been preprocessed, but that should be okay
  _property = _prb->getProperty();

}


bool MultiCM::runSchedule(Schedule& schedule, unsigned ds, StrategySet& remember, bool fallback)
{
  CALL("MultiCM::runSchedule");

  if(fallback){
    //Currently we cannot handle a second schedule
    return 1;
  }
  //TODO should we use ds?
  // There is an invariant that all strategies must use the same
  // preprocessing, therefore the transformation must change
  // the schedule in some way to accomodate this
  transformToOptionsList(schedule);

  // As all strategies have the same preprocessing options we can
  // do the preprocessing once
  {
    TimeCounter tc2(TC_PREPROCESSING);

    Preprocess prepro(*env -> options); // should have been set by transformToOptionsList

    //phases for preprocessing are being set inside the proprocess method
    prepro.preprocess(*_prb);
  }

  Kernel::MainLoopScheduler scheduler(*_prb, *env -> optionsList);
  scheduler.run();

  int resultValue=1;
  //return value to zero if we were successful
#if SATISFIABLE_IS_SUCCESS
  if(env -> statistics->terminationReason==Statistics::REFUTATION ||
     env -> statistics->terminationReason==Statistics::SATISFIABLE) {
#else
  if(env -> statistics->terminationReason==Statistics::REFUTATION) {
#endif
    resultValue=0;
    }

  env -> beginOutput();
  UIHelper::outputResult(env -> out());
  env -> endOutput();

  return resultValue;
}

void MultiCM::transformToOptionsList(Schedule& schedule)
{
  CALL("MultiCM::transformToOptionsList");

  //For each strategy we create an option object in the optionsList
  //Need to ensure additional global options are dealt with appropriately
  //Currently all global options are copied but overriden with those in the strategy 
  
  // save the original options that are about to be deleted
  const Options* orig_opt = env->options;
  ASS(orig_opt);

  //Replace options list
  unsigned strategies = schedule.size(); 

  //We will filter out tabulation, so count the number
  Schedule::Iterator tit(schedule);
  while(tit.hasNext()){ if(tit.next().find("tab")==0) strategies--;} 

  ASS(strategies>0);
  cout << "creating with " << strategies << " strategies" << endl;
  env->optionsList = SmartPtr<OptionsList>(new OptionsList(strategies));
  env -> options = &((*env->optionsList)[0]);


  unsigned index=0;
  Schedule::BottomFirstIterator sit(schedule);
  while(sit.hasNext()){
    vstring sliceCode = sit.next();

    if(sliceCode.find("tab")==0) continue;

    // get the option
    Options& opt = (*env->optionsList)[index++];

    // copy orig
    opt = *orig_opt;    

    //decode slice
    cout << "decoding " << sliceCode << endl;
    opt.set("ignore_missing","on");
    opt.set("decode",sliceCode);

    // Remove preprocessing from all but the first sliceCode
    // TODO - would be better to select a set of compatiable options from all sliceCodes
    if(index>1){
      //int max=9;
      //vstring sn[max] = {"fde","gsp","updr","sd","sgt","ss","st","nm","ins"};
      //for(int i=0;i<max;i++){
        //Options::AbstractOptionValue* ov = orig->getOptionValueByName(sn[i]);
        //opt.setShort(sn[i],ov->getStringOfActual());
      //}
      opt.setFunctionDefinitionElimination(orig_opt->functionDefinitionElimination());
      opt.setGeneralSplitting(orig_opt->generalSplitting());
      opt.set("unused_predicate_definition_removal",orig_opt->unusedPredicateDefinitionRemoval()?"on":"off");
      opt.set("sine_depth",Lib::Int::toString(orig_opt->sineDepth()));
      opt.set("sine_generality_threshold",Lib::Int::toString(orig_opt->sineGeneralityThreshold()));
      opt.setSineSelection(orig_opt->sineSelection());
      opt.set("sine_tolerance",Lib::Int::toString(orig_opt->sineTolerance()));
      opt.set("naming",Lib::Int::toString(orig_opt->naming()));
      opt.set("inequality_splitting",Lib::Int::toString(orig_opt->inequalitySplitting()));
      //TODO others?
    }

    ASS(opt.saturationAlgorithm()!=Options::TABULATION);

    //Slowdown simulated time limit for use in LRS
    int stl = opt.simulatedTimeLimit();
    if(stl){
      opt.setSimulatedTimeLimit(int(stl * SLOWNESS));
    }
  }

}


}

