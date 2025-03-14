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
 * @copyright Copyright (C) 2016-2023 Maxar (http://www.maxar.com/)
 */
#include "HootApiDb.h"

// hoot
#include <hoot/core/elements/Relation.h>
#include <hoot/core/io/InternalIdReserver.h>
#include <hoot/core/io/ServicesJobStatus.h>
#include <hoot/core/io/SqlBulkInsert.h>
#include <hoot/core/io/TableType.h>
#include <hoot/core/util/ConfigOptions.h>
#include <hoot/core/util/DateTimeUtils.h>
#include <hoot/core/util/DbUtils.h>
#include <hoot/core/util/HootException.h>
#include <hoot/core/util/Log.h>
#include <hoot/core/util/UuidHelper.h>

// Qt
#include <QStringList>
#include <QVariant>
#include <QtSql/QSqlError>
#include <QtSql/QSqlRecord>

// Standard
#include <math.h>
#include <cmath>
#include <fstream>

// tgs
#include <tgs/System/Time.h>

using namespace geos::geom;
using namespace std;

namespace hoot
{

HootApiDb::HootApiDb()
  : _ignoreInsertConflicts(ConfigOptions().getMapMergeIgnoreDuplicateIds()),
    _precision(ConfigOptions().getWriterPrecision()),
    _createIndexesOnClose(true),
    _flushOnClose(true)
{
  _init();
}

HootApiDb::~HootApiDb()
{
  close();
}

void HootApiDb::_init()
{
  _floatingPointCoords = true;
  _capitalizeRelationMemberType = false;
  _inTransaction = false;

  int recordsPerBulkInsert = 500;

  // set it to something obsurd.
  _lastMapId = -numeric_limits<long>::max();

  _nodesInsertElapsed = 0;
  // 500 found experimentally on my desktop -JRS
  _nodesPerBulkInsert = recordsPerBulkInsert;
  _nodesPerBulkDelete = recordsPerBulkInsert;

  _wayNodesInsertElapsed = 0;
  // arbitrary, needs benchmarking
  _wayNodesPerBulkInsert = recordsPerBulkInsert;

  _wayInsertElapsed = 0;
  // arbitrary, needs benchmarking
  _waysPerBulkInsert = recordsPerBulkInsert;

  // arbitrary, needs benchmarking
  _relationsPerBulkInsert = recordsPerBulkInsert;

  _currUserId = -1;
  _currMapId = -1;
  _currChangesetId = -1;
  _changesetEnvelope.init();
  _changesetChangeCount = 0;

  //  Set the max node/way/relation IDs to negative for updating sequences after inserts
  _maxInsertNodeId = -1;
  _maxInsertWayId = -1;
  _maxInsertRelationId = -1;
}

Envelope HootApiDb::calculateEnvelope() const
{
  LOG_TRACE("Calculating envelope...");

  const long mapId = _currMapId;
  Envelope result;

  // if you're having performance issues read this:
  // http://www.postgresql.org/docs/8.0/static/functions-aggregate.html
  QSqlQuery boundsQuery =
    _exec(QString("SELECT MIN(latitude) as minLat, MAX(latitude) AS maxLat "
                  ", MIN(longitude) as minLon, MAX(longitude) AS maxLon"
                  " FROM %1").arg(getCurrentNodesTableName(mapId)));

  if (boundsQuery.next())
  {
    double minY = boundsQuery.value(0).toDouble();
    double maxY = boundsQuery.value(1).toDouble();
    double minX = boundsQuery.value(2).toDouble();
    double maxX = boundsQuery.value(3).toDouble();
    result = Envelope(minX, maxX, minY, maxY);
  }
  else
  {
    QString error = QString("Error calculating bounds: %1").arg(boundsQuery.lastError().text());
    throw HootException(error);
  }

  return result;
}

void HootApiDb::_checkLastMapId(long mapId)
{
  LOG_TRACE("Checking last map ID: " << mapId << "...");
  LOG_VART(_lastMapId);
  if (_lastMapId != mapId)
  {
    _flushBulkInserts();
    _flushBulkDeletes();
    _resetQueries();
    _nodeIdReserver.reset();
    _wayIdReserver.reset();
    _relationIdReserver.reset();
    _lastMapId = mapId;
    LOG_VART(_lastMapId);
  }
}

void HootApiDb::close()
{
  LOG_TRACE("Closing database connection...");

  if (_createIndexesOnClose)
    createPendingMapIndexes();

  if (_flushOnClose)
  {
    _flushBulkInserts();
    _flushBulkDeletes();
  }

  _resetQueries();

  if (_inTransaction)
  {
    LOG_WARN("Closing database before transaction is committed. Rolling back transaction.");
    rollback();
  }

  // Seeing this? "Unable to free statement: connection pointer is NULL"
  // Make sure all queries are listed in _resetQueries.
  _db.close();
}

void HootApiDb::commit()
{
  LOG_TRACE("Committing transaction...");

  if (_db.isOpen() == false)
    throw HootException("Tried to commit a transaction on a closed database.");

  if (_inTransaction == false)
    throw HootException(QString("Tried to commit but weren't in a transaction.  You may need to set hootapi.db.writer.create.user=true."));

  createPendingMapIndexes();
  _flushBulkInserts();
  _flushBulkDeletes();
  _resetQueries();
  if (!_db.commit())
    throw HootException("Error committing transaction: " + _db.lastError().text());

  _inTransaction = false;

  // If we get this far, transaction commit was successful. Now execute
  // all of our post-transaction tasks. (like drop database statements)
  // A task that throws an error will prevent subsequent tasks from executing.
  QVectorIterator<QString> taskIt(_postTransactionStatements);
  while (taskIt.hasNext())
  {
    QString task = taskIt.next();
    DbUtils::execNoPrepare(_db, task);
  }
}

void HootApiDb::_copyTableStructure(const QString& from, const QString& to) const
{
  // inserting strings in this fashion is safe b/c it is private and we closely control the table
  // names.
  QString sql = QString("CREATE TABLE %1 (LIKE %2 INCLUDING DEFAULTS INCLUDING CONSTRAINTS INCLUDING INDEXES)").arg(to, from);
  QSqlQuery q(_db);

  LOG_VART(sql);
  if (q.exec(sql) == false)
    throw HootException(QString("Error executing query: %1 (%2)").arg(q.lastError().text(), sql));
}

void HootApiDb::createPendingMapIndexes()
{
  if (!_pendingMapIndexes.empty())
  {
    LOG_DEBUG("Creating " << _pendingMapIndexes.size() << " map indexes...");
  }

  for (auto mapId : qAsConst(_pendingMapIndexes))
  {
    DbUtils::execNoPrepare(
      _db,
      QString("ALTER TABLE %1 "
        "ADD CONSTRAINT current_nodes_changeset_id_fkey_%2 FOREIGN KEY (changeset_id) "
          "REFERENCES %3 (id) MATCH SIMPLE "
          "ON UPDATE NO ACTION ON DELETE NO ACTION ")
          .arg(getCurrentNodesTableName(mapId), getMapIdString(mapId), getChangesetsTableName(mapId)));

    DbUtils::execNoPrepare(
      _db,
      QString("CREATE INDEX %1_tile_idx ON %1 USING btree (tile)")
        .arg(getCurrentNodesTableName(mapId)));

    DbUtils::execNoPrepare(
      _db,
      QString("ALTER TABLE %1 "
        "ADD CONSTRAINT current_relations_changeset_id_fkey_%2 FOREIGN KEY (changeset_id) "
          "REFERENCES %3 (id) MATCH SIMPLE "
          "ON UPDATE NO ACTION ON DELETE NO ACTION ")
          .arg(getCurrentRelationsTableName(mapId), getMapIdString(mapId), getChangesetsTableName(mapId)));

    DbUtils::execNoPrepare(
      _db,
      QString("ALTER TABLE %1 "
        "ADD CONSTRAINT current_way_nodes_node_id_fkey_%2 FOREIGN KEY (node_id) "
          "REFERENCES %3 (id) MATCH SIMPLE "
          "ON UPDATE NO ACTION ON DELETE NO ACTION, "
        "ADD CONSTRAINT current_way_nodes_way_id_fkey_%2 FOREIGN KEY (way_id) "
          "REFERENCES %4 (id) MATCH SIMPLE "
          "ON UPDATE NO ACTION ON DELETE NO ACTION")
          .arg(getCurrentWayNodesTableName(mapId), getMapIdString(mapId),
               getCurrentNodesTableName(mapId), getCurrentWaysTableName(mapId)));

    DbUtils::execNoPrepare(
      _db,
      QString("ALTER TABLE %1 "
        "ADD CONSTRAINT current_ways_changeset_id_fkey_%2 FOREIGN KEY (changeset_id) "
          "REFERENCES %3 (id) MATCH SIMPLE "
          "ON UPDATE NO ACTION ON DELETE NO ACTION ")
          .arg(getCurrentWaysTableName(mapId), getMapIdString(mapId), getChangesetsTableName(mapId)));
  }

  _pendingMapIndexes.clear();
}

void HootApiDb::deleteMap(long mapId) const
{
  //  Don't try to delete an invalid map ID
  if (mapId == -1)
    return;

  LOG_STATUS("Deleting map: " << mapId << "...");

  // Drop related sequences
  dropSequence(getCurrentRelationMembersSequenceName(mapId));
  dropSequence(getCurrentRelationsSequenceName(mapId));
  dropSequence(getCurrentWayNodesSequenceName(mapId));
  dropSequence(getCurrentWaysSequenceName(mapId));
  dropSequence(getCurrentNodesSequenceName(mapId));
  dropSequence(getChangesetsSequenceName(mapId));

  // Drop related tables
  dropTable(getCurrentRelationMembersTableName(mapId));
  dropTable(getCurrentRelationsTableName(mapId));
  dropTable(getCurrentWayNodesTableName(mapId));
  dropTable(getCurrentWaysTableName(mapId));
  dropTable(getCurrentNodesTableName(mapId));
  dropTable(getChangesetsTableName(mapId));

  // Delete map last
  _exec("DELETE FROM " + getMapsTableName() + " WHERE id=:id", (qlonglong)mapId);

  LOG_DEBUG("Finished deleting map: " << mapId << ".");
}

bool HootApiDb::hasTable(const QString& tableName) const
{
  QString sql = "SELECT 1 from pg_catalog.pg_class c JOIN pg_catalog.pg_namespace n ON "
                "n.oid = c.relnamespace WHERE c.relname = :name";
  QSqlQuery q = _exec(sql, tableName);

  return q.next();
}

void HootApiDb::dropTable(const QString& tableName) const
{
  LOG_TRACE("Dropping table: " << tableName << "...");

  // inserting strings in this fashion is safe b/c it is private and we closely control the table
  // names.
  QString sql = QString("DROP TABLE IF EXISTS %1 CASCADE;").arg(tableName);
  QSqlQuery q(_db);

  if (q.exec(sql) == false)
    throw HootException(QString("Error executing query: %1 (%2)").arg(q.lastError().text(), sql));
}

void HootApiDb::dropSequence(const QString& sequenceName) const
{
  LOG_TRACE("Dropping sequence: " << sequenceName << "...");

  // inserting strings in this fashion is safe b/c it is private and we closely control the sequence
  // names.
  QString sql = QString("DROP SEQUENCE IF EXISTS %1 CASCADE;").arg(sequenceName);
  QSqlQuery q(_db);

  if (q.exec(sql) == false)
    throw HootException(QString("Error executing query: %1 (%2)").arg(q.lastError().text(), sql));
}

void HootApiDb::deleteUser(long userId)
{
  LOG_TRACE("Deleting user: " << userId << "...");

  QSqlQuery maps = _exec(QString("SELECT id FROM %1 WHERE user_id=:user_id").arg(getMapsTableName()), (qlonglong)userId);

  // delete all the maps owned by this user
  while (maps.next())
  {
    long mapId = maps.value(0).toLongLong();
    deleteMap(mapId);
  }

  _exec(QString("DELETE FROM %1 WHERE id=:id").arg(ApiDb::getUsersTableName()), (qlonglong)userId);
}

QString HootApiDb::_escapeTags(const Tags& tags)
{
  QStringList l;
  static QChar f1('\\'), f2('"');

  for (auto it = tags.begin(); it != tags.end(); ++it)
  {
    QString key = it.key();
    QString val = it.value().trimmed();
    if (val.isEmpty() == false)
    {
      // this doesn't appear to be working, but I think it is implementing the spec as described here:
      // http://www.postgresql.org/docs/9.0/static/hstore.html
      // The spec described above does seem to work on the psql command line. Curious.
      QString k = QString(key).replace(f1, "\\\\").replace(f2, "\\\"").replace("'", "''");
      QString v = QString(val).replace(f1, "\\\\").replace(f2, "\\\"").replace("'", "''");

      l << QString("'%1'").arg(k);
      l << QString("'%1'").arg(v);
    }
  }

  QString hstoreStr = l.join(",");
  if (!hstoreStr.isEmpty())
     hstoreStr = "hstore(ARRAY[" + hstoreStr + "])";
  else
     hstoreStr = "''";
  return hstoreStr;
}

QString HootApiDb::execToString(QString sql, QVariant v1, QVariant v2, QVariant v3) const
{
  QSqlQuery q = _exec(sql, v1, v2, v3);

  QStringList l;
  while (q.next())
  {
    QStringList row;
    for (int i = 0; i < q.record().count(); ++i)
      row.append(q.value(i).toString());

    l.append(row.join(";"));
  }
  q.finish();
  return l.join("\n");
}

void HootApiDb::_flushBulkInserts() const
{
  LOG_TRACE("Flushing bulk inserts...");

  if (_nodeBulkInsert != nullptr)
  {
    LOG_VART(_nodeBulkInsert->getPendingCount());
    _nodeBulkInsert->flush();
  }
  if (_wayBulkInsert != nullptr)
  {
    LOG_VART(_wayBulkInsert->getPendingCount());
    _wayBulkInsert->flush();
  }
  if (_wayNodeBulkInsert != nullptr)
  {
    LOG_VART(_wayNodeBulkInsert->getPendingCount());
    _wayNodeBulkInsert->flush();
  }
  if (_relationBulkInsert != nullptr)
  {
    LOG_VART(_relationBulkInsert->getPendingCount());
    _relationBulkInsert->flush();
  }
}

void HootApiDb::_flushBulkDeletes() const
{
  LOG_TRACE("Flushing bulk deletes...");

  if (_nodeBulkDelete != nullptr)
  {
    LOG_VART(_nodeBulkDelete->getPendingCount());
    _nodeBulkDelete->flush();
  }
}

bool HootApiDb::isCorrectHootDbVersion()
{
  return getHootDbVersion() == ApiDb::expectedHootDbVersion();
}

QString HootApiDb::getHootDbVersion()
{
  if (_selectHootDbVersion == nullptr)
  {
    _selectHootDbVersion = std::make_shared<QSqlQuery>(_db);
    _selectHootDbVersion->prepare("SELECT id || ':' || author AS version_id FROM databasechangelog "
                                  "ORDER BY dateexecuted DESC LIMIT 1");
  }

  if (_selectHootDbVersion->exec() == false)
    throw HootException(_selectHootDbVersion->lastError().text());

  QString result;
  if (_selectHootDbVersion->next())
    result = _selectHootDbVersion->value(0).toString();
  else
    throw HootException("Unable to retrieve the DB version.");

  return result;
}

long HootApiDb::_getNextNodeId()
{
  const long mapId = _currMapId;
  _checkLastMapId(mapId);
  if (_nodeIdReserver == nullptr)
    _nodeIdReserver = std::make_shared<InternalIdReserver>(_db, getCurrentNodesSequenceName(mapId));

  return _nodeIdReserver->getNextId();
}

long HootApiDb::_getNextRelationId()
{
  const long mapId = _currMapId;
  _checkLastMapId(mapId);
  if (_relationIdReserver == nullptr)
    _relationIdReserver = std::make_shared<InternalIdReserver>(_db, getCurrentRelationsSequenceName(mapId));

  return _relationIdReserver->getNextId();
}

long HootApiDb::_getNextWayId()
{
  const long mapId = _currMapId;
  _checkLastMapId(mapId);
  if (_wayIdReserver == nullptr)
    _wayIdReserver = std::make_shared<InternalIdReserver>(_db, getCurrentWaysSequenceName(mapId));

  return _wayIdReserver->getNextId();
}

void HootApiDb::beginChangeset()
{
  Tags emptyTags;
  return beginChangeset(emptyTags);
}

void HootApiDb::beginChangeset(const Tags& tags)
{
  LOG_TRACE("Starting changeset...");

  _changesetEnvelope.init();
  _changesetChangeCount = 0;
  const long mapId = _currMapId;
  const long userId = _currUserId;

  _checkLastMapId(mapId);
  if (_insertChangeSet == nullptr)
  {
    _insertChangeSet = std::make_shared<QSqlQuery>(_db);
    _insertChangeSet->prepare(
      QString("INSERT INTO %1 (user_id, created_at, min_lat, max_lat, min_lon, max_lon, "
        "closed_at, tags) "
        "VALUES (:user_id, NOW(), :min_lat, :max_lat, :min_lon, :max_lon, NOW(), %2) "
        "RETURNING id")
        .arg(getChangesetsTableName(mapId), _escapeTags(tags)));
  }
  _insertChangeSet->bindValue(":user_id", (qlonglong)userId);
  _insertChangeSet->bindValue(":min_lat", _changesetEnvelope.isNull() ?  0.0 : _changesetEnvelope.getMinY());
  _insertChangeSet->bindValue(":max_lat", _changesetEnvelope.isNull() ? -1.0 : _changesetEnvelope.getMaxY());
  _insertChangeSet->bindValue(":min_lon", _changesetEnvelope.isNull() ?  0.0 : _changesetEnvelope.getMinX());
  _insertChangeSet->bindValue(":max_lon", _changesetEnvelope.isNull() ? -1.0 : _changesetEnvelope.getMaxX());
  LOG_VART(_insertChangeSet->lastQuery());

  _currChangesetId = _insertRecord(*_insertChangeSet);
  LOG_VART(_currChangesetId);

  _changesetChangeCount = 0;
  LOG_TRACE("Started new changeset " << QString::number(_currChangesetId));
}

long HootApiDb::insertChangeset(const geos::geom::Envelope& bounds, const Tags& tags,
                                const long numChanges)
{
  LOG_TRACE("Inserting and closing changeset...");

  const long mapId = _currMapId;
  const long userId = _currUserId;

  _checkLastMapId(mapId);
  if (_insertChangeSet2 == nullptr)
  {
    _insertChangeSet2 = std::make_shared<QSqlQuery>(_db);
    _insertChangeSet2->prepare(
      QString("INSERT INTO %1 (user_id, created_at, min_lat, max_lat, min_lon, max_lon, "
        "closed_at, num_changes, tags) "
        "VALUES (:user_id, NOW(), :min_lat, :max_lat, :min_lon, :max_lon, NOW(), :num_changes, %2) "
        "RETURNING id")
        .arg(getChangesetsTableName(mapId), _escapeTags(tags)));
  }
  _insertChangeSet2->bindValue(":user_id", (qlonglong)userId);
  _insertChangeSet2->bindValue(":min_lat", bounds.isNull() ?  0.0 : bounds.getMinY());
  _insertChangeSet2->bindValue(":max_lat", bounds.isNull() ? -1.0 : bounds.getMaxY());
  _insertChangeSet2->bindValue(":min_lon", bounds.isNull() ?  0.0 : bounds.getMinX());
  _insertChangeSet2->bindValue(":max_lon", bounds.isNull() ? -1.0 : bounds.getMaxX());
  _insertChangeSet2->bindValue(":num_changes", (int)numChanges);
  LOG_VART(_insertChangeSet2->lastQuery());

  _currChangesetId = _insertRecord(*_insertChangeSet2);
  LOG_VART(_currChangesetId);

  _changesetChangeCount = 0;
  LOG_TRACE("Inserted and closed changeset " << QString::number(_currChangesetId));

  return _currChangesetId;
}

void HootApiDb::endChangeset()
{
  LOG_TRACE("Ending changeset...");

  // If we're already closed, nothing to do
  if (_currChangesetId == -1)
  {
    LOG_DEBUG("Tried to end a changeset but there isn't an active changeset currently.");
    return;
  }

  const long mapId = _currMapId;
  if (!changesetExists(_currChangesetId))
    throw HootException(QString("No changeset exists with ID: %1").arg(_currChangesetId));

  _checkLastMapId(mapId);
  if (_closeChangeSet == nullptr)
  {
    _closeChangeSet = std::make_shared<QSqlQuery>(_db);
    _closeChangeSet->prepare(
      QString("UPDATE %1 SET min_lat=:min_lat, max_lat=:max_lat, min_lon=:min_lon, "
        "max_lon=:max_lon, closed_at=NOW(), num_changes=:num_changes WHERE id=:id")
         .arg(getChangesetsTableName(mapId)));
  }
  _closeChangeSet->bindValue(":min_lat", _changesetEnvelope.isNull() ?  0.0 : _changesetEnvelope.getMinY());
  _closeChangeSet->bindValue(":max_lat", _changesetEnvelope.isNull() ? -1.0 : _changesetEnvelope.getMaxY());
  _closeChangeSet->bindValue(":min_lon", _changesetEnvelope.isNull() ?  0.0 : _changesetEnvelope.getMinX());
  _closeChangeSet->bindValue(":max_lon", _changesetEnvelope.isNull() ? -1.0 : _changesetEnvelope.getMaxX());
  _closeChangeSet->bindValue(":num_changes", (int)_changesetChangeCount);
  _closeChangeSet->bindValue(":id", (qlonglong)_currChangesetId);
  LOG_VART(_closeChangeSet->lastQuery());

  if (_closeChangeSet->exec() == false)
  {
    LOG_ERROR("query bound values: ");
    LOG_ERROR(_closeChangeSet->boundValues());
    LOG_ERROR("\n");
    throw HootException(
      QString("Error executing close changeset: %1 (SQL: %2) with envelope: %3")
        .arg(_closeChangeSet->lastError().text(), _closeChangeSet->executedQuery(), QString::fromStdString(_changesetEnvelope.toString())));
  }

  LOG_DEBUG("Successfully closed changeset " << QString::number(_currChangesetId));

  // NOTE: do *not* alter _currChangesetId or _changesetEnvelope yet.  We haven't written data to
  // database yet!   they will be refreshed upon opening a new database, so leave them alone!
  _changesetChangeCount = 0;
}

long HootApiDb::insertMap(QString displayName)
{
  LOG_TRACE("Inserting map...");

  const int userId = static_cast<int>(_currUserId);

  if (_insertMap == nullptr)
  {
    _insertMap = std::make_shared<QSqlQuery>(_db);
    _insertMap->prepare(
      QString("INSERT INTO %1 (display_name, user_id, public, created_at) "
              "VALUES (:display_name, :user_id, :public, NOW()) "
              "RETURNING id").arg(getMapsTableName()));
  }
  _insertMap->bindValue(":display_name", displayName);
  _insertMap->bindValue(":user_id", userId);
  _insertMap->bindValue(":public", false);

  long mapId = _insertRecord(*_insertMap);

  _copyTableStructure(ApiDb::getChangesetsTableName(), getChangesetsTableName(mapId));
  _copyTableStructure(ApiDb::getCurrentNodesTableName(), getCurrentNodesTableName(mapId));
  _copyTableStructure(ApiDb::getCurrentRelationMembersTableName(), getCurrentRelationMembersTableName(mapId));
  _copyTableStructure(ApiDb::getCurrentRelationsTableName(), getCurrentRelationsTableName(mapId));
  _copyTableStructure(ApiDb::getCurrentWayNodesTableName(), getCurrentWayNodesTableName(mapId));
  _copyTableStructure(ApiDb::getCurrentWaysTableName(), getCurrentWaysTableName(mapId));

  DbUtils::execNoPrepare(_db, "CREATE SEQUENCE " + getChangesetsSequenceName(mapId));
  DbUtils::execNoPrepare(_db, "CREATE SEQUENCE " + getCurrentNodesSequenceName(mapId));
  DbUtils::execNoPrepare(_db, "CREATE SEQUENCE " + getCurrentRelationsSequenceName(mapId));
  DbUtils::execNoPrepare(_db, "CREATE SEQUENCE " + getCurrentWaysSequenceName(mapId));

  DbUtils::execNoPrepare(
    _db,
    QString("ALTER TABLE %1 "
      "ALTER COLUMN id SET DEFAULT NEXTVAL('%2'::regclass)")
        .arg(getCurrentNodesTableName(mapId), getCurrentNodesSequenceName(mapId)));

  DbUtils::execNoPrepare(
    _db,
    QString("ALTER TABLE %1 "
      "ALTER COLUMN id SET DEFAULT NEXTVAL('%2'::regclass)")
        .arg(getCurrentRelationsTableName(mapId), getCurrentRelationsSequenceName(mapId)));

  DbUtils::execNoPrepare(
    _db,
    QString("ALTER TABLE %1 "
      "ALTER COLUMN id SET DEFAULT NEXTVAL('%2'::regclass)")
        .arg(getCurrentWaysTableName(mapId), getCurrentWaysSequenceName(mapId)));

  // remove the index to speed up inserts. It'll be added back by createPendingMapIndexes
  DbUtils::execNoPrepare(
    _db,
    QString("DROP INDEX %1_tile_idx").arg(getCurrentNodesTableName(mapId)));

  _pendingMapIndexes.append(mapId);

  return mapId;
}

bool HootApiDb::insertNode(const double lat, const double lon, const Tags& tags, long& assignedId,
                           long version)
{
  assignedId = _getNextNodeId();
  return insertNode(assignedId, lat, lon, tags, version);
}

bool HootApiDb::insertNode(const long id, const double lat, const double lon, const Tags &tags,
                           long version)
{
  LOG_TRACE("Inserting node: " << id << "...");

  const long mapId = _currMapId;
  double start = Tgs::Time::getTime();

  _checkLastMapId(mapId);

  if (_nodeBulkInsert == nullptr)
  {
    QStringList columns({"id", "latitude", "longitude", "changeset_id", "timestamp", "tile", "version", "tags"});

    _nodeBulkInsert = std::make_shared<SqlBulkInsert>(_db, getCurrentNodesTableName(mapId), columns, _ignoreInsertConflicts);
  }

  QList<QVariant> v;
  v.append((qlonglong)id);
  v.append(lat);
  v.append(lon);
  v.append((qlonglong)_currChangesetId);
  v.append(DateTimeUtils::currentTimeAsString());
  v.append(tileForPoint(lat, lon));
  if (version == 0)
    v.append((qlonglong)1);
  else
    v.append((qlonglong)version);
  // escaping tags ensures that we won't introduce a SQL injection vulnerability, however, if a
  // bad tag is passed and it isn't escaped properly (shouldn't happen) it may result in a syntax
  // error.
  v.append(_escapeTags(tags));

  _nodeBulkInsert->insert(v);

  _nodesInsertElapsed += Tgs::Time::getTime() - start;

  if (_nodeBulkInsert->getPendingCount() >= _nodesPerBulkInsert)
    _nodeBulkInsert->flush();

  ConstNodePtr envelopeNode = std::make_shared<Node>(Status::Unknown1, id, lon, lat, 0.0);
  _updateChangesetEnvelope(envelopeNode);

  LOG_TRACE("Inserted node: " << ElementId(ElementType::Node, id));
  LOG_VART(QString::number(lat, 'g', _precision))
  LOG_VART(QString::number(lon, 'g', _precision));

  //  Update the max node id
  _maxInsertNodeId = max(_maxInsertNodeId, id);

  return true;
}

bool HootApiDb::insertRelation(const Tags &tags, long& assignedId, long version)
{
  assignedId = _getNextRelationId();

  return insertRelation(assignedId, tags, version);
}

bool HootApiDb::insertRelation(const long relationId, const Tags &tags, long version)
{
  LOG_TRACE("Inserting relation: " << relationId << "...");

  const long mapId = _currMapId;
  _checkLastMapId(mapId);

  if (_relationBulkInsert == nullptr)
  {
    QStringList columns({"id", "changeset_id", "timestamp", "version", "tags"});

    _relationBulkInsert =
      std::make_shared<SqlBulkInsert>(_db, getCurrentRelationsTableName(mapId), columns, _ignoreInsertConflicts);
  }

  QList<QVariant> v;
  v.append((qlonglong)relationId);
  v.append((qlonglong)_currChangesetId);
  v.append(DateTimeUtils::currentTimeAsString());
  if (version == 0)
    v.append((qlonglong)1);
  else
    v.append((qlonglong)version);
  // escaping tags ensures that we won't introduce a SQL injection vulnerability, however, if a
  // bad tag is passed and it isn't escaped properly (shouldn't happen) it may result in a syntax
  // error.
  v.append(_escapeTags(tags));

  _relationBulkInsert->insert(v);

  _lazyFlushBulkInsert();

  LOG_TRACE("Inserted relation: " << ElementId(ElementType::Relation, relationId));

  //  Update the max relation id
  _maxInsertRelationId = max(_maxInsertRelationId, relationId);

  return true;
}

bool HootApiDb::insertRelationMember(const long relationId, const ElementType& type,
                                     const long elementId, const QString& role, const int sequenceId)
{
  LOG_TRACE("Inserting relation member for relation: " << relationId << "...");

  const long mapId = _currMapId;
  _checkLastMapId(mapId);

  if (_insertRelationMembers == nullptr)
  {
    QString sql = QString("INSERT INTO %1 (relation_id, member_type, member_id, member_role, sequence_id) "
                          "VALUES (:relation_id, :member_type, :member_id, :member_role, :sequence_id)")
                      .arg(getCurrentRelationMembersTableName(mapId));
    //  ON CONFLICT DO NOTHING simply ignores each inserted row that violates an
    //  arbiter constraint or index
    if (_ignoreInsertConflicts)
      sql.append(" ON CONFLICT DO NOTHING");

    _insertRelationMembers = std::make_shared<QSqlQuery>(_db);
    _insertRelationMembers->prepare(sql);
  }

  _insertRelationMembers->bindValue(":relation_id", (qlonglong)relationId);
  _insertRelationMembers->bindValue(":member_type", type.toString().toLower());
  _insertRelationMembers->bindValue(":member_id", (qlonglong)elementId);
  _insertRelationMembers->bindValue(":member_role", role);
  _insertRelationMembers->bindValue(":sequence_id", sequenceId);

  if (!_insertRelationMembers->exec())
    throw HootException(QString("Error inserting relation member: %1").arg(_insertRelationMembers->lastError().text()));

  return true;
}

long HootApiDb::getOrCreateUser(QString email, QString displayName, bool admin)
{
  long result = getUserId(email, false);

  if (result == -1)
  {
    result = insertUser(email, displayName);
    if (admin)
    {
      if (!_setUserAsAdmin)
      {
        _setUserAsAdmin = std::make_shared<QSqlQuery>(_db);
        _setUserAsAdmin->prepare(
          QString("UPDATE %1 SET privileges = privileges || '\"admin\"=>\"true\"' :: hstore"
                  " WHERE id=:id;").arg(getUsersTableName()));
      }
      //  Add the "admin => true" value to the HSTORE
      _setUserAsAdmin->bindValue(":id", (qlonglong)result);

      if (!_setUserAsAdmin->exec())
        throw HootException(QString("Error setting user as admin: %1").arg(_setUserAsAdmin->lastError().text()));
    }
  }

  return result;
}

void HootApiDb::setUserId(const long sessionUserId)
{
  _currUserId = sessionUserId;

  LOG_DEBUG("User ID updated to " + QString::number(_currUserId));
}

void HootApiDb::setMapId(const long sessionMapId)
{
  _currMapId = sessionMapId;
  assert(_currMapId > 0);

  LOG_DEBUG("Map ID updated to " + QString::number(_currMapId));
}

long HootApiDb::_insertRecord(QSqlQuery& query) const
{
  if (query.exec() == false)
  {
    QString err = QString("Error executing query: %1 (%2)").arg(query.executedQuery(), query.lastError().text());
    LOG_ERROR(err);
    throw HootException(err);
  }
  bool ok = false;
  long id = -1;
  if (query.next())
    id = query.value(0).toLongLong(&ok);

  if (!ok || id == -1)
  {
    LOG_ERROR("query bound values: ");
    LOG_ERROR(query.boundValues());
    LOG_ERROR("\n");
    throw HootException(QString("Error retrieving new ID %1 Query: %2").arg(query.lastError().text(), query.executedQuery()));
  }

  query.finish();

  return id;
}

bool HootApiDb::isSupported(const QUrl& url) const
{
  bool valid = ApiDb::isSupported(url);

  //postgresql is deprecated but still supported
  if (url.scheme() != MetadataTags::HootApiDbScheme() && url.scheme() != "postgresql")
    valid = false;

  if (valid)
  {
    QString path = url.path();
    QStringList plist = path.split("/");

    if (plist.size() == 3)
    {
      if (plist[1] == "")
      {
        LOG_WARN("Looks like a DB path, but a DB name was expected. E.g. "
                 << MetadataTags::HootApiDbScheme() << "://myhost:5432/mydb/mylayer");
        valid = false;
      }
      else if (plist[2] == "")
      {
        LOG_WARN("Looks like a DB path, but a layer name was expected. E.g. "
                 << MetadataTags::HootApiDbScheme() << "://myhost:5432/mydb/mylayer");
        valid = false;
      }
    }
    else if ((plist.size() == 4) && ((plist[1] == "") || (plist[2 ] == "") || (plist[3] == "")))
    {
      LOG_WARN("Looks like a DB path, but a valid DB name, layer, and element was expected. E.g. "
               << MetadataTags::HootApiDbScheme() << "://myhost:5432/mydb/mylayer/1");
      valid = false;
    }
    // need this for a base db connection; like used by db-list command
    else if (plist.size() == 2)
    {
      if (plist[1] == "")
      {
        LOG_WARN("Looks like a DB path, but a DB name was expected. E.g. "
                 << MetadataTags::HootApiDbScheme() << "://myhost:5432/mydb");
        valid = false;
      }
    }
    else
      valid = false;
  }
  return valid;
}

void HootApiDb::_lazyFlushBulkInsert() const
{
  if ((_nodeBulkInsert && _nodeBulkInsert->getPendingCount() > _nodesPerBulkInsert) ||
      (_wayNodeBulkInsert && _wayNodeBulkInsert->getPendingCount() > _wayNodesPerBulkInsert) ||
      (_wayBulkInsert && _wayBulkInsert->getPendingCount() > _waysPerBulkInsert) ||
      (_relationBulkInsert && _relationBulkInsert->getPendingCount() > _relationsPerBulkInsert))
    _flushBulkInserts();
}

void HootApiDb::open(const QUrl& url)
{
  LOG_DEBUG("Opening database connection: " << url.toString(QUrl::RemoveUserInfo) << "...");

  if (!isSupported(url))
    throw HootException(QString("An unsupported URL was passed into HootApiDb: %1").arg(url.toString(QUrl::RemoveUserInfo)));

  _resetQueries();

  ApiDb::open(url);

  if (isCorrectHootDbVersion() == false)
  {
    const QString msg = "Running against an unexpected Hootenanny DB version.";
    LOG_DEBUG("Expected: " << expectedHootDbVersion());
    LOG_DEBUG("Actual: " << getHootDbVersion());
    throw HootException(msg);
  }
}

void HootApiDb::_resetQueries()
{
  LOG_TRACE("Resetting queries...");

  ApiDb::_resetQueries();

  _closeChangeSet.reset();
  _insertChangeSet.reset();
  _insertChangeSetTag.reset();
  _insertMap.reset();
  _insertWayNodes.reset();
  _insertRelationMembers.reset();
  _selectHootDbVersion.reset();
  _insertUser.reset();
  _mapExistsById.reset();
  _changesetExists.reset();
  _selectReserveNodeIds.reset();
  _selectNodeIdsForWay.reset();
  _selectMapIdsForCurrentUser.reset();
  _selectPublicMapIds.reset();
  _selectPublicMapNames.reset();
  _selectMapNamesOwnedByCurrentUser.reset();
  _selectMembersForRelation.reset();
  _updateNode.reset();
  _updateWay.reset();
  _getMapIdByName.reset();
  _insertChangeSet2.reset();
  _numChangesets.reset();
  _getSessionIdByUserId.reset();
  _accessTokensAreValid.reset();
  _getAccessTokenByUserId.reset();
  _getAccessTokenSecretByUserId.reset();
  _insertUserSession.reset();
  _updateUserAccessTokens.reset();
  _insertFolder.reset();
  _insertFolderMapMapping.reset();
  _folderIdsAssociatedWithMap.reset();
  _deleteFolders.reset();
  _getMapPermissionsById.reset();
  _getMapPermissionsByName.reset();
  _getMapIdByNameForCurrentUser.reset();
  _updateJobStatusResourceId.reset();
  _insertJob.reset();
  _deleteJobById.reset();
  _getJobStatusResourceId.reset();

  // bulk insert objects.
  _nodeBulkInsert.reset();
  _nodeIdReserver.reset();
  _relationBulkInsert.reset();
  _relationIdReserver.reset();
  _wayNodeBulkInsert.reset();
  _wayBulkInsert.reset();
  _wayIdReserver.reset();

  _updateIdSequence.reset();
  _setUserAsAdmin.reset();
  _isUserAdmin.reset();
}

long HootApiDb::getMapIdFromUrl(const QUrl& url)
{
  LOG_TRACE("Retrieving map ID from url: " << url);
  LOG_VART(_currUserId);

  const QStringList urlParts = url.path().split("/");
  bool ok;
  long mapId = urlParts[urlParts.size() - 1].toLong(&ok);
  LOG_VART(ok);
  LOG_VART(mapId);

  // If the ID was a valid number, treat it like an ID first.
  ok = mapExists(mapId);

  // If a map with the parsed ID doesn't exist, let's try it as a map name.
  if (!ok)
  {
    // get all map ids with name
    const QString mapName = urlParts[urlParts.size() - 1];
    LOG_VART(mapName);

    mapId = selectMapIdForCurrentUser(mapName);
    LOG_VART(mapId);

    // Here, we don't handle the situation where multiple maps with with the same name are owned
    // by the same user.
    const QString countMismatchErrorMsg = "Expected 1 map with the name: '%1' but found %2 maps.";
    if (mapId == -1)
    {
      // try for public maps
      const std::set<long> mapIds = selectPublicMapIds(mapName);
      LOG_VART(mapIds);
      if (mapIds.size() > 1)
        throw HootException(QString(countMismatchErrorMsg).arg(mapName).arg(mapIds.size()));
      else
      {
        mapId = *mapIds.begin();
        LOG_VART(mapId);
      }
    }
  }

  return mapId;
}

void HootApiDb::verifyCurrentUserMapUse(const long mapId, const bool write)
{
  LOG_VART(mapId);
  LOG_VART(write);

  if (!mapExists(mapId))
    throw HootException("No map exists with requested ID: " + QString::number(mapId));
  else if (!currentUserCanAccessMap(mapId, write))
  {
    QString errorMsg;
    QString accessType = "read";
    if (write)
      accessType = "write";
    if (_currUserId != -1)
    {
      errorMsg =
        QString("User with ID: %1 does not have %2 access to map with ID: %3")
          .arg(QString::number(_currUserId), accessType, QString::number(mapId));
    }
    else
    {
      errorMsg =
        QString("Requested map with ID: %1 not available for public %2 access.")
          .arg(QString::number(mapId), accessType);
    }
    throw HootException(errorMsg);
  }
}

bool HootApiDb::currentUserCanAccessMap(const long mapId, const bool write)
{
  LOG_VART(mapId);
  LOG_VART(_currUserId);
  LOG_VART(write);

  if (_getMapPermissionsById == nullptr)
  {
    _getMapPermissionsById = std::make_shared<QSqlQuery>(_db);
    const QString sql =
      QString("SELECT m.user_id, f.public from %1 m "
              "LEFT JOIN %2 fmm ON (fmm.map_id = m.id) "
              "LEFT JOIN %3 f ON (f.id = fmm.folder_id) "
              "WHERE m.id = :mapId")
        .arg(getMapsTableName(), getFolderMapMappingsTableName(), getFoldersTableName());
    LOG_VART(sql);
    _getMapPermissionsById->prepare(sql);
  }
  _getMapPermissionsById->bindValue(":mapId", (qlonglong)mapId);
  if (!_getMapPermissionsById->exec())
    throw HootException(_getMapPermissionsById->lastError().text());

  long userId = -1;
  bool isPublic = false;
  bool isAdmin = false;
  if (_getMapPermissionsById->next())
  {
    bool ok;
    userId = _getMapPermissionsById->value(0).toLongLong(&ok);
    LOG_VART(userId);
    if (!ok)
      throw HootException(_getMapPermissionsById->lastError().text());
    isPublic = _getMapPermissionsById->value(1).toBool();
    LOG_VART(isPublic);
  }
  //  Only query for admin permissions if the map isn't owned by the current user
  if (_currUserId != userId)
  {
    //  Create the admin query if it doesn't exist
    if (!_isUserAdmin)
    {
      _isUserAdmin = std::make_shared<QSqlQuery>(_db);
      _isUserAdmin->prepare("SELECT (u.privileges -> 'admin')::boolean AS is_admin FROM users u WHERE id=:id;");
    }
    //  Query for the admin privileges of the current user
    _isUserAdmin->bindValue(":id", (qlonglong)_currUserId);
    if (!_isUserAdmin->exec())
      throw HootException(_isUserAdmin->lastError().text());
    else if (_isUserAdmin->next())
      isAdmin = _isUserAdmin->value(0).toBool();
    _isUserAdmin->finish();
  }
  _getMapPermissionsById->finish();

  if (write)
    return _currUserId == userId || isAdmin;
  else
    return isPublic || _currUserId == userId || isAdmin;
}

set<long> HootApiDb::selectPublicMapIds(QString name)
{
  set<long> result;
  LOG_VART(name);

  if (_selectPublicMapIds == nullptr)
  {
    _selectPublicMapIds = std::make_shared<QSqlQuery>(_db);
    const QString sql =
      QString("SELECT m.id, f.public from %1 m "
              "LEFT JOIN %2 fmm ON (fmm.map_id = m.id) "
              "LEFT JOIN %3 f ON (f.id = fmm.folder_id) "
              "WHERE m.display_name = :mapName AND f.public = TRUE")
        .arg(getMapsTableName(), getFolderMapMappingsTableName(), getFoldersTableName());
    LOG_VART(sql);
    _selectPublicMapIds->prepare(sql);
  }
  _selectPublicMapIds->bindValue(":mapName", name);
  LOG_VART(_selectPublicMapIds->lastQuery());

  if (_selectPublicMapIds->exec() == false)
    throw HootException(_selectPublicMapIds->lastError().text());

  while (_selectPublicMapIds->next())
  {
    bool ok;
    long id = _selectPublicMapIds->value(0).toLongLong(&ok);
    if (!ok)
      throw HootException("Error selecting map public IDs.");
    result.insert(id);
  }

  return result;
}

QStringList HootApiDb::selectMapNamesAvailableToCurrentUser()
{
  QStringList result;
  result.append(selectMapNamesOwnedByCurrentUser());
  result.append(selectPublicMapNames());
  result.removeDuplicates();
  result.sort();
  return result;
}

QStringList HootApiDb::selectMapNamesOwnedByCurrentUser()
{
  QStringList result;

  LOG_VART(_currUserId);

  if (_selectMapNamesOwnedByCurrentUser == nullptr)
  {
    _selectMapNamesOwnedByCurrentUser = std::make_shared<QSqlQuery>(_db);
    _selectMapNamesOwnedByCurrentUser->prepare(
      QString("SELECT display_name FROM %1 WHERE user_id = :user_id").arg(getMapsTableName()));
  }
  _selectMapNamesOwnedByCurrentUser->bindValue(":user_id", (qlonglong)_currUserId);
  LOG_VART(_selectMapNamesOwnedByCurrentUser->lastQuery());

  if (_selectMapNamesOwnedByCurrentUser->exec() == false)
    throw HootException(_selectMapNamesOwnedByCurrentUser->lastError().text());

  while (_selectMapNamesOwnedByCurrentUser->next())
    result.append(_selectMapNamesOwnedByCurrentUser->value(0).toString());

  LOG_VART(result.size());

  return result;
}

QStringList HootApiDb::selectPublicMapNames()
{
  QStringList result;

  if (_selectPublicMapNames == nullptr)
  {
    _selectPublicMapNames = std::make_shared<QSqlQuery>(_db);
    const QString sql =
      QString("SELECT m.display_name from %1 m "
              "LEFT JOIN %2 fmm ON (fmm.map_id = m.id) "
              "LEFT JOIN %3 f ON (f.id = fmm.folder_id) "
              "WHERE f.public = TRUE")
        .arg(getMapsTableName(), getFolderMapMappingsTableName(), getFoldersTableName());
    LOG_VART(sql);
    _selectPublicMapNames->prepare(sql);
  }
  LOG_VART(_selectPublicMapNames->lastQuery());

  if (_selectPublicMapNames->exec() == false)
    throw HootException(_selectPublicMapNames->lastError().text());

  while (_selectPublicMapNames->next())
    result.append(_selectPublicMapNames->value(0).toString());

  LOG_VART(result.size());

  return result;
}

long HootApiDb::selectMapIdForCurrentUser(QString name)
{
  long result;

  LOG_VART(name);
  LOG_VART(_currUserId);

  if (_selectMapIdsForCurrentUser == nullptr)
  {
    _selectMapIdsForCurrentUser = std::make_shared<QSqlQuery>(_db);
    _selectMapIdsForCurrentUser->prepare(
      QString("SELECT id FROM %1 WHERE display_name = :name AND user_id = :user_id").arg(getMapsTableName()));
  }
  _selectMapIdsForCurrentUser->bindValue(":user_id", (qlonglong)_currUserId);

  _selectMapIdsForCurrentUser->bindValue(":name", name);
  LOG_VART(_selectMapIdsForCurrentUser->lastQuery());

  if (_selectMapIdsForCurrentUser->exec() == false)
    throw HootException(_selectMapIdsForCurrentUser->lastError().text());

  // There should only be one map owned by a user with a given name, since that's all
  // HootApiDbWriter allows.
  if (_selectMapIdsForCurrentUser->next())
  {
    bool ok;
    long id = _selectMapIdsForCurrentUser->value(0).toLongLong(&ok);
    if (!ok)
      throw HootException("Error selecting map IDs.");
    result = id;
  }
  else
    result = -1;

  return result;
}

set<long> HootApiDb::getFolderIdsAssociatedWithMap(const long mapId)
{
  set<long> result;

  if (_folderIdsAssociatedWithMap == nullptr)
  {
    _folderIdsAssociatedWithMap = std::make_shared<QSqlQuery>(_db);
    _folderIdsAssociatedWithMap->prepare(
      QString("SELECT folder_id FROM %1 WHERE map_id = :mapId").arg(getFolderMapMappingsTableName()));
  }
  _folderIdsAssociatedWithMap->bindValue(":mapId", (qlonglong)mapId);

  if (_folderIdsAssociatedWithMap->exec() == false)
    throw HootException(_folderIdsAssociatedWithMap->lastError().text());

  while (_folderIdsAssociatedWithMap->next())
  {
    bool ok;
    long id = _folderIdsAssociatedWithMap->value(0).toLongLong(&ok);
    if (!ok)
      throw HootException("Error selecting map IDs.");
    result.insert(id);
  }

  return result;
}

void HootApiDb::_deleteFolderMapMappingsByMapId(const long mapId) const
{
  _exec(QString("DELETE FROM %1 WHERE map_id = :id").arg(getFolderMapMappingsTableName()), (qlonglong)mapId);
}

void HootApiDb::_deleteAllFolders(const set<long>& folderIds)
{
  if (folderIds.empty())
    return;

  if (_deleteFolders == nullptr)
    _deleteFolders = std::make_shared<QSqlQuery>(_db);

  QString sql = QString("DELETE FROM %1 WHERE id IN (").arg(getFoldersTableName());
  for (auto id : folderIds)
    sql += QString::number(id) + ",";
  sql.chop(1);
  sql += ")";
  LOG_VART(sql);

  if (!_deleteFolders->exec(sql))
  {
    LOG_VART(_deleteFolders->executedQuery());
    LOG_VART(_deleteFolders->lastError().databaseText());
    LOG_VART(_deleteFolders->lastError().number());
    LOG_VART(_deleteFolders->lastError().driverText());
    throw HootException(QString("Error deleting folders. %1").arg(_deleteFolders->lastError().text()));
  }
  _deleteFolders->finish();
}

long HootApiDb::insertFolder(const QString& displayName, const long parentId, const long userId,
                             const bool isPublic)
{
  if (_insertFolder == nullptr)
  {
    _insertFolder = std::make_shared<QSqlQuery>(_db);
    _insertFolder->prepare(
      QString("INSERT INTO %1 (display_name, parent_id, user_id, public, created_at)"
              " VALUES (:displayName, :parentId, :userId, :public, NOW())"
              " RETURNING id")
        .arg(getFoldersTableName()));
  }
  _insertFolder->bindValue(":displayName", displayName);
  _insertFolder->bindValue(":userId", (qlonglong)userId);
  _insertFolder->bindValue(":public", isPublic);
  if (parentId != -1) 
    _insertFolder->bindValue(":parentId", (qlonglong)parentId);

  return _insertRecord(*_insertFolder);
}

void HootApiDb::insertFolderMapMapping(const long mapId, const long folderId)
{
  if (_insertFolderMapMapping == nullptr)
  {
    _insertFolderMapMapping = std::make_shared<QSqlQuery>(_db);
    _insertFolderMapMapping->prepare(
      QString("INSERT INTO %1 (map_id, folder_id) VALUES (:mapId, :folderId)").arg(getFolderMapMappingsTableName()));
  }
  _insertFolderMapMapping->bindValue(":mapId", (qlonglong)mapId);
  _insertFolderMapMapping->bindValue(":folderId", (qlonglong)folderId);
  if (!_insertFolderMapMapping->exec())
  {
    LOG_VART(_insertFolderMapMapping->executedQuery());
    LOG_VART(_insertFolderMapMapping->lastError().databaseText());
    LOG_VART(_insertFolderMapMapping->lastError().number());
    LOG_VART(_insertFolderMapMapping->lastError().driverText());
    throw HootException(
      QString("Error inserting folder mapping for folder ID: %1 and map ID: %2 %3")
        .arg(QString::number(folderId), QString::number(mapId), _insertFolderMapMapping->lastError().text()));
  }
}

QString HootApiDb::tableTypeToTableName(const TableType& tableType) const
{
  if (tableType == TableType::Node)
    return getCurrentNodesTableName(_currMapId);
  else if (tableType == TableType::Way)
    return getCurrentWaysTableName(_currMapId);
  else if (tableType == TableType::Relation)
    return getCurrentRelationsTableName(_currMapId);
  else if (tableType == TableType::WayNode)
    return getCurrentWayNodesTableName(_currMapId);
  else if (tableType == TableType::RelationMember)
    return getCurrentRelationMembersTableName(_currMapId);
  else
    throw HootException("Unsupported table type.");
}

bool HootApiDb::mapExists(const long id)
{
  if (_mapExistsById == nullptr)
  {
    _mapExistsById = std::make_shared<QSqlQuery>(_db);
    _mapExistsById->prepare(QString("SELECT display_name FROM %1 WHERE id = :mapId").arg(getMapsTableName()));
  }
  _mapExistsById->bindValue(":mapId", (qlonglong)id);
  if (_mapExistsById->exec() == false)
    throw HootException(_mapExistsById->lastError().text());

  return _mapExistsById->next();
}

long HootApiDb::getMapIdByName(const QString& name)
{
  //assuming unique name here
  if (_getMapIdByName == nullptr)
  {
    _getMapIdByName = std::make_shared<QSqlQuery>(_db);
    _getMapIdByName->prepare(
      QString("SELECT id FROM %1 WHERE display_name = :mapName").arg(getMapsTableName()));
  }
  _getMapIdByName->bindValue(":mapName", name);
  if (_getMapIdByName->exec() == false)
    throw HootException(_getMapIdByName->lastError().text());

  long result = -1;
  if (_getMapIdByName->next())
  {
    bool ok;
    result = _getMapIdByName->value(0).toLongLong(&ok);
    if (!ok)
      throw HootException(_getMapIdByName->lastError().text());
  }
  _getMapIdByName->finish();
  return result;
}

long HootApiDb::getMapIdByNameForCurrentUser(const QString& name)
{
  LOG_VARD(_currUserId);

  //assuming unique name here
  if (_getMapIdByNameForCurrentUser == nullptr)
  {
    _getMapIdByNameForCurrentUser = std::make_shared<QSqlQuery>(_db);
    _getMapIdByNameForCurrentUser->prepare(
      QString("SELECT id FROM %1 WHERE display_name = :mapName AND user_id = :userId").arg(getMapsTableName()));
  }
  _getMapIdByNameForCurrentUser->bindValue(":mapName", name);
  _getMapIdByNameForCurrentUser->bindValue(":userId", (qlonglong)_currUserId);
  if (_getMapIdByNameForCurrentUser->exec() == false)
    throw HootException(_getMapIdByNameForCurrentUser->lastError().text());

  long result = -1;
  if (_getMapIdByNameForCurrentUser->next())
  {
    bool ok;
    result = _getMapIdByNameForCurrentUser->value(0).toLongLong(&ok);
    if (!ok)
      throw HootException(_getMapIdByNameForCurrentUser->lastError().text());
  }
  _getMapIdByNameForCurrentUser->finish();
  LOG_VARD(result);
  return result;
}

long HootApiDb::numChangesets()
{
  if (!_numChangesets)
  {
    _numChangesets = std::make_shared<QSqlQuery>(_db);
    _numChangesets->prepare(QString("SELECT COUNT(*) FROM %1").arg(getChangesetsTableName(_currMapId)));
  }
  LOG_VART(_numChangesets->lastQuery());

  if (!_numChangesets->exec())
  {
    LOG_ERROR(_numChangesets->executedQuery());
    LOG_ERROR(_numChangesets->lastError().text());
    throw HootException(_numChangesets->lastError().text());
  }

  long result = -1;
  if (_numChangesets->next())
  {
    bool ok;
    result = _numChangesets->value(0).toLongLong(&ok);
    if (!ok)
      throw HootException(QString("Could not get changeset count for map with ID: %1").arg(QString::number(_currMapId)));
  }
  _numChangesets->finish();
  return result;
}

bool HootApiDb::changesetExists(const long id)
{
  LOG_TRACE("Checking changeset with ID: " << id << " exists...");

  const long mapId = _currMapId;

  _checkLastMapId(mapId);
  if (_changesetExists == nullptr)
  {
    _changesetExists = std::make_shared<QSqlQuery>(_db);
    _changesetExists->prepare(QString("SELECT num_changes FROM %1 WHERE id = :changesetId")
      .arg(getChangesetsTableName(mapId)));
  }
  _changesetExists->bindValue(":changesetId", (qlonglong)id);
  LOG_VART(_changesetExists->lastQuery());
  if (_changesetExists->exec() == false)
  {
    LOG_ERROR(_changesetExists->executedQuery());
    LOG_ERROR(_changesetExists->lastError().text());
    throw HootException(_changesetExists->lastError().text());
  }

  return _changesetExists->next();
}

bool HootApiDb::accessTokensAreValid(const QString& userName, const QString& accessToken,
                                     const QString& accessTokenSecret)
{
  LOG_VART(userName);
  LOG_VART(accessToken);

  if (_accessTokensAreValid == nullptr)
  {
    _accessTokensAreValid = std::make_shared<QSqlQuery>(_db);
    _accessTokensAreValid->prepare(
        QString("SELECT COUNT(*) FROM %1 "
                "WHERE display_name = :userName AND "
                "provider_access_key = :accessToken AND "
                "provider_access_token = :accessTokenSecret").arg(ApiDb::getUsersTableName()));
  }
  _accessTokensAreValid->bindValue(":userName", userName);
  _accessTokensAreValid->bindValue(":accessToken", accessToken);
  _accessTokensAreValid->bindValue(":accessTokenSecret", accessTokenSecret);
  if (!_accessTokensAreValid->exec())
    throw HootException(QString("Error validating access tokens for user name: %1 %2").arg(userName, _accessTokensAreValid->lastError().text()));

  long result = -1;
  if (_accessTokensAreValid->next())
  {
    bool ok;
    result = _accessTokensAreValid->value(0).toLongLong(&ok);
    assert(result <= 1);
    if (!ok)
      throw HootException("Error validating access tokens for user name: " + userName);
  }
  _accessTokensAreValid->finish();

  LOG_VART(result);
  return result > 0;
}

QString HootApiDb::getAccessTokenByUserId(const long userId)
{
  LOG_VART(userId);

  QString accessToken = "";

  if (_getAccessTokenByUserId == nullptr)
  {
    _getAccessTokenByUserId = std::make_shared<QSqlQuery>(_db);
    _getAccessTokenByUserId->prepare(
      QString("SELECT provider_access_key FROM %1 WHERE id = :userId").arg(ApiDb::getUsersTableName()));
  }
  _getAccessTokenByUserId->bindValue(":userId", (qlonglong)userId);
  if (!_getAccessTokenByUserId->exec())
    throw HootException(QString("Error finding access token for user ID: %1 %2").arg(QString::number(userId), _getAccessTokenByUserId->lastError().text()));

  //shouldn't be more than one result, but if there is we don't care
  if (_getAccessTokenByUserId->next())
    accessToken = _getAccessTokenByUserId->value(0).toString();
  else
  {
    LOG_DEBUG("No access token available for user ID: " << userId);
    _getAccessTokenByUserId->finish();
    return "";
  }
  _getAccessTokenByUserId->finish();

  LOG_VART(accessToken);
  return accessToken;
}

QString HootApiDb::getAccessTokenSecretByUserId(const long userId)
{
  LOG_VART(userId);

  QString accessTokenSecret = "";

  if (_getAccessTokenSecretByUserId == nullptr)
  {
    _getAccessTokenSecretByUserId = std::make_shared<QSqlQuery>(_db);
    _getAccessTokenSecretByUserId->prepare(
      QString("SELECT provider_access_token FROM %1 WHERE id = :userId").arg(ApiDb::getUsersTableName()));
  }
  _getAccessTokenSecretByUserId->bindValue(":userId", (qlonglong)userId);
  if (!_getAccessTokenSecretByUserId->exec())
  {
    throw HootException(
      QString("Error finding access token secret for user ID: %1 %2")
        .arg(QString::number(userId), _getAccessTokenSecretByUserId->lastError().text()));
  }

  //shouldn't be more than one result, but if there is we don't care
  if (_getAccessTokenSecretByUserId->next())
    accessTokenSecret = _getAccessTokenSecretByUserId->value(0).toString();
  else
  {
    LOG_DEBUG("No access token secret available for user ID: " << userId);
    _getAccessTokenSecretByUserId->finish();
    return "";
  }
  _getAccessTokenSecretByUserId->finish();

  return accessTokenSecret;
}

void HootApiDb::insertUserSession(const long userId, const QString& sessionId)
{
  if (_insertUserSession == nullptr)
  {
    _insertUserSession = std::make_shared<QSqlQuery>(_db);
    _insertUserSession->prepare(
      QString("INSERT INTO %1"
              " (session_id, creation_time, last_access_time, max_inactive_interval, user_id)"
              " VALUES (:sessionId, %2, %3, :maxInactiveInterval, :userId)")
        .arg(getUserSessionTableName(), currentTimestampAsBigIntSql(), currentTimestampAsBigIntSql()));
  }
  _insertUserSession->bindValue(":sessionId", sessionId);
  _insertUserSession->bindValue(":maxInactiveInterval", 1);
  _insertUserSession->bindValue(":userId", (qlonglong)userId);
  if (!_insertUserSession->exec())
  {
    LOG_VART(_insertUserSession->executedQuery());
    LOG_VART(_insertUserSession->lastError().databaseText());
    LOG_VART(_insertUserSession->lastError().number());
    LOG_VART(_insertUserSession->lastError().driverText());
    throw HootException(
      QString("Error inserting session for user ID: %1 %2").arg(QString::number(userId), _insertUserSession->lastError().text()));
  }
}

void HootApiDb::updateUserAccessTokens(const long userId, const QString& accessToken, const QString& accessTokenSecret)
{
  if (_updateUserAccessTokens == nullptr)
  {
    _updateUserAccessTokens = std::make_shared<QSqlQuery>(_db);
    _updateUserAccessTokens->prepare(
      QString("UPDATE %1 SET provider_access_key=:accessToken, provider_access_token=:accessTokenSecret WHERE id=:userId")
        .arg(ApiDb::getUsersTableName()));
  }
  _updateUserAccessTokens->bindValue(":userId", (qlonglong)userId);
  _updateUserAccessTokens->bindValue(":accessToken", accessToken);
  _updateUserAccessTokens->bindValue(":accessTokenSecret", accessTokenSecret);
  if (!_updateUserAccessTokens->exec())
  {
    throw HootException(
      QString("Error updating access tokens for user ID: %1 %2").arg(QString::number(userId), _updateUserAccessTokens->lastError().text()));
  }
}

QString HootApiDb::getSessionIdByUserId(const long userId)
{
  LOG_VART(userId);

  QString sessionId = "";

  if (_getSessionIdByUserId == nullptr)
  {
    _getSessionIdByUserId = std::make_shared<QSqlQuery>(_db);
    _getSessionIdByUserId->prepare(QString("SELECT session_id FROM %1 WHERE user_id = :userId").arg(getUserSessionTableName()));
  }
  _getSessionIdByUserId->bindValue(":userId", (qlonglong)userId);
  if (!_getSessionIdByUserId->exec())
  {
    throw HootException(
      QString("Error finding session ID for user ID: %1 %2").arg(QString::number(userId), _getSessionIdByUserId->lastError().text()));
  }

  //shouldn't be more than one result, but if there is we don't care
  if (_getSessionIdByUserId->next())
    sessionId = _getSessionIdByUserId->value(0).toString();
  else
  {
    LOG_DEBUG("No user session ID available for user ID: " << userId);
    _getSessionIdByUserId->finish();
    return "";
  }
  _getSessionIdByUserId->finish();

  LOG_VART(sessionId);
  return sessionId;
}

QString HootApiDb::getSessionIdByAccessTokens(const QString& userName, const QString& accessToken,
                                              const QString& accessTokenSecret)
{
  QString sessionId = "";

  if (accessTokensAreValid(userName, accessToken, accessTokenSecret))
  {
    const long userId = getUserIdByName(userName);
    //no user with user name found
    if (userId == -1)
      return "";

    //will be an empty string if no session ID was found
    sessionId = getSessionIdByUserId(userId);
  }
  else
    throw HootException("Access tokens for user: " + userName + " are invalid.");

  return sessionId;
}

QString HootApiDb::elementTypeToElementTableName(const ElementType& elementType) const
{
  return tableTypeToTableName(TableType::fromElementType(elementType));
}

vector<long> HootApiDb::selectNodeIdsForWay(long wayId)
{
  const long mapId = _currMapId;
  _checkLastMapId(mapId);
  QString sql = QString("SELECT node_id FROM %1 WHERE way_id = :wayId ORDER BY sequence_id").arg(getCurrentWayNodesTableName(mapId));
  return _selectNodeIdsForWaySql(wayId, sql);
}

vector<RelationData::Entry> HootApiDb::selectMembersForRelation(long relationId)
{
  if (!_selectMembersForRelation)
  {
    _selectMembersForRelation = std::make_shared<QSqlQuery>(_db);
    _selectMembersForRelation->setForwardOnly(true);
    _selectMembersForRelation->prepare(
      QString("SELECT member_type, member_id, member_role FROM %1"
              " WHERE relation_id = :relationId ORDER BY sequence_id").arg(getCurrentRelationMembersTableName(_currMapId)));
  }

  _selectMembersForRelation->bindValue(":relationId", (qlonglong)relationId);

  return ApiDb::_selectRelationMembers(_selectMembersForRelation);
}

void HootApiDb::updateNode(const long id, const double lat, const double lon, const long version,
                           const Tags& tags)
{
  LOG_TRACE("Updating node: " << id << "...");

  const long mapId = _currMapId;
  _flushBulkInserts();

  _checkLastMapId(mapId);

  if (_updateNode == nullptr)
  {
    _updateNode = std::make_shared<QSqlQuery>(_db);
    _updateNode->prepare(
      QString("UPDATE %1"
      " SET latitude=:latitude, longitude=:longitude, changeset_id=:changeset_id, "
      " timestamp=:timestamp, tile=:tile, version=:version, tags=%2"
      " WHERE id=:id").arg(getCurrentNodesTableName(mapId), _escapeTags(tags)));
  }

  _updateNode->bindValue(":id", (qlonglong)id);
  _updateNode->bindValue(":latitude", lat);
  _updateNode->bindValue(":longitude", lon);
  _updateNode->bindValue(":changeset_id", (qlonglong)_currChangesetId);
  _updateNode->bindValue(":timestamp", DateTimeUtils::currentTimeAsString());
  _updateNode->bindValue(":tile", (qlonglong)tileForPoint(lat, lon));
  _updateNode->bindValue(":version", (qlonglong)version);

  if (_updateNode->exec() == false)
    throw HootException(QString("Error executing query: %1 (%2)").arg(_updateNode->executedQuery(), _updateNode->lastError().text()));

  _updateNode->finish();

  LOG_TRACE("Updated node: " << ElementId(ElementType::Node, id));
}

void HootApiDb::updateWay(const long id, const long version, const Tags& tags)
{
  LOG_TRACE("Updating way: " << id << "...");

  const long mapId = _currMapId;
  _flushBulkInserts();
  _checkLastMapId(mapId);

  if (_updateWay == nullptr)
  {
    _updateWay = std::make_shared<QSqlQuery>(_db);
    _updateWay->prepare(
      QString("UPDATE %1"
              " SET changeset_id=:changeset_id, timestamp=:timestamp, version=:version, tags=%2"
              " WHERE id=:id").arg(getCurrentWaysTableName(mapId), _escapeTags(tags)));
  }

  _updateWay->bindValue(":id", (qlonglong)id);
  _updateWay->bindValue(":changeset_id", (qlonglong)_currChangesetId);
  _updateWay->bindValue(":timestamp", DateTimeUtils::currentTimeAsString());
  _updateWay->bindValue(":version", (qlonglong)version);

  if (_updateWay->exec() == false)
    throw HootException(QString("Error executing query: %1 (%2)").arg(_updateWay->executedQuery(), _updateWay->lastError().text()));

  _updateWay->finish();

  LOG_TRACE("Updated way: " << ElementId(ElementType::Way, id));
}

bool HootApiDb::insertWay(const Tags &tags, long &assignedId, long version)
{
  assignedId = _getNextWayId();

  return insertWay(assignedId, tags, version);
}

bool HootApiDb::insertWay(const long wayId, const Tags &tags, long version)
{
  LOG_TRACE("Inserting way: " << wayId << "...");

  const long mapId = _currMapId;

  double start = Tgs::Time::getTime();

  _checkLastMapId(mapId);

  if (_wayBulkInsert == nullptr)
  {
    QStringList columns({"id", "changeset_id", "timestamp", "version", "tags"});

    _wayBulkInsert =
      std::make_shared<SqlBulkInsert>(
        _db, getCurrentWaysTableName(mapId), columns, _ignoreInsertConflicts);
  }

  QList<QVariant> v;
  v.append((qlonglong)wayId);
  v.append((qlonglong)_currChangesetId);
  v.append(DateTimeUtils::currentTimeAsString());
  if (version == 0)
    v.append((qlonglong)1);
  else
    v.append((qlonglong)version);

  // escaping tags ensures that we won't introduce a SQL injection vulnerability, however, if a
  // bad tag is passed and it isn't escaped properly (shouldn't happen) it may result in a syntax
  // error.
  v.append(_escapeTags(tags));

  _wayBulkInsert->insert(v);

  _wayNodesInsertElapsed += Tgs::Time::getTime() - start;

  _lazyFlushBulkInsert();

  LOG_TRACE("Inserted way: " << ElementId(ElementType::Way, wayId));
  LOG_TRACE(tags);

  //  Update the max way id
  _maxInsertWayId = max(_maxInsertWayId, wayId);

  return true;
}

void HootApiDb::insertWayNodes(long wayId, const vector<long>& nodeIds)
{
  LOG_TRACE("Inserting way nodes for way: " << wayId << "...");

  const long mapId = _currMapId;
  double start = Tgs::Time::getTime();

  LOG_TRACE("Inserting nodes into way " << QString::number(wayId));

  _checkLastMapId(mapId);

  if (_wayNodeBulkInsert == nullptr)
  {
    QStringList columns({"way_id", "node_id", "sequence_id"});

    _wayNodeBulkInsert =
      std::make_shared<SqlBulkInsert>(
        _db, getCurrentWayNodesTableName(mapId), columns, _ignoreInsertConflicts);
  }

  QList<QVariant> v;
  v.append((qlonglong)wayId);
  v.append((qlonglong)0);
  v.append((qlonglong)0);

  for (size_t i = 0; i < nodeIds.size(); ++i)
  {
    v[1] = (qlonglong)nodeIds[i];
    v[2] = (qlonglong)i;
    _wayNodeBulkInsert->insert(v);
  }

  _wayNodesInsertElapsed += Tgs::Time::getTime() - start;

  _lazyFlushBulkInsert();
}

void HootApiDb::incrementChangesetChangeCount()
{
  _changesetChangeCount++;

  // If we've hit maximum count of changes for a changeset, close this one out and start a new one
  if ( _changesetChangeCount >= maximumChangeSetEdits())
  {
    endChangeset();
    beginChangeset();
  }
}

void HootApiDb::_updateChangesetEnvelope(const ConstNodePtr node)
{
  const double nodeX = node->getX();
  const double nodeY = node->getY();

  _changesetEnvelope.expandToInclude(nodeX, nodeY);
  LOG_TRACE(
    "Changeset bounding box updated to include X=" + QString::number(nodeX) + ", Y=" +
    QString::number(nodeY));
}

long HootApiDb::reserveElementId(const ElementType::Type type)
{
  long retVal = -1;

  switch (type)
  {
  case ElementType::Node:
    retVal = _getNextNodeId();
    break;
  case ElementType::Way:
    retVal = _getNextWayId();
    break;
  case ElementType::Relation:
    retVal = _getNextRelationId();
    break;
  default:
    LOG_ERROR("Requested element ID for unknown element type");
    throw HootException("reserveElementId called with unknown type");
    break;
  }

  return retVal;
}

QUrl HootApiDb::getBaseUrl()
{
  // read the DB values from the DB config file.
  Settings s = readDbConfig();
  QUrl result;
  result.setScheme(MetadataTags::HootApiDbScheme());
  result.setHost(s.get("DB_HOST").toString());
  result.setPort(s.get("DB_PORT").toInt());
  result.setUserName(s.get("DB_USER").toString());
  result.setPassword(s.get("DB_PASSWORD").toString());
  result.setPath("/" + s.get("DB_NAME").toString());
  return result;
}

QString HootApiDb::getTableName(const QString& url)
{
  const QStringList urlParts =  url.split("/");
  return urlParts[urlParts.size() - 1];
}

QString HootApiDb::removeTableName(const QString& url)
{
  QStringList urlParts =  url.split("/");
  QString modifiedUrl;
  for (int i = 0; i < urlParts.size() - 1; i++)
    modifiedUrl += urlParts[i] + "/";

  modifiedUrl.chop(1);
  return modifiedUrl;
}

void HootApiDb::updateJobStatusResourceId(const QString& jobId, const long resourceId)
{
  LOG_VARD(jobId);
  LOG_VARD(resourceId);

  if (_updateJobStatusResourceId == nullptr)
  {
    _updateJobStatusResourceId = std::make_shared<QSqlQuery>(_db);
    _updateJobStatusResourceId->prepare(
      QString("UPDATE %1 SET resource_id = :resourceId WHERE job_id = :jobId").arg(getJobStatusTableName()));
  }
  _updateJobStatusResourceId->bindValue(":jobId", jobId);
  _updateJobStatusResourceId->bindValue(":resourceId", (qlonglong)resourceId);
  if (!_updateJobStatusResourceId->exec())
  {
    const QString err =
      QString("Error executing query: %1 (%2)")
        .arg(_updateJobStatusResourceId->executedQuery(), _updateJobStatusResourceId->lastError().text());
    throw HootException(err);
  }
  _updateJobStatusResourceId->finish();
}

QString HootApiDb::insertJob(const QString& statusDetail)
{
  if (_insertJob == nullptr)
  {
    _insertJob = std::make_shared<QSqlQuery>(_db);
    _insertJob->prepare(
      QString("INSERT INTO %1 (job_id, start, status, percent_complete, status_detail) "
      "VALUES (:jobId, NOW(), :status, :percentComplete, :statusDetail)").arg(getJobStatusTableName()));
  }
  const QString jobId = UuidHelper::createUuid().toString();
  _insertJob->bindValue(":jobId", jobId);
  _insertJob->bindValue(":status", (int)ServicesJobStatus::Running);
  _insertJob->bindValue(":percentComplete", 0);
  _insertJob->bindValue(":statusDetail", statusDetail);
  if (!_insertJob->exec())
  {
    const QString err =
      QString("Error executing query: %1 (%2)")
        .arg(_insertJob->executedQuery(), _insertJob->lastError().text());
    throw HootException(err);
  }
  _insertJob->finish();
  LOG_VARD(jobId);
  return jobId;
}

long HootApiDb::getJobStatusResourceId(const QString& jobId)
{
  LOG_VARD(jobId);
  if (_getJobStatusResourceId == nullptr)
  {
    _getJobStatusResourceId = std::make_shared<QSqlQuery>(_db);
    _getJobStatusResourceId->prepare(QString("SELECT resource_id FROM %1 WHERE job_id = :jobId").arg(getJobStatusTableName()));
  }
  _getJobStatusResourceId->bindValue(":jobId", jobId);

  if (!_getJobStatusResourceId->exec())
  {
    const QString err =
      QString("Error executing query: %1 (%2)")
        .arg(_getJobStatusResourceId->executedQuery(), _getJobStatusResourceId->lastError().text());
    throw HootException(err);
  }

  long resourceId = -1;
  bool ok = false;
  if (_getJobStatusResourceId->next())
    resourceId = _getJobStatusResourceId->value(0).toLongLong(&ok);
  LOG_VARD(resourceId);

  if (!ok)
  {
    LOG_DEBUG("No resource ID available for job ID: " << jobId);
  }
  _getJobStatusResourceId->finish();
  return resourceId;
}

void HootApiDb::_deleteJob(const QString& id)
{
  if (_deleteJobById == nullptr)
  {
    _deleteJobById = std::make_shared<QSqlQuery>(_db);
    _deleteJobById->prepare(QString("DELETE FROM %1 WHERE job_id = :jobId").arg(getJobStatusTableName()));
  }
  _deleteJobById->bindValue(":jobId", id);
  if (!_deleteJobById->exec())
  {
    const QString err =
      QString("Error executing query: %1 (%2)")
        .arg(_deleteJobById->executedQuery(), _deleteJobById->lastError().text());
    throw HootException(err);
  }
  _deleteJobById->finish();
}

void HootApiDb::updateImportSequences()
{
  if (_maxInsertNodeId > 0)
    _updateImportSequence(_maxInsertNodeId, getCurrentNodesSequenceName(_currMapId));
  if (_maxInsertWayId > 0)
    _updateImportSequence(_maxInsertWayId, getCurrentWaysSequenceName(_currMapId));
  if (_maxInsertRelationId > 0)
    _updateImportSequence(_maxInsertRelationId, getCurrentRelationsSequenceName(_currMapId));
}

void HootApiDb::_updateImportSequence(long max, const QString& sequence)
{
  LOG_TRACE("Updating sequence " << sequence);
  if (!_updateIdSequence)
    _updateIdSequence = std::make_shared<QSqlQuery>(_db);
  if (!_updateIdSequence->exec(QString("ALTER SEQUENCE %1 RESTART %2").arg(sequence, QString::number(max + 1))))
  {
    const QString err =
      QString("Error executing query: %1 (%2)")
        .arg(_updateIdSequence->executedQuery(), _updateIdSequence->lastError().text());
    LOG_TRACE(err);
    throw HootException(err);
  }
  _updateIdSequence->finish();
}

}
