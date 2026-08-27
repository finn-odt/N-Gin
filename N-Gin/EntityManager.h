#pragma once

#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <tuple>

#include "Usings.h"
#include "StackSet.h"

class EntityManager
{

	// ENTITY MANAGEMENT
	EntityId nextID = 0;
	std::vector<EntityRecord> entityRecords;
	StackSet<EntityId> freedIDs;

	std::vector<Archetype> archetypes;
	std::unordered_map<Type, ArchetypeId, TypeHash> archetypeIndex;  // with custom Hash for vector<ComponentId>

	// COMPONENT MANAGEMENT
	// [Map] EntityId -> vector<ComponentId>

	// [Map] vector<ComponentId> -> Archetype
	// [Map] EntityId -> Archetype

	void RemoveRowFromArchetype(Archetype& archetype, size_t row);


	/**
	 * Either returns or creates a new archetype
	 * based on the given source-archetype to
	 * which the given Component is added.
	 *
	 * @param sourceArchetypeId ID of the archetype on which the Component is added.
	 * @param addedComponent ID of Component that is to be added.
	 * @return Unique Archetype ID of the newly created or already existing archetype.
	 */
	template<typename T>
	ArchetypeId GetOrCreateArchetypeWithAddedComponent(ArchetypeId sourceArchetypeId, ComponentId addedComponent)
	{
		Archetype& source = archetypes[sourceArchetypeId];

		Type newType = source.type;

		auto insertIt = std::lower_bound(newType.begin(), newType.end(), addedComponent);
		if (insertIt == newType.end() || *insertIt != addedComponent)
			newType.insert(insertIt, addedComponent);

		auto existingIt = archetypeIndex.find(newType);
		if (existingIt != archetypeIndex.end())
			return existingIt->second;

		Archetype newArchetype;
		newArchetype.id = static_cast<ArchetypeId>(archetypes.size());
		newArchetype.type = newType;

		for (ComponentId id : newType)
		{
			newArchetype.columnIndex[id] = newArchetype.columns.size();

			if (id == addedComponent)
			{
				newArchetype.columns.push_back(std::make_unique<Column<T>>());
			}
			else
			{
				const size_t sourceColumnIndex = source.columnIndex[id];
				newArchetype.columns.push_back(
					source.columns[sourceColumnIndex]->CreateEmpty()
				);
			}
		}

		const ArchetypeId newId = newArchetype.id;

		archetypes.push_back(std::move(newArchetype));
		archetypeIndex.emplace(newType, newId);

		return newId;
	}

	/**
	 * Either returns or creates a new archetype
	 * based on the given source-archetype from
	 * which the given Component is removed.
	 *
	 * @param sourceArchetypeId ID of the archetype from which the Component is removed.
	 * @param addedComponent ID of Component that is to be removed.
	 * @return Unique Archetype ID of the newly created or already existing archetype.
	 */
	ArchetypeId GetOrCreateArchetypeWithRemovedComponent(ArchetypeId sourceArchetypeId, ComponentId removedComponent)
	{
		Archetype& source = archetypes[sourceArchetypeId];

		Type newType = source.type;

		auto removeIt = std::lower_bound(newType.begin(), newType.end(), removedComponent);
		if (removeIt != newType.end() && *removeIt == removedComponent)
			newType.erase(removeIt);

		auto existingIt = archetypeIndex.find(newType);
		if (existingIt != archetypeIndex.end())
			return existingIt->second;

		Archetype newArchetype;
		newArchetype.id = static_cast<ArchetypeId>(archetypes.size());
		newArchetype.type = newType;

		for (ComponentId id : newType)
		{
			newArchetype.columnIndex[id] = newArchetype.columns.size();

			const size_t sourceColumnIndex = source.columnIndex[id];

			newArchetype.columns.push_back(
				source.columns[sourceColumnIndex]->CreateEmpty()
			);
		}

		const ArchetypeId newId = newArchetype.id;

		archetypes.push_back(std::move(newArchetype));
		archetypeIndex.emplace(newType, newId);

		return newId;
	}


public:
	EntityManager();

	~EntityManager() = default;

	EntityId AddEntity(const std::string& name = "Game Object");

	bool RemoveEntity(EntityId id);

	bool IsAlive(EntityId id) const;

	/**
	 * Get the name of an entity from
	 * its EntityRecord.
	 * 
	 * @param entity ID of the Entity's name
	 * @return Name as a string
	 */
	const std::string& GetEntityName(EntityId entity) const
	{
		return entityRecords[entity].name;
	}

	/**
	 * Set the name of an entity in
	 * its EntityRecord.
	 * Only if the entity is alive.
	 *
	 * @param entity ID of the Entity to be changed
	 * @param name Name that is to be set
	 */
	void SetEntityName(EntityId entity, const std::string& name)
	{
		if (!IsAlive(entity))
			return;

		entityRecords[entity].name = name;
	}

	/**
	 * Adds a given Component to a given Entity,
	 * by moving the Entity to a new archetype.
	 *
	 * @param entity ID of Entity that is modified.
	 * @param component Component that is to be added.
	 * @return Whether the attachment of the component was successful.
	 */
	template<typename T>
	bool AddComponent(EntityId entity, T component)
	{
		//entity moves from old archetype to new archetype with T added
		/*
			--- EXAMPLE ---
			Old archetype: [Transform]
			Add Physics
			New archetype: [Transform, Physics]
			Move entity row from old archetype to new archetype
		*/

		if (!IsAlive(entity))
			return false;

		const ComponentId componentId = GetComponentTypeId<T>();

		EntityRecord& record = entityRecords[entity];
		const ArchetypeId sourceArchetypeId = record.archetype;

		Archetype& sourceArchetype = archetypes[sourceArchetypeId];

		if (sourceArchetype.columnIndex.find(componentId) != sourceArchetype.columnIndex.end())
			return false; // entity already has this component

		ArchetypeId destinationArchetypeId;

		auto edgeIt = sourceArchetype.addEdges.find(componentId);
		if (edgeIt != sourceArchetype.addEdges.end())
		{
			destinationArchetypeId = edgeIt->second;
		}
		else
		{
			destinationArchetypeId = GetOrCreateArchetypeWithAddedComponent<T>(sourceArchetypeId, componentId);

			archetypes[sourceArchetypeId].addEdges[componentId] = destinationArchetypeId;
			archetypes[destinationArchetypeId].removeEdges[componentId] = sourceArchetypeId;
		}

		// Re-fetch references because archetypes vector may have reallocated.
		EntityRecord& currentRecord = entityRecords[entity];
		Archetype& source = archetypes[sourceArchetypeId];
		Archetype& destination = archetypes[destinationArchetypeId];

		const size_t sourceRow = currentRecord.row;
		const size_t destinationRow = destination.entities.size();

		destination.entities.push_back(entity);

		for (ComponentId id : destination.type)
		{
			const size_t destinationColumnIndex = destination.columnIndex[id];
			IColumn& destinationColumn = *destination.columns[destinationColumnIndex];

			if (id == componentId)
			{
				auto& typedColumn = static_cast<Column<T>&>(destinationColumn);
				typedColumn.data.push_back(std::move(component));
			}
			else
			{
				const size_t sourceColumnIndex = source.columnIndex[id];
				IColumn& sourceColumn = *source.columns[sourceColumnIndex];

				sourceColumn.MoveElementTo(sourceRow, destinationColumn);
			}
		}

		// remove entity from old archetype
		RemoveRowFromArchetype(source, sourceRow);

		currentRecord.archetype = destinationArchetypeId;
		currentRecord.row = destinationRow;
		currentRecord.alive = true;

		return true;
	}

	/**
	 * Removes a given Component from a given Entity,
	 * by moving the Entity to a new archetype.
	 *
	 * @param entity ID of Entity that is modified.
	 * @param component Component that is to be removed.
	 * @return Whether the detachment of the component was successful.
	 */
	template<typename T>
	bool RemoveComponent(EntityId entity, T component)
	{
		//entity moves from old archetype to new archetype with T removed
		/*
			--- EXAMPLE ---
			Old archetype: [Transform, Physics]
			Remove Physics
			New archetype: [Transform]
			Move only Transform data
			Discard Physics data
		*/

		if (!IsAlive(entity))
			return false;

		const ComponentId componentId = GetComponentTypeId<T>();

		EntityRecord& record = entityRecords[entity];
		const ArchetypeId sourceArchetypeId = record.archetype;

		Archetype& sourceArchetype = archetypes[sourceArchetypeId];

		if (sourceArchetype.columnIndex.find(componentId) == sourceArchetype.columnIndex.end())
			return false; // entity does not have this component

		ArchetypeId destinationArchetypeId;

		auto edgeIt = sourceArchetype.removeEdges.find(componentId);
		if (edgeIt != sourceArchetype.removeEdges.end())
		{
			destinationArchetypeId = edgeIt->second;
		}
		else
		{
			destinationArchetypeId =
				GetOrCreateArchetypeWithRemovedComponent(sourceArchetypeId, componentId);

			archetypes[sourceArchetypeId].removeEdges[componentId] = destinationArchetypeId;
			archetypes[destinationArchetypeId].addEdges[componentId] = sourceArchetypeId;
		}

		EntityRecord& currentRecord = entityRecords[entity];
		Archetype& source = archetypes[sourceArchetypeId];
		Archetype& destination = archetypes[destinationArchetypeId];

		const size_t sourceRow = currentRecord.row;
		const size_t destinationRow = destination.entities.size();

		destination.entities.push_back(entity);

		for (ComponentId id : destination.type)
		{
			const size_t sourceColumnIndex = source.columnIndex[id];
			const size_t destinationColumnIndex = destination.columnIndex[id];

			IColumn& sourceColumn = *source.columns[sourceColumnIndex];
			IColumn& destinationColumn = *destination.columns[destinationColumnIndex];

			sourceColumn.MoveElementTo(sourceRow, destinationColumn);
		}

		RemoveRowFromArchetype(source, sourceRow);

		currentRecord.archetype = destinationArchetypeId;
		currentRecord.row = destinationRow;
		currentRecord.alive = true;

		return true;
	}

	/**
	 * Get a component of type T from
	 * the given Entity.
	 *
	 * @param entity ID of Entity to be checked.
	 * @return The component of the given Entity (or Nullptr)
	 */
	template<typename T>
	T* GetComponent(EntityId entity)
	{
		//find entity record -> find archetype -> find column -> return &column's data[row] (component of that entity)

		if (!IsAlive(entity))
			return nullptr;

		const ComponentId componentId = GetComponentTypeId<T>();

		EntityRecord& record = entityRecords[entity];
		Archetype& archetype = archetypes[record.archetype];

		auto columnIt = archetype.columnIndex.find(componentId);
		if (columnIt == archetype.columnIndex.end())
			return nullptr;

		const size_t columnIndex = columnIt->second;

		auto* column = static_cast<Column<T>*>(archetype.columns[columnIndex].get());

		return &column->data[record.row];
	}

	/**
	 * Check if a Component of type T exists
	 * on the given Entity.
	 *
	 * @param entity ID of Entity to be checked.
	 * @return Whether the Entity has that component or not.
	 */
	template<typename T>
	bool HasComponent(EntityId entity) const
	{
		// checks whether the entity's current archetype has a column for that component type (T)

		if (!IsAlive(entity))
			return false;

		const ComponentId componentId = GetComponentTypeId<T>();

		const EntityRecord& record = entityRecords[entity];
		const Archetype& archetype = archetypes[record.archetype];

		return archetype.columnIndex.find(componentId) != archetype.columnIndex.end();
	}

	/**
	 * Executes a given function on all
	 * Entities that have the required
	 * Component-structure.
	 *
	 * @param func Function that is executed for all fitting entities.
	 */
	template<typename... Components, typename Func>
	void ForEach(Func&& func)
	{
		for (Archetype& archetype : archetypes)
		{
			// Archetype must contain every requested component.
			const bool matches =
				(archetype.columnIndex.contains(
					GetComponentTypeId<Components>()
				) && ...);

			if (!matches)
				continue;

			// Resolve component columns once per archetype.
			auto columns = std::tuple<Column<Components>*...>
			{
				static_cast<Column<Components>*>(
					archetype.columns[
						archetype.columnIndex.at(
							GetComponentTypeId<Components>()
						)
					].get()
				)...
			};

			for (size_t row = 0;
				row < archetype.entities.size();
				++row)
			{
				std::apply(
					[&](auto*... column)
					{
						func(
							archetype.entities[row],
							column->data[row]...
						);
					},
					columns
				);
			}
		}
	}
};

// Generic Functions could also get there own file:
// #include "EntityManager.inl"
