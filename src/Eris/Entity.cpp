#include <utility>

#ifdef HAVE_CONFIG_H
    #include "config.h"
#endif

#include "Entity.h"
#include "Connection.h"
#include "TypeInfo.h"
#include "LogStream.h"
#include "Exceptions.h"
#include "Avatar.h"
#include "Task.h"

#include <wfmath/atlasconv.h>
#include <Atlas/Objects/Entity.h>
#include <Atlas/Objects/Operation.h>
#include <Atlas/Objects/BaseObject.h>

#include <algorithm>
#include <set> 
#include <cassert>

using namespace Atlas::Objects::Operation;
using Atlas::Objects::Root;
using Atlas::Objects::Entity::RootEntity;
using Atlas::Message::Element;
using Atlas::Message::ListType;
using Atlas::Message::MapType;
using Atlas::Objects::smart_static_cast;
using Atlas::Objects::smart_dynamic_cast;

using WFMath::TimeStamp;
using WFMath::TimeDiff;

namespace Eris {

Entity::Entity(std::string id, TypeInfo* ty) :
		m_type(ty),
		m_location(nullptr),
		m_id(std::move(id)),
		m_stamp(-1.0f),
		m_visible(false),
		m_waitingForParentBind(false),
		m_angularMag(0),
		m_updateLevel(0),
		m_hasBBox(false),
		m_moving(false),
		m_recentlyCreated(false)
{
    assert(!m_id.empty());
    m_orientation.identity();
    
    
    if (m_type) {
        m_type->PropertyChanges.connect(sigc::mem_fun(*this, &Entity::typeInfo_PropertyChanges));
    }
}

Entity::~Entity()
{
	shutdown();
}

void Entity::shutdown() {
	setLocation(nullptr);

	for (auto& child: m_contents) {
		//Release all children.
		child->setLocation(nullptr, false);
	}
	m_contents.clear();

	//Delete any lingering tasks.
	for (auto& entry : m_tasks) {
		TaskRemoved(entry.first, entry.second.get());
	}
}

void Entity::init(const RootEntity& ge, bool fromCreateOp)
{
    // setup initial state
	firstSight(ge);
    
    if (fromCreateOp)
    {
        m_recentlyCreated = true;
    }
}


Entity* Entity::getTopEntity()
{
	if (m_waitingForParentBind) {
		return nullptr;
	}
	if (!m_location) {
		return this;
	}
	return m_location->getTopEntity();
}

bool Entity::isAncestorTo(Eris::Entity& entity) const
{
    if (!entity.getLocation()) {
        return false;
    }
    if (static_cast<const Eris::Entity*>(this) == entity.getLocation()) {
        return true;
    }
    return isAncestorTo(*entity.getLocation());

}

const Element& Entity::valueOfProperty(const std::string& name) const
{
    ///first check with the instance properties
	auto A = m_properties.find(name);
    if (A == m_properties.end())
    {
        if (m_type) {
            ///it wasn't locally defined, now check with typeinfo
            const Element* element = m_type->getProperty(name);
            if (element) {
                return *element;
            }
        }
        error() << "did valueOfProperty(" << name << ") on entity " << m_id << " which has no such name";
        throw InvalidOperation("no such property " + name);
    } else {
        return A->second;
    }
}

bool Entity::hasProperty(const std::string& p) const
{
    ///first check with the instance properties
    if (m_properties.find(p) != m_properties.end()) {
        return true;
    } else if (m_type) {
        ///it wasn't locally defined, now check with typeinfo
        if (m_type->getProperty(p) != nullptr) {
            return true;
        }
    }
    return false;
}

const Element* Entity::ptrOfProperty(const std::string& name) const
{
    ///first check with the instance properties
	auto A = m_properties.find(name);
    if (A == m_properties.end())
    {
        if (m_type) {
            ///it wasn't locally defined, now check with typeinfo
            const Element* element = m_type->getProperty(name);
            if (element) {
                return element;
            }
        }
        return nullptr;
    } else {
        return &A->second;
    }
}


Entity::PropertyMap Entity::getProperties() const
{
    ///Merge both the local properties and the type default properties.
    PropertyMap properties;
    properties.insert(m_properties.begin(), m_properties.end());
    if (m_type) {
		fillPropertiesFromType(properties, *m_type);
    }
    return properties;
}

const Entity::PropertyMap& Entity::getInstanceProperties() const
{
    return m_properties;
}

void Entity::fillPropertiesFromType(Entity::PropertyMap& properties, const TypeInfo& typeInfo) const
{
    properties.insert(typeInfo.getProperties().begin(), typeInfo.getProperties().end());
    ///Make sure to fill from the closest properties first, as insert won't replace an existing value

	if (typeInfo.getParent()) {
		fillPropertiesFromType(properties, *typeInfo.getParent());
	}

}

sigc::connection Entity::observe(const std::string& propertyName, const PropertyChangedSlot& slot, bool evaluateNow)
{
    // sometimes, I realize how great SigC++ is
    auto connection = m_observers[propertyName].connect(slot);
    if (evaluateNow) {
        auto prop = ptrOfProperty(propertyName);
        if (prop) {
            slot(*prop);
        }
    }
    return connection;
}

const WFMath::Point<3>& Entity::getPredictedPos() const
{
    return (m_moving ? m_predicted.position : m_position);
}

const WFMath::Vector<3>& Entity::getPredictedVelocity() const
{
    return (m_moving ? m_predicted.velocity : m_velocity);
}

const WFMath::Quaternion& Entity::getPredictedOrientation() const
{
    return (m_moving ? m_predicted.orientation : m_orientation);
}

bool Entity::isMoving() const
{
    return m_moving;
}

void Entity::updatePredictedState(const WFMath::TimeStamp& t, double simulationSpeed)
{
    assert(isMoving());

    if (m_acc.isValid() && m_acc != WFMath::Vector<3>::ZERO()) {
		double posDeltaTime = static_cast<double>((t - m_lastPosTime).milliseconds()) / 1000.0;
        m_predicted.velocity = m_velocity + (m_acc * posDeltaTime * simulationSpeed);
		m_predicted.position = m_position + (m_velocity * posDeltaTime * simulationSpeed) + (m_acc * 0.5 * posDeltaTime * posDeltaTime * simulationSpeed);
    } else {
        m_predicted.velocity = m_velocity;
		if (m_predicted.velocity != WFMath::Vector<3>::ZERO()) {
			double posDeltaTime = static_cast<double>((t - m_lastPosTime).milliseconds()) / 1000.0;
			m_predicted.position = m_position + (m_velocity * posDeltaTime * simulationSpeed);
		} else {
			m_predicted.position = m_position;
		}
    }
    if (m_angularVelocity.isValid() && m_angularMag != .0) {
		double orientationDeltaTime = static_cast<double>((t - m_lastOrientationTime).milliseconds()) / 1000.0;
        m_predicted.orientation = m_orientation * WFMath::Quaternion(m_angularVelocity, m_angularMag * orientationDeltaTime * simulationSpeed);
    } else {
        m_predicted.orientation = m_orientation;
    }
}

void Entity::firstSight(const RootEntity &gent)
{    
    if (!gent->isDefaultLoc()) {
    	setLocationFromAtlas(gent->getLoc());
    } else {
    	setLocation(nullptr);
    }
    
    setContentsFromAtlas(gent->getContains());
    //Since this is the first sight of this entity we should include all type props too.
    setFromRoot(gent, true);
}

void Entity::setFromRoot(const Root& obj, bool includeTypeInfoProperties)
{	
    beginUpdate();
    
    Atlas::Message::MapType properties;
    obj->addToMessage(properties);

    properties.erase("id"); //Id can't be changed once it's initially set, which it's at Entity creation time.
    properties.erase("contains"); //Contains are handled by the setContentsFromAtlas method which should be called separately.

    for (auto& entry : properties) {
        // see if the value in the sight matches the existing value
        auto I = m_properties.find(entry.first);
        if ((I != m_properties.end()) && (I->second == entry.second)) {
			continue;
		}
        try {
            setProperty(entry.first, entry.second);
        } catch (const std::exception& ex) {
            warning() << "Error when setting property '" << entry.first << "'. Message: " << ex.what();
        }
    }

    //Add any values found in the type, if they aren't defined in the entity already.
    if (includeTypeInfoProperties && m_type) {
        Atlas::Message::MapType typeProperties;
		fillPropertiesFromType(typeProperties, *m_type);
        for (auto& entry : typeProperties) {
			propertyChangedFromTypeInfo(entry.first, entry.second);
        }
    }

	endUpdate();

}

void Entity::onTalk(const Atlas::Objects::Operation::RootOperation& talk)
{
    const std::vector<Root>& talkArgs = talk->getArgs();
    if (talkArgs.empty())
    {
        warning() << "entity " << getId() << " got sound(talk) with no args";
        return;
    }

    for (const auto& arg: talkArgs) {
		Say.emit(arg);
    }
    //Noise.emit(talk);
}

void Entity::onLocationChanged(Entity* oldLoc)
{
    LocationChanged.emit(oldLoc);
}

void Entity::onMoved(const WFMath::TimeStamp& timeStamp)
{
    if (m_moving) {
        //We should update the predicted pos and velocity.
        updatePredictedState(timeStamp, 1.0);
    }
    Moved.emit();
}

void Entity::onAction(const Atlas::Objects::Operation::RootOperation& arg, const TypeInfo& typeInfo)
{
	Acted.emit(arg, typeInfo);
}

void Entity::onHit(const Atlas::Objects::Operation::Hit& arg, const TypeInfo& typeInfo)
{
	Hit.emit(arg, typeInfo);
}

void Entity::onSoundAction(const Atlas::Objects::Operation::RootOperation& op, const TypeInfo& typeInfo)
{
    Noise.emit(op, typeInfo);
}

void Entity::onImaginary(const Atlas::Objects::Root& arg)
{
    Atlas::Message::Element attr;
    if (arg->copyAttr("description", attr) == 0 && attr.isString()) {
        Emote.emit(attr.asString());
    }
}

void Entity::setMoving(bool inMotion)
{
    assert(m_moving != inMotion);
    
	m_moving = inMotion;
	Moving.emit(inMotion);

}

void Entity::onChildAdded(Entity* child)
{
    ChildAdded.emit(child);
}

void Entity::onChildRemoved(Entity* child)
{
    ChildRemoved(child);
}

void Entity::onTaskAdded(const std::string& id, Task* task)
{
	TaskAdded(id, task);
}


void Entity::setProperty(const std::string &p, const Element &v)
{
    beginUpdate();

	m_properties[p] = v;

	nativePropertyChanged(p, v);
	onPropertyChanged(p, v);

    // fire observers
    
    auto obs = m_observers.find(p);
    if (obs != m_observers.end()) {
        obs->second.emit(v);
    }

    addToUpdate(p);
    endUpdate();
}

bool Entity::nativePropertyChanged(const std::string& p, const Element& v)
{
    // in the future, hash these names to a compile-time integer index, and
    // make this a switch statement. The same index could also be used
    // in endUpdate
    
    if (p == "name") {
        m_name = v.asString();
        return true;
    } else if (p == "stamp") {
        m_stamp = v.asFloat();
        return true;
    } else if (p == "pos") {
        m_position.fromAtlas(v);
        return true;
    } else if (p == "velocity") {
        m_velocity.fromAtlas(v);
        return true;
    } else if (p == "angular") {
        m_angularVelocity.fromAtlas(v);
        m_angularMag = m_angularVelocity.mag();
        return true;
    } else if (p == "accel") {
        m_acc.fromAtlas(v);
        return true;
    } else if (p == "orientation") {
        m_orientation.fromAtlas(v);
		return true;
    } else if (p == "bbox") {
        m_bboxUnscaled.fromAtlas(v);
        m_bbox = m_bboxUnscaled;
        if (m_scale.isValid() && m_bbox.isValid()) {
            m_bbox.lowCorner().x() *= m_scale.x();
            m_bbox.lowCorner().y() *= m_scale.y();
            m_bbox.lowCorner().z() *= m_scale.z();
            m_bbox.highCorner().x() *= m_scale.x();
            m_bbox.highCorner().y() *= m_scale.y();
            m_bbox.highCorner().z() *= m_scale.z();
        }
        m_hasBBox = m_bbox.isValid();
        return true;
    } else if (p == "loc") {
        setLocationFromAtlas(v.asString());
        return true;
    } else if (p == "contains") {
        throw InvalidOperation("tried to set contains via setProperty");
    } else if (p == "tasks") {
        updateTasks(v);
        return true;
    } else if (p == "scale") {
        if (v.isList()) {
            if (v.List().size() == 1) {
                if (v.List().front().isNum()) {
                    auto num = static_cast<WFMath::CoordType>(v.List().front().asNum());
                    m_scale = WFMath::Vector<3>(num, num, num);
                }
            } else {
                m_scale.fromAtlas(v.List());
            }
        } else {
            m_scale = WFMath::Vector<3>();
        }
        m_bbox = m_bboxUnscaled;
        if (m_scale.isValid() && m_bbox.isValid()) {
            m_bbox.lowCorner().x() *= m_scale.x();
            m_bbox.lowCorner().y() *= m_scale.y();
            m_bbox.lowCorner().z() *= m_scale.z();
            m_bbox.highCorner().x() *= m_scale.x();
            m_bbox.highCorner().y() *= m_scale.y();
            m_bbox.highCorner().z() *= m_scale.z();
        }
        return true;
    }

    return false; // not a native property
}

void Entity::onPropertyChanged(const std::string& propertyName, const Element& v)
{
    // no-op by default
}


void Entity::typeInfo_PropertyChanges(const std::string& propertyName, const Atlas::Message::Element& element)
{
	propertyChangedFromTypeInfo(propertyName, element);
}

void Entity::propertyChangedFromTypeInfo(const std::string& propertyName, const Atlas::Message::Element& element)
{
    ///Only fire the events if there's no property already defined for this entity
    if (m_properties.find(propertyName) == m_properties.end()) {
        beginUpdate();
		nativePropertyChanged(propertyName, element);
		onPropertyChanged(propertyName, element);
    
        // fire observers
        
        ObserverMap::const_iterator obs = m_observers.find(propertyName);
        if (obs != m_observers.end()) {
            obs->second.emit(element);
        }
    
        addToUpdate(propertyName);
        endUpdate();
    }
}


void Entity::beginUpdate()
{
    ++m_updateLevel;
}

void Entity::addToUpdate(const std::string& propertyName)
{
    assert(m_updateLevel > 0);
    m_modifiedProperties.insert(propertyName);
}

void Entity::endUpdate()
{
    if (m_updateLevel < 1)
    {
        error() << "mismatched begin/end update pair on entity";
        return;
    }
        
    if (--m_updateLevel == 0) // unlocking updates
    {
        Changed.emit(m_modifiedProperties);
        
        if (m_modifiedProperties.find("pos") != m_modifiedProperties.end() ||
			m_modifiedProperties.find("velocity") != m_modifiedProperties.end() ||
			m_modifiedProperties.find("orientation") != m_modifiedProperties.end() ||
			m_modifiedProperties.find("angular") != m_modifiedProperties.end())
        {
        	auto now = TimeStamp::now();
			if (m_modifiedProperties.find("pos") != m_modifiedProperties.end()) {
				m_lastPosTime = now;
			}
			if (m_modifiedProperties.find("orientation") != m_modifiedProperties.end()) {
				m_lastOrientationTime = now;
			}

            const WFMath::Vector<3> & velocity = getVelocity();
            bool nowMoving = (velocity.isValid() && (velocity.sqrMag() > 1e-3)) || (m_angularVelocity.isValid() && m_angularVelocity != WFMath::Vector<3>::ZERO());
            if (nowMoving != m_moving) {
            	setMoving(nowMoving);
            }
            
            onMoved(now);
        }
        
        m_modifiedProperties.clear();
    }
}


void Entity::updateTasks(const Element& e)
{
    if (e.isNone()) {
        for (auto& entry : m_tasks) {
            TaskRemoved(entry.first, entry.second.get());
        }
        m_tasks.clear();
        return;
    }
    if (!e.isMap()) {
        return; // malformed
    }
    auto& taskMap = e.Map();
    
    auto previousTasks = std::move(m_tasks);
    m_tasks.clear();
    
    for (auto& entry : taskMap) {
        auto& taskElement = entry.second;
        if (!taskElement.isMap()) {
            continue;
        }
        const MapType& tkmap(taskElement.Map());
		auto it = tkmap.find("name");
        if (it == tkmap.end())
        {
            error() << "task without name";
            continue;
        }
        if (!it->second.isString())
        {
            error() << "task with invalid name";
            continue;
        }

        auto tasksI = previousTasks.find(entry.first);
        std::unique_ptr<Task> task;

        bool newTask = false;
        if (tasksI == previousTasks.end())
        {   // not found, create a new one
            task = std::make_unique<Task>(*this, it->second.asString());
            newTask = true;
        } else {
            task = std::move(tasksI->second);
            previousTasks.erase(entry.first);
        }

		task->updateFromAtlas(tkmap);
		if (newTask) {
			onTaskAdded(entry.first, task.get());
		}
        m_tasks.emplace(entry.first, std::move(task));
    } // of Atlas-specified tasks iteration
    
    for (auto& entry : previousTasks) {

        if (entry.second) {
            TaskRemoved(entry.first, entry.second.get());
        }
    } // of previous-task cleanup iteration
}

void Entity::setLocationFromAtlas(const std::string& locId) {
	if (locId.empty()) {
		return;
	}

	Entity* newLocation = getEntity(locId);
	if (!newLocation) {

		m_waitingForParentBind = true;
		setVisible(false); // fire disappearance, VisChanged if necessary

		if (m_location) {
			removeFromLocation();
		}
		m_location = nullptr;
		assert(!m_visible);
		return;
	}

	setLocation(newLocation);
}

void Entity::setLocation(Entity* newLocation, bool removeFromOldLocation)
{
    if (newLocation == m_location) return;

    if (newLocation) {
		m_waitingForParentBind = newLocation->m_waitingForParentBind;
	}
        
// do the actual member updating
    bool wasVisible = isVisible();
    if (m_location && removeFromOldLocation) {
    	removeFromLocation();
    }
        
    Entity* oldLocation = m_location;
    m_location = newLocation;
    
    onLocationChanged(oldLocation);
    
// fire VisChanged and Appearance/Disappearance signals
    updateCalculatedVisibility(wasVisible);
    
    if (m_location) {
    	addToLocation();
    }
}

void Entity::addToLocation()
{
    assert(!m_location->hasChild(m_id));
    m_location->addChild(this);
}

void Entity::removeFromLocation()
{
    assert(m_location->hasChild(m_id));
    m_location->removeChild(this);
}

void Entity::buildEntityDictFromContents(IdEntityMap& dict)
{
    for (auto& child : m_contents) {
		dict[child->getId()] = child;
    }
}

void Entity::setContentsFromAtlas(const std::vector<std::string>& contents)
{
// convert existing contents into a map, for fast membership tests
    IdEntityMap oldContents;
    buildEntityDictFromContents(oldContents);
    
// iterate over new contents
    for (auto& content : contents) {
        Entity* child = nullptr;

		auto J = oldContents.find(content);
        if (J != oldContents.end()) {
            child = J->second;
            assert(child->getLocation() == this);
            oldContents.erase(J);
        } else {
            child = getEntity(content);
            if (!child) {
            	continue;
            }
            
            if (child->m_waitingForParentBind) {
                assert(!child->m_visible);
                child->m_waitingForParentBind = false;
            }
            
            /* we have found the child, update it's location */
            child->setLocation(this);
        }
    
        child->setVisible(true);
    } // of contents list iteration
    
// mark previous contents which are not in new contents as invisible
    for (auto& entry : oldContents) {
        entry.second->setVisible(false);
    }
}

bool Entity::hasChild(const std::string& eid) const
{
    for (auto& m_content : m_contents) {
        if (m_content->getId() == eid) {
			return true;
		}
    }
    
    return false;
}

void Entity::addChild(Entity* e)
{
    m_contents.push_back(e);
    onChildAdded(e);
    assert(e->getLocation() == this);
}

void Entity::removeChild(Entity* e)
{
    assert(e->getLocation() == this);

    auto I = std::find(m_contents.begin(), m_contents.end(), e);
    if (I != m_contents.end()) {
		m_contents.erase(I);
		onChildRemoved(e);
		return;
    }
	error() << "child " << e->getId() << " of entity " << m_id << " not found doing remove";
}

// visiblity related methods

void Entity::setVisible(bool vis)
{
    // force visibility to false if in limbo; necessary for the character entity,
    // which otherwise gets double appearances on activation
    if (m_waitingForParentBind) vis = false;

    bool wasVisible = isVisible(); // store before we update m_visible
    m_visible = vis;

    updateCalculatedVisibility(wasVisible);
}

bool Entity::isVisible() const
{
    if (m_waitingForParentBind) return false;

    if (m_location) {
		return m_visible && m_location->isVisible();
	} else {
		return m_visible; // only for the root entity
	}
}

void Entity::updateCalculatedVisibility(bool wasVisible)
{
    bool nowVisible = isVisible();
    if (nowVisible == wasVisible) return;
    
    /* the following code looks odd, so remember that only one of nowVisible and
    wasVisible can ever be true. The structure is necessary so that we fire
    Appearances top-down, but Disappearances bottom-up. */
    
    if (nowVisible) {
        onVisibilityChanged(true);
    }
    
    for (auto& item : m_contents) {
        /* in case this isn't clear; if we were visible, then child visibility
        was simply it's locally set value; if we were invisible, that the
        child must also have been invisible too. */
        bool childWasVisible = wasVisible && item->m_visible;
		item->updateCalculatedVisibility(childWasVisible);
    }
    
    if (wasVisible) {
        onVisibilityChanged(false);
    }
}

void Entity::onVisibilityChanged(bool vis)
{
    VisibilityChanged.emit(vis);
}

boost::optional<std::string> Entity::extractEntityId(const Atlas::Message::Element& element)
{
    if (element.isString()) {
        return element.String();
    } else if (element.isMap()) {
        auto I = element.asMap().find("$eid");
        if (I != element.asMap().end() && I->second.isString()) {
            return I->second.String();
        }
    }
    return boost::none;

}


} // of namespace 
