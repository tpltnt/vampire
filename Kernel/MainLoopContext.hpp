/**
 * @file MainLoopContext.hpp
 *
 * @since 19 May 2014
 * @author dmitry
 */

#ifndef __MainLoopContext__
#define __MainLoopContext__

#include <iostream>

#include "Lib/Allocator.hpp"
#include "Lib/EnvironmentFwd.hpp"
#include "Kernel/ConcurrentMainLoopFwd.hpp"
#include "Kernel/ProblemFwd.hpp"
#include "Shell/OptionsFwd.hpp"

namespace Kernel {

class MainLoopContext {
public:
	MainLoopContext(Problem& prb, Shell::Options& opts);
	const unsigned _id;

private:
	Lib::Allocator* _allocator;
	bool _use_global;
public:
	Lib::Allocator* getAllocator(){ 
		if(_use_global) return Lib::Allocator::current;
		return _allocator; 
	}
	void switchAllocatorToGlobal(){ _use_global=true;}
	void switchAllocatorBack(){ _use_global=false;}

	static Lib::Allocator* getCurrentAllocator(){
		if(currentContext){
			return currentContext->getAllocator();
		}
		return Lib::Allocator::current;
	}

       CLASS_NAME(MainLoopContext);
       USE_ALLOCATOR(MainLoopContext);

	virtual ~MainLoopContext();

	// Do one main loop step in this context
	virtual void doStep(unsigned int timeSlice = 100);
	// Do init required by algorithm, and set phase
	virtual void init();
	// Do cleanup required by algorithm, and set phase
	virtual void cleanup();

	// Get the ConcurrentMainLoop
	ConcurrentMainLoop* getMainLoop() const { return _ml; }

	unsigned int updateTimeCounter();
	unsigned int elapsedDeciseconds() const {
		return _elapsed / 100;
	}
	unsigned int elapsed() const {
		return _elapsed;
	}

#if VDEBUG
	bool checkEnvironment(const Lib::Environment* env) const {
		return (_env == env);
	}
#endif //VDEBUG

	static MainLoopContext* currentContext;

	inline
	bool initialised() const { return _initialised; }

	inline
	unsigned int averageTimeSlice() const {
		//const unsigned int timeSlice = _elapsed / _steps;
		return (_steps > 0 &&_elapsed > _steps)? (_elapsed /_steps): 1;
	}

/*	inline
	bool withinTimeBudget() const {
		return _elapsed < _timeBudget;
	}

	inline
	void addTimeBudget(unsigned int budget){
		_timeBudget += budget;
	}*/

protected:
	// Switch into this context
	void switchIn();
	// Switch out from the context
	void switchOut();

	class AutoSwitch{
		public:
	        AutoSwitch(MainLoopContext* c) : _cntxt(c) { _cntxt -> switchIn(); }
	        ~AutoSwitch(){ _cntxt -> switchOut(); }
		private:
	        MainLoopContext* _cntxt;
	};
	friend class AutoSwitch;

	ConcurrentMainLoop* _ml;
	const Shell::Options& _opts;
	Problem* _prb;
private:
        static unsigned id_counter;
	Lib::Environment* _env;
	Lib::Environment* _temp_env; //A variable to preserve the current environment before switching in.
								 //TODO: a manager pattern for main loops needs to be implemented for context switching
	unsigned int _startTime, _elapsed, _timeBudget;

	bool _initialised;

	unsigned int _steps;

};

#if VDEBUG && DESCRIPTOR_ON

#define ALLOC_KNOWN_LOCAL(size,className)                             \
  (MainLoopContext::getCurrentAllocator()->allocateKnown(size,className))
#define DEALLOC_KNOWN_LOCAL(obj,size,className)                       \
  (MainLoopContext::getCurrentAllocator()->deallocateKnown(obj,size,className))

#else

#define ALLOC_KNOWN_LOCAL(size,className)                             \
  (MainLoopContext::getCurrentAllocator()->allocateKnown(size))
#define DEALLOC_KNOWN_LOCAL(obj,size,className)                       \
  (MainLoopContext::getCurrentAllocator()->deallocateKnown(obj,size))

#endif

} /* namespace Kernel */

#endif /* __MainLoopContext__ */
