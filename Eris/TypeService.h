#ifndef ERIS_TYPE_SERVICE_H
#define ERIS_TYPE_SERVICE_H

#include <sigc++/object.h>
#include <sigc++/signal.h>

#include <Eris/Router.h>
#include <Eris/Types.h>
#include <Atlas/Objects/Decoder.h>

#include <map>
#include <set>

namespace Eris {

class Connection;
class TypeInfo;

typedef TypeInfo* TypeInfoPtr;
typedef std::set<TypeInfoPtr> TypeInfoSet;

class TypeService : virtual public SigC::Object, public Router
{
public:
    TypeService(Connection *con);

    void init();

    /// load the core Atlas::Objects specification from the named file
    void readAtlasSpec(const std::string &specfile);

    /** find the TypeInfo for the named type; this may involve a search, or a map lookup.
     The returned TypeInfo	 node may not be bound, and the caller should verify this
     before using the type. */
    TypeInfoPtr getTypeByName(const std::string &tynm);

    /** retrive the TypeInfo for an object; this should be faster (hopefully constant time) since it
    can take advantage of integer typeids */
    TypeInfoPtr getTypeForAtlas(const Atlas::Objects::Root &obj);

    /** Lookup the requested type, by name, and return NULL if it's unknown. */
    TypeInfoPtr findTypeByName(const std::string &tynm);

    /** emitted when a new type is available and bound to it's parents */
    SigC::Signal1<void, TypeInfoPtr> BoundType;

    /** emitted when a type is confirmed as being undefined */
    SigC::Signal1<void, TypeInfoPtr> BadType;

    void listUnbound();

    RouterResult handleOperation(const Atlas::Objects::Operation::RootOperation&);

    bool verifyObjectTypes(const Atlas::Objects::Root& obj);

 private:
    /** helper class to parse definitions out of the spec file */
    class SpecDecoder : public Atlas::Objects::ObjectsDecoder
    {
    public:
        SpecDecoder(TypeService* ts);
    protected:
        virtual void objectArrived(const Atlas::Objects::Root& obj);

    private:
        TypeService* m_typeService;
    };

    /** request the information about a type from the server.
    @param id The ID of the type to lookup
    */
    void sendRequest(const std::string& id);

    /// build a TypeInfo object if necessary, and add it to the database
    void registerLocalType(const Atlas::Objects::Root &def);

    void recvTypeInfo(const Atlas::Objects::Root &atype);
    void recvError(const Atlas::Objects::Operation::Get& get);

    TypeInfoPtr defineBuiltin(const std::string& name, TypeInfoPtr parent);

    void innerVerifyType(const Atlas::Objects::Root& obj, TypeInfoSet& unbound);
    void verifyOpArguments(const Atlas::Objects::Root& obj, TypeInfoSet& unbound);
    
    typedef std::map<std::string, TypeInfoPtr> TypeInfoMap;
    /** The easy bit : a simple map from 'string-id' (e.g 'look' or 'farmer')
    to the corresponding TypeInfo instance. This could be a hash_map in the
    future, if efficeny consdierations indicate it would be worthwhile */
    TypeInfoMap m_types;

    Connection* m_con;

    typedef std::set<int> RefNoSet;
    /** until we specialize INFO to TYPEINFO, it's hard to be sure which
    INFO or ERROR ops relate to type requests. We use this set to record
    the serialNo of each request we issue, to help decoding ops we get
    back from the server. */
    RefNoSet m_typeRequests;

    bool m_inited;

    StringSet m_badTypes;
    TypeInfoPtr m_anonymousType;
};

} // of namespace Eris

#endif // of ERIS_TYPE_SERVICE_H