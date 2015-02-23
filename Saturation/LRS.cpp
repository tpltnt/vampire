/**
 * @file LRS.cpp
 * Implements class LRS.
 */

#include "Lib/Environment.hpp"
#include "Lib/Timer.hpp"
#include "Lib/TimeCounter.hpp"
#include "Lib/VirtualIterator.hpp"
#include "Kernel/Clause.hpp"
#include "Kernel/LiteralSelector.hpp"
#include "Kernel/MainLoopContext.hpp"
#include "Kernel/MainLoopScheduler.hpp"
#include "Shell/Statistics.hpp"
#include "Shell/Options.hpp"

#include "LRS.hpp"

namespace Saturation
{

using namespace Lib;
using namespace Kernel;
using namespace Shell;

bool LRS::isComplete()
{
  CALL("LRS::isComplete");

  return !_limitsEverActive && SaturationAlgorithm::isComplete();
}


void LRS::onUnprocessedSelected(Clause* c)
{
  CALL("LRS::onUnprocessedSelected");

  SaturationAlgorithm::onUnprocessedSelected(c);

  if(shouldUpdateLimits()) {
    TimeCounter tc(TC_LRS_LIMIT_MAINTENANCE);

    long long estimatedReachable=estimatedReachableCount();
    if(estimatedReachable>=0) {
      _passive->updateLimits(estimatedReachable);
      if(!_limitsEverActive) {
        Limits* lims=getLimits();
        _limitsEverActive=lims->weightLimited() || lims->ageLimited();
      }
    }
  }
}

/**
 * Return true if it is time to update age and weight
 * limits of the LRS strategy
 *
 * The time of the limit update is determined by a counter
 * of calls of this method.
 */
bool LRS::shouldUpdateLimits()
{
  CALL("LRS::shouldUpdateLimits");

  static unsigned cnt=0;
  cnt++;

  //when there are limits, we check more frequently so we don't skip too much inferences
  if(cnt==500 || ((getLimits()->weightLimited() || getLimits()->ageLimited()) && cnt>50 ) ) {
    cnt=0;
    return true;
  }
  return false;
}

/**
 * Resturn an estimate of the number of clauses that the saturation
 * algorithm will be able to activate in the remaining time
 */
long long LRS::estimatedReachableCount()
{
  CALL("LRS::estimatedReachableCount");

  //int currTime=env -> timer->elapsedMilliseconds();
  long long globalTimeSpent= env->timer->elapsedMilliseconds();
  long long localTimeSpent= Kernel::MainLoopContext::currentContext-> updateTimeCounter(); //currTime-_startTime;
  //the result is in miliseconds, as _opt.lrsFirstTimeCheck() is in percents.

  unsigned int localFirstCheck=_opt.lrsFirstTimeCheck()*_opt.localTimeLimitInDeciseconds();
  unsigned int globalFirstCheck=_opt.lrsFirstTimeCheck()*_opt.timeLimitInDeciseconds();
//  int timeSpent=currTime;

  if(localTimeSpent < localFirstCheck && globalTimeSpent < globalFirstCheck) {
    return -1;
  }

  long long globalTimeLeft;
  long long localTimeLeft;
  if(_opt.simulatedTimeLimit()) {
    localTimeLeft = _opt.simulatedTimeLimit()*100 - localTimeSpent;
    globalTimeLeft = localTimeLeft;
  } else {
    globalTimeLeft = (_opt.timeLimitInDeciseconds()*100 - globalTimeSpent ) / Kernel::MainLoopScheduler::scheduler -> numberOfAliveContexts();//Rough estimate based on fair scheduling
    localTimeLeft = _opt.localTimeLimitInDeciseconds()*100 - localTimeSpent;
  }

  if(localTimeLeft <= 0){
	  if(globalTimeLeft <=0){
		  //we end-up here even if there is no time timit (i.e. time limit is set to 0)
		  return -1;
	  }else{
		  localTimeLeft = globalTimeLeft;
	  }
  }else{
	 if(globalTimeLeft > 0 && globalTimeLeft < localTimeLeft){
	  	  localTimeLeft = globalTimeLeft;
	 }
  }

  long long processed=env -> statistics-> activeClauses;
  if(processed <= 10) {
    return -1;
  }
  return (processed * localTimeLeft) / localTimeSpent;
}

}
