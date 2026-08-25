#pragma once

#include "EntityManager.h"


/**
 * Constructor for the EntityManager,
 * that initializes an Empty-Archetype
 * and adds it as first existing Archetype.
 */
EntityManager::EntityManager()
{
	Archetype emptyArchetype;
	emptyArchetype.id = EMPTY_ARCHETYPE_ID;  // is set in Usings as constant
	emptyArchetype.type = {};  // no components in vector

	archetypes.push_back(std::move(emptyArchetype));
	archetypeIndex.emplace(Type{}, EMPTY_ARCHETYPE_ID);
}


/**
 * Adds a new Entity to the existing ones
 * by either recycling old freed records 
 * or creating more and returns the 
 * created unique EntityId afterward.
 *
 * @return The newly created unique EntityId of the added entity.
 */
EntityId EntityManager::AddEntity()
{
	EntityId id;

	if (!freedIDs.Empty())
	{
		id = freedIDs.Pop();
	}
	else
	{
		id = nextID++;
		// could potentially resize for more than item at once to save performance
		entityRecords.resize(nextID);  // resize for one more item (nextID is already currentID+1)
	}

	EntityRecord& record = entityRecords[id];  // create entity record
	record.alive = true;  // and set it alive

	// Put the new entity into the empty archetype: []
	Archetype& emptyArchetype = archetypes[EMPTY_ARCHETYPE_ID];

	record.archetype = EMPTY_ARCHETYPE_ID;
	record.row = emptyArchetype.entities.size();

	emptyArchetype.entities.push_back(id);

	return id;
}

/**
 * Removes the given Entity from existence,
 * but recycles the record for later use 
 * in an Empty-archetype.
 *
 * @param id ID of the Entity to be removed from existence.
 * @return If the given Entity was existent and could be removed.
 */
bool EntityManager::RemoveEntity(EntityId id)
{
	if (!IsAlive(id))
		return false;

	EntityRecord& record = entityRecords[id];
	Archetype& archetype = archetypes[record.archetype];

	RemoveRowFromArchetype(archetype, record.row);

	record.alive = false;
	record.archetype = EMPTY_ARCHETYPE_ID;
	record.row = 0;

	freedIDs.Push(id);

	return true;
}

/**
 * Removes an Entity and it's Component-data
 * via the given row-index from the given archetype
 * (e.g. due to adding/removing a component).
 *
 * @param archetype Archetype of whom the Entity should be removed.
 * @param row ID of the row where the Entity exists in the archetype.
 */
void EntityManager::RemoveRowFromArchetype(Archetype& archetype, size_t row)
{
	const size_t lastRow = archetype.entities.size() - 1;  // last row index
	const EntityId movedEntity = archetype.entities[lastRow];  // last entity in entity-vector of archetype

	// Remove component data of entities[row] from all component columns
	for (auto& column : archetype.columns)
	{
		column->RemoveSwap(row);  // replace by last item in vector and remove from back
	}

	// Remove entity from entity-vector by replacing it
	if (row != lastRow)  // except if last should be deleted anyway
	{
		archetype.entities[row] = movedEntity;  // move entity at vector-back to position of removed entity
		entityRecords[movedEntity].row = row;  // update entity records
	}

	archetype.entities.pop_back();  // remove back of archetype-entity-vector
}

/**
 * Checks whether an Entity exists and is alive.
 *
 * @param id ID of the Entity to be checked.
 * @return If the given Entity exists and is alive.
 */
bool EntityManager::IsAlive(EntityId id) const
{
	return id < entityRecords.size() && entityRecords[id].alive;
}

/* Use this Class:
 
		entityManager.AddComponent(entity, Transform{});
		entityManager.AddComponent(entity, Physics{});

		Transform* transform = entityManager.GetComponent<Transform>(entity);

		if (entityManager.HasComponent<Physics>(entity))
		{
		    entityManager.RemoveComponent<Physics>(entity);
		}
*/