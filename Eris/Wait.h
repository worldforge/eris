#ifndef ERIS_WAIT_H
#define ERIS_WAIT_H

#include <Atlas/Message/Element.h>

#include <sigc++/object.h>
#include <sigc++/signal.h>

namespace Atlas { namespace Objects {
	class Root;
}}

namespace Eris
{

// Forward declarations
class Dispatcher;
class Connection;
	
class WaitForBase : virtual public SigC::Object
{
public:
	WaitForBase(const Atlas::Message::Element &m, Connection *conn);
	virtual ~WaitForBase() {;}
		
	bool isPending() const
	{ return _pending; }
	
	void fire();
	
	/** Predicate matching STL UnaryFunction, indicating whether the wait has been fired. This
	is used in STL remove_if and so on. */
	static bool hasFired(WaitForBase *w)
	{ return w->_pending; }
	
protected:
	bool _pending;
	Atlas::Message::Element _msg;
	Connection* _conn;
};

class WaitForDispatch : public WaitForBase
{
public:
	WaitForDispatch(const Atlas::Message::Element &msg,  
		const std::string &ppath,
		Dispatcher *dsp,
		Connection *conn);

	WaitForDispatch(const Atlas::Objects::Root &msg, 
		const std::string &ppath,
		Dispatcher *dsp,
		Connection *conn);

	virtual ~WaitForDispatch();

protected:
	std::string _parentPath;
	Dispatcher* _dsp;
};

class WaitForSignal : public WaitForBase
{
public:	
	WaitForSignal(SigC::Signal0<void> &sig, const Atlas::Message::Element &msg, Connection *conn);
	virtual ~WaitForSignal();
protected:
	
};

}

#endif