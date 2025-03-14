/*
 * This file is part of Hootenanny.
 *
 * Hootenanny is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --------------------------------------------------------------------
 *
 * The following copyright notices are generated automatically. If you
 * have a new notice to add, please use the format:
 * " * @copyright Copyright ..."
 * This will properly maintain the copyright information. Maxar
 * copyrights will be updated automatically.
 *
 * @copyright Copyright (C) 2015-2023 Maxar (http://www.maxar.com/)
 */
#ifndef OSMMAP_H
#define OSMMAP_H

// GDAL Includes
#include <ogrsf_frmts.h>

// Hoot
#include <hoot/core/elements/ElementIterator.h>
#include <hoot/core/elements/ElementProvider.h>
#include <hoot/core/elements/Node.h>
#include <hoot/core/elements/NodeMap.h>
#include <hoot/core/elements/Relation.h>
#include <hoot/core/elements/RelationMap.h>
#include <hoot/core/elements/Way.h>
#include <hoot/core/elements/WayMap.h>
#include <hoot/core/util/DefaultIdGenerator.h>
#include <hoot/core/util/Units.h>
#include <hoot/core/visitors/ElementVisitor.h>

namespace hoot
{

class ElementId;
class IdSwap;
class OsmMapIndex;
class OsmMapListener;
class Roundabout;
class RubberSheet;

/**
 * The OsmMap contains all the information necessary to represent an OSM map. It holds the nodes,
 * ways, relations and an index to access them efficiently. It also provides a number of methods
 * for CRUD.
 *
 * In the long term it might be nice to remove the OsmIndex circular reference, but haven't
 * figured out a good way to do that. Possibly refactor into an OsmMap class and OsmData class. The
 * OsmMap class maintains pointers to OsmData and an OsmIndex where neither directly references the
 * other.
 */
class OsmMap : public std::enable_shared_from_this<OsmMap>, public ElementProvider, public ElementIterator
{
public:

  static QString className() { return "OsmMap"; }

  OsmMap();
  explicit OsmMap(const std::shared_ptr<const OsmMap>&);
  explicit OsmMap(const std::shared_ptr<OsmMap>&);
  explicit OsmMap(const std::shared_ptr<OGRSpatialReference>& srs);
  ~OsmMap() override = default;

  ///////////////////////////////////////GENERIC ELEMENT////////////////////////////////////////////

  void addElement(const std::shared_ptr<Element>& e);
  template<class T>
  void addElements(T it, T end);

  ConstElementPtr getElement(const ElementId& id) const override;
  ConstElementPtr getElement(ElementType type, long id) const;
  ElementPtr getElement(const ElementId& id);
  ElementPtr getElement(ElementType type, long id);

  size_t getElementCount() const;
  size_t size() const { return getElementCount(); }

  /**
   * Returns true if an element with the specified type/id exists.
   * Throws an exception if the type is unrecognized.
   */
  bool containsElement(const ElementId& eid) const override;
  bool containsElement(ElementType type, long id) const;
  bool containsElement(const std::shared_ptr<const Element>& e) const;

  /**
   * Calls the visitRo method on all elements. See Element::visitRo for a more
   * thorough description.
   *  - The order will always be nodes, ways, relations, but the IDs will not
   *    be in any specific order.
   *  - Unlike Element::visitRo, elements will not be visited multiple times.
   *  - Modifying the OsmMap while traversing will result in undefined behaviour.
   *  - This should be slightly faster than visitRw.
   *
   * If the visitor implements OsmMapConsumer then setOsmMap will be called before visiting any
   * elements.
   */
  void visitRo(ConstElementVisitor& visitor) const;

  /**
   * Calls the visitRw method on all elements. See Element::visitRw for a more
   * thorough description.
   *  - The order will always be nodes, ways, relations, but the IDs will not
   *    be in any specific order.
   *  - Elements that are added during the traversal may or may not be visited.
   *  - Elements may be deleted during traversal.
   *  - The visitor is guaranteed to not visit deleted elements.
   *
   * If the visitor implements OsmMapConsumer then setOsmMap will be called before
   * visiting any elements.
   */
  void visitRw(ElementVisitor& visitor);
  void visitRw(ConstElementVisitor& visitor);

  /**
   * Replace the all instances of from with instances of to. In some cases this may be an invalid
   * operation and an exception will be throw. E.g. replacing a node with a way where the node
   * is part of another way.
   */
  void replace(const std::shared_ptr<const Element>& from, const std::shared_ptr<Element>& to, bool remove_from = true);
  /**
   * Similar to above, but from is replaced with a collection of elements. This makes sense in the
   * context of a relation, but may not make sense in other cases (e.g. replace a single node
   * that is part of a way with multiple nodes).
   */
  void replace(const std::shared_ptr<const Element>& from, const QList<ElementPtr>& to, bool remove_from = true);

  /////////////////////////////////////NODE//////////////////////////////////////////////

  ConstNodePtr getNode(long id) const override;
  NodePtr getNode(long id) override;
  ConstNodePtr getNode(const ElementId& eid) const { return getNode(eid.getId()); }
  NodePtr getNode(const ElementId& eid) { return getNode(eid.getId()); }
  inline const NodeMap& getNodes() const { return _nodes; }
  QSet<long> getNodeIds() const;
  inline long getNodeCount() const { return _nodes.size(); }

  /**
   * Returns true if the node is in this map.
   */
  bool containsNode(long id) const override { return _nodes.find(id) != _nodes.end(); }

  void addNode(const NodePtr& n);
  /**
   * Add all the nodes in the provided vector. This can be faster than calling addNode multiple
   * times.
   */
  void addNodes(const std::vector<NodePtr>& nodes);

  /**
   * Intelligently replaces all instances of oldNode with newNode. This looks at all the ways
   * for references to oldNode and replaces those references with newNode. Finally, oldNode is
   * removed from this OsmMap entirely.
   */
  void replaceNode(long oldId, long newId);
  /**
   * Replace all of the nodes in the map<from, to> all at one time, deleting the 'from' nodes after updating
   * all ways and relations with the new values, used to quickly replace large quantities of nodes
   */
  void replaceNodes(const std::map<long, long>& replacements);

  long createNextNodeId() const { return _idGen->createNodeId(); }

  void visitNodesRo(ConstElementVisitor& visitor) const;
  void visitNodesRw(ElementVisitor& visitor);

  int numNodesAppended() const { return _numNodesAppended; }
  int numNodesSkippedForAppending() const { return _numNodesSkippedForAppending; }

  ///////////////////////////////////////WAY////////////////////////////////////////////////

  /**
   * Return the way with the specified id or null if it doesn't exist.
   */
  WayPtr getWay(long id) override;
  WayPtr getWay(ElementId eid);
  /**
   * Similar to above but const'd.
   *
   * We can't return these values by reference b/c the conversion from non-const to const requires
   * a copy. The copy would be a temporary variable if we returned a reference which creates some
   * weirdness and a warning.
   */
  ConstWayPtr getWay(long id) const override;
  ConstWayPtr getWay(ElementId eid) const;
  inline const WayMap& getWays() const { return _ways; }
  QSet<long> getWayIds() const;
  inline long getWayCount() const { return _ways.size(); }

  void addWay(const WayPtr& w);

  bool containsWay(long id) const override { return _ways.find(id) != _ways.end(); }

  long createNextWayId() const { return _idGen->createWayId(); }

  void visitWaysRo(ConstElementVisitor& visitor) const;
  void visitWaysRw(ElementVisitor& visitor);

  int numWaysAppended() const { return _numWaysAppended; }
  int numWaysSkippedForAppending() const { return _numWaysSkippedForAppending; }

  /** Removing multiple ways using other methods will trigger the indices in this object
   *  to be rebuilt between each delete operation which is expensive, this method will delete
   *  all of the ways in one shot and rebuild the indices afterwards.
   * @param way_ids Vector of way IDs that are to be removed
   * @param removeFully When set to true, way IDs are removed from relations in the map too
   *  when false, the way is removed from the map only, relations still reference the way ID
   */
  void bulkRemoveWays(const std::vector<long>& way_ids, bool removeFully);

  ////////////////////////////////////////RELATION/////////////////////////////////////////////////

  ConstRelationPtr getRelation(long id) const override;
  RelationPtr getRelation(long id) override;
  ConstRelationPtr getRelation(ElementId eid) const;
  inline const RelationMap& getRelations() const { return _relations; }
  QSet<long> getRelationIds() const;
  inline long getRelationCount() const { return _relations.size(); }

  void addRelation(const RelationPtr& r);

  bool containsRelation(long id) const override { return _relations.find(id) != _relations.end(); }

  long createNextRelationId() const { return _idGen->createRelationId(); }

  void visitRelationsRo(ConstElementVisitor& visitor) const;
  void visitRelationsRw(ElementVisitor& visitor);

  int numRelationsAppended() const { return _numRelationsAppended; }
  int numRelationsSkippedForAppending() const { return _numRelationsSkippedForAppending; }

  /////////////////////////////////////////////////////////////////////////////////////

  /**
   * Append all the elements in input map to this map.
   *
   * The default behavior is to skip an element from the map being appended from if it has the same
   * ID as an element in this map and the elements are considered identical. If the elements are
   * considered to be identical, an error occurs. Alternatively, the throwOutDupes parameter will
   * allow for overriding that behavior at the expense of not appending the elements.
   *
   * @param map
   * @param throwOutDupes if true, and elements in the map being appended from have the same IDs as
   * elements in this map, those elements are ignored
   * @throws if there is element ID overlap and throwOutDupes = false
   * @throws if the map being appended to is the same as the map being appended from
   * @throws if the map being appended to does not have the same projection as the map being
   * appended from
   */
  void append(const std::shared_ptr<const OsmMap>& map, const bool throwOutDupes = false);
  void clear();
  bool isEmpty() const { return getElementCount() == 0; }

  /**
   * Validates the consistency of the map. Primarily this checks to make sure that all nodes
   * referenced by a way exist in the map. A full dump of all invalid ways is logged before the
   * function throws an error.
   * @param strict If true, the method throws an exception rather than returning a result if the
   *               validation fails.
   * @return True if the map is valid, false otherwise.
   */
  bool validate(bool strict = true) const;

  std::set<ElementId> getParents(ElementId eid) const;

  /**
   * Returns the SRS for this map. The SRS should never be changed and defaults to WGS84.
   */
  std::shared_ptr<OGRSpatialReference> getProjection() const override { return _srs; }
  QString getProjectionEpsgString() const;

  void registerListener(const std::shared_ptr<OsmMapListener>& l) { _listeners.push_back(l); }

  /**
   * Resets the way and node counters. This should ONLY BE CALLED BY UNIT TESTS.
   */
  static void resetCounters() { IdGenerator::getInstance()->reset(); }

  void appendSource(const QString& url);
  void replaceSource(const QString& url);

  void resetIterator() override;

  /**
   * This returns an index of the OsmMap. Adding or removing ways from the map will make the index
   * out of date and will require calling getIndex again.
   */
  const OsmMapIndex& getIndex() const { return *_index; }
  const std::vector<std::shared_ptr<OsmMapListener>>& getListeners() const { return _listeners; }
  const IdGenerator& getIdGenerator() const { return *_idGen; }
  QString getSource() const;
  std::vector<std::shared_ptr<Roundabout>> getRoundabouts() const { return _roundabouts; }
  std::shared_ptr<IdSwap> getIdSwap() const { return _idSwap; }
  QString getName() const { return _name; }

  void setName(const QString& name) { _name = name; }
  void setIdSwap(const std::shared_ptr<IdSwap>& swap) { _idSwap = swap; }
  void setRoundabouts(const std::vector<std::shared_ptr<Roundabout>>& rnd) { _roundabouts = rnd; }
  void setProjection(const std::shared_ptr<OGRSpatialReference>& srs);
  void setEnableProgressLogging(bool enable) { _enableProgressLogging = enable; }
  void setCachedRubberSheet(std::shared_ptr<RubberSheet> rubbersheet)
  { _cachedRubberSheet = rubbersheet; }
  std::shared_ptr<RubberSheet> getCachedRubberSheet() const { return _cachedRubberSheet; }
  void setIdGenerator(const std::shared_ptr<IdGenerator>& gen) { _idGen = gen;  }

protected:

  void _next() override;

private:

  // Friend classes that need to modify private elements
  friend class RemoveNodeByEid;
  friend class RemoveWayByEid;
  friend class RemoveRelationByEid;

  mutable std::shared_ptr<IdGenerator> _idGen;

  std::shared_ptr<OGRSpatialReference> _wgs84;

  std::shared_ptr<OGRSpatialReference> _srs;

  mutable NodeMap _nodes;
  mutable RelationMap _relations;
  mutable WayMap _ways;

  std::shared_ptr<OsmMapIndex> _index;
  NodePtr _nullNode;
  ConstNodePtr _constNullNode;
  RelationPtr _nullRelation;
  WayPtr _nullWay;
  ConstWayPtr _constNullWay;
  mutable NodeMap::const_iterator _tmpNodeMapIt;
  RelationMap::iterator _tmpRelationIt;
  mutable WayMap::const_iterator _tmpWayIt;
  std::vector<std::shared_ptr<OsmMapListener>> _listeners;

  std::vector<std::shared_ptr<Element>> _replaceTmpArray;

  std::vector<std::shared_ptr<Roundabout>> _roundabouts;

  std::shared_ptr<IdSwap> _idSwap;

  // useful during debugging
  QString _name;
  /** List of source URLs of map data */
  std::set<QString> _sources;

  int _numNodesAppended;
  int _numWaysAppended;
  int _numRelationsAppended;
  int _numNodesSkippedForAppending;
  int _numWaysSkippedForAppending;
  int _numRelationsSkippedForAppending;

  // If we're making recursive calls to the visit methods in another class doing its own progress
  // logging, its helpful to be able to turn loging here off.
  bool _enableProgressLogging;

  // for use with ElementIterator
  ElementId _currentElementId;
  NodeMap::const_iterator _currentNodeItr;
  WayMap::const_iterator _currentWayItr;
  RelationMap::const_iterator _currentRelationItr;

  std::shared_ptr<RubberSheet> _cachedRubberSheet;

  void _copy(const std::shared_ptr<const OsmMap>& from);

  /**
   * Returns true if there is a node in l.
   */
  bool _listContainsNode(const QList<ElementPtr> l) const;

  void _replaceNodeInRelations(long oldId, long newId);

  void _initCounters();
};

using OsmMapPtr = std::shared_ptr<OsmMap>;
using ConstOsmMapPtr = std::shared_ptr<const OsmMap>;

template<class T>
void OsmMap::addElements(T it, T end)
{
  while (it != end)
  {
    addElement(*it);
    ++it;
  }
}

inline NodePtr OsmMap::getNode(long id)
{
  _tmpNodeMapIt = _nodes.find(id);
  if (_tmpNodeMapIt != _nodes.end())
    return _tmpNodeMapIt->second;
  else
    return _nullNode;
}

inline ConstNodePtr OsmMap::getNode(long id) const
{
  _tmpNodeMapIt = _nodes.find(id);
  if (_tmpNodeMapIt != _nodes.end())
    return _tmpNodeMapIt->second;
  else
    return _constNullNode;
}

inline ConstRelationPtr OsmMap::getRelation(long id) const
{
  auto it = _relations.find(id);
  if (it != _relations.end())
    return it->second;
  else
    return _nullRelation;
}

inline ConstRelationPtr OsmMap::getRelation(ElementId eid) const
{
  return getRelation(eid.getId());
}

inline RelationPtr OsmMap::getRelation(long id)
{
  _tmpRelationIt = _relations.find(id);
  if (_tmpRelationIt != _relations.end())
    return _tmpRelationIt->second;
  else
    return _nullRelation;
}

inline ConstWayPtr OsmMap::getWay(long id) const
{
  _tmpWayIt = _ways.find(id);
  if (_tmpWayIt != _ways.end())
    return _tmpWayIt->second;
  else
    return _constNullWay;
}

inline ConstWayPtr OsmMap::getWay(ElementId eid) const
{
  return getWay(eid.getId());
}

inline WayPtr OsmMap::getWay(long id)
{
  _tmpWayIt = _ways.find(id);
  if (_tmpWayIt != _ways.end())
    return _tmpWayIt->second;
  else
    return _nullWay;
}

inline WayPtr OsmMap::getWay(ElementId eid)
{
  return getWay(eid.getId());
}

}

#endif
