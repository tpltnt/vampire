/**
 * @file VirtualIterator.hpp
 * Defines VirtualIterator which allows iterators over various
 * structures to be stored in the same object.
 */

#ifndef __VirtualIterator__
#define __VirtualIterator__

#include "Forwards.hpp"

#include "Debug/Assertion.hpp"
#include "Debug/Tracer.hpp"

#include "Allocator.hpp"
#include "Exception.hpp"
#include "Reflection.hpp"

namespace Lib {

///@addtogroup Iterators
///@{

template<typename T>
  class VirtualIterator;

/**
 * Base class of objects that provide implementation of
 * VirtualIterator objects.
 */
template<typename T>
class IteratorCore {
private:
  //private and undefined operator= and copy constructor to avoid implicitly generated ones
  IteratorCore(const IteratorCore&);
  IteratorCore& operator=(const IteratorCore&);
public:
  DECL_ELEMENT_TYPE(T);
  IteratorCore() : _refCnt(0) {}
  virtual ~IteratorCore() { ASS(_refCnt==0); }
  virtual bool hasNext() = 0;
  virtual T next() = 0;
  virtual bool knowsSize() const { return false; }
  virtual size_t size() const { INVALID_OPERATION("This iterator cannot retrieve its size."); }

  CLASS_NAME("IteratorCore");
//  CLASS_NAME(typeid(IteratorCore).name());
  USE_ALLOCATOR_UNK;
private:
  mutable int _refCnt;

  friend class VirtualIterator<T>;
};

/**
 * Implementation object for VirtualIterator, that represents
 * an empty iterator.
 */
template<typename T>
class EmptyIterator
: public IteratorCore<T>
{
public:
  EmptyIterator() {}
  bool hasNext() { return false; };
  T next() { INVALID_OPERATION("next() called on EmptyIterator object"); };
  bool knowsSize() const { return true; }
  size_t size() const { return 0; }
};

/**
 * Template class of virtual iterators, i.e. iterators that
 * can polymorphically use different implementations.
 */
template<typename T>
class VirtualIterator {
public:
  DECL_ELEMENT_TYPE(T);
  static VirtualIterator getEmpty()
  {
    static VirtualIterator inst(new EmptyIterator<T>());
    return inst;
  }
  inline
  VirtualIterator() : _core(0) {}
  inline
  explicit VirtualIterator(IteratorCore<T>* core) : _core(core) { _core->_refCnt++;}
  inline
  VirtualIterator(const VirtualIterator& obj) : _core(obj._core)
  {
    if(_core) {
      _core->_refCnt++;
    }
  }
  inline
  ~VirtualIterator()
  {
    if(_core) {
	_core->_refCnt--;
	if(!_core->_refCnt) {
	  delete _core;
	}
    }
  }
  VirtualIterator& operator=(const VirtualIterator& obj)
  {
    CALL("VirtualIterator::operator=");

    IteratorCore<T>* oldCore=_core;
    _core=obj._core;
    if(_core) {
	_core->_refCnt++;
    }
    if(oldCore) {
	oldCore->_refCnt--;
	if(!oldCore->_refCnt) {
	  delete oldCore;
	}
    }
    return *this;
  }

  /**
   * Remove reference to the iterator core.
   * Return true iff the the iterator core does not exist
   * any more after return from this function.
   *
   * The returned value can be useful for asserting
   * that the iterator core (and all resources it
   * used) was indeed released.
   */
  inline
  bool drop()
  {
    if(_core) {
      _core->_refCnt--;
      if(_core->_refCnt) {
	_core=0;
	return false;
      }
      else {
	delete _core;
	_core=0;
      }
    }
    _core=0;
    return true;
  }

  /** True if there exists next element */
  inline
  bool hasNext() { return _core->hasNext(); }
  /**
   * Return the next value
   * @warning hasNext() must have been called before
   */
  inline
  T next() { return _core->next(); }

  bool knowsSize() const { return _core->knowsSize(); }
  size_t size() const { ASS(knowsSize()); return _core->size(); }
private:
  IteratorCore<T>* _core;
};

template<typename T>
inline
VirtualIterator<T> vi(IteratorCore<T>* core)
{
  return VirtualIterator<T>(core);
}

/**
 * Implementation object for VirtualIterator, that can proxy any
 * non-virtual iterator, that supports hasNext() and next() methods.
 */
template<typename T, class Inner>
class ProxyIterator
: public IteratorCore<T>
{
public:
  explicit ProxyIterator(Inner inn) :_inn(inn) {}
  bool hasNext() { return _inn.hasNext(); };
  T next() { return _inn.next(); };
private:
  Inner _inn;
};

template<typename T, class Inner>
inline
VirtualIterator<T> getProxyIterator(Inner it)
{
  return VirtualIterator<T>(new ProxyIterator<T,Inner>(it));
}

template<class Inner>
inline
VirtualIterator<ELEMENT_TYPE(Inner)> pvi(Inner it)
{
  return VirtualIterator<ELEMENT_TYPE(Inner)>(new ProxyIterator<ELEMENT_TYPE(Inner),Inner>(it));
}




///@}

}

#endif
