#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>

class ECSController
{
	struct Column {
		void* elements;      // buffer with component data
		size_t element_size; // size of a single element
		size_t count;        // number of elements
	};
	struct ArchetypeRecord {
		size_t column;
	};

	using EntityId = uint32_t;
	using ComponentId = uint16_t;
	using Type = std::vector<ComponentId>;  // list of component ids
	using ArchetypeId = uint16_t;  // 65536 different archetypes should be sufficient for the start
	using ArchetypeMap = std::unordered_map<ArchetypeId, ArchetypeRecord>;

	struct Archetype;  // forward declaration
	struct ArchetypeEdge {
		Archetype& add;
		Archetype& remove;
	};
	struct Archetype {  // Type used to store each unique component list only once
		ArchetypeId id; // unique integer identifier for an archetype
		Type type;
		std::vector<Column> components; // one vector for each component
		std::unordered_map<ComponentId, ArchetypeEdge> edges;
	};
	struct Record {
		Archetype& archetype;
		size_t row;
	};

	// Find an archetype by its list of component ids
	std::unordered_map<Type, Archetype> archetype_index;

	// Find the archetype for an entity
	std::unordered_map<EntityId, Record> entity_index;
	// Find the archetypes for a component
	std::unordered_map<ComponentId, ArchetypeMap> component_index;


	public:

		/*bool has_component(EntityId entity, ComponentId component) {
			Archetype& archetype = entity_index[entity];
			ArchetypeSet& archetype_set = component_index[component];
			return archetype_set.count(archetype.id) != 0;
		}*/

		void* get_component(EntityId entity, ComponentId component) {
			Record& record = entity_index[entity];
			Archetype& archetype = r.archetype;

			// has archetype components?
			ArchetypeMap archetypes = component_index[component];
			if (archetypes.count(archetype.id) == 0) {
				return nullptr;
			}
			ArchetypeRecord& a_record = archetypes[archetype.id];
			return archetype.columns[a_record.column][record.row];
		}

		void add_component(EntityId entity, ComponentId component) {
			Record& record = entity_index[entity];
			Archetype& archetype = record.archetype;
			Archetype& next_archetype = archetype.add_archetypes[component];
			move_entity(archetype, record.row, next_archetype);
		}


		

		ECSController(const StateType type)
		{
			this->type = type;
		}
};
