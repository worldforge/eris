#ifndef ERIS_TYPE_DISPATCH_H
#define ERIS_TYPE_DISPATCH_H

#include <Eris/Dispatcher.h>

namespace Eris
{

class TypeDispatcher : public StdBranchDispatcher
{
public:
	TypeDispatcher(const std::string &nm, const std::string &ty) :
		StdBranchDispatcher(nm), _type(ty) 
	{;}
	
	virtual ~TypeDispatcher() {;}
		
	virtual bool dispatch(DispatchContextDeque &dq);
protected:
	const std::string _type;
};

class ObjectDispatcher : public StdBranchDispatcher
{
public:
	ObjectDispatcher(const std::string &nm) :
		StdBranchDispatcher(nm)
	{;}
	
	virtual ~ObjectDispatcher() {;}
		
	virtual bool dispatch(DispatchContextDeque &dq);
};

}

#endif
