#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <memory>
#include <utility>
#include <algorithm>
#include <Windows.h>
#include <sstream>

#define GRAVITY 9.81f

using EntityId = std::uint32_t;
using ComponentId = uint16_t;
using Type = std::vector<ComponentId>;  // always: sorted + unique
/*
{ Position, Velocity }
{ Velocity, Position }
they should be the same
*/

using ArchetypeId = uint16_t;  // 65536 different archetypes should be sufficient for the start

inline ComponentId NextComponentTypeId()
{
    static ComponentId nextId = 0;
    return nextId++;
}

template<typename T>
ComponentId GetComponentTypeId()
{
    static const ComponentId id = NextComponentTypeId();
    return id;
}

struct IColumn;  // forward declaration
struct Archetype
{
    ArchetypeId id = 0;
    Type type;

    std::vector<EntityId> entities;  // all entities of this archetype

    std::unordered_map<ComponentId, size_t> columnIndex;
    std::vector<std::unique_ptr<IColumn>> columns;  // data container for Components to every entity

    std::unordered_map<ComponentId, ArchetypeId> addEdges;
    std::unordered_map<ComponentId, ArchetypeId> removeEdges;

    Archetype() = default;  // default constructor

    // An Archetype owns its columns -> copying makes no sense.
    Archetype(const Archetype&) = delete;  // no copy-constructor
    Archetype& operator=(const Archetype&) = delete;  // no copy via =-operator

    // Moving is exactly what std::vector needs (as unique_ptr cannot be copied, only moved)
    Archetype(Archetype&&) noexcept = default;
    Archetype& operator=(Archetype&&) noexcept = default;
};

struct EntityRecord
{
    bool alive = false;
    ArchetypeId archetype = 0;
    size_t row = 0;  // index in archetype-entity-list
};

struct ArchetypeEdge
{
    ArchetypeId add;
    ArchetypeId remove;
};

// Data Container for Components
struct IColumn
{
    virtual ~IColumn() = default;

    virtual void RemoveSwap(size_t row) = 0;
    virtual void MoveElementTo(size_t row, IColumn& destination) = 0;
    virtual std::unique_ptr<IColumn> CreateEmpty() const = 0;
};
template<typename T>
struct Column : IColumn
{
    std::vector<T> data;

    /**
     * Remove the given row from the
     * data vector by swapping it 
     * to the back and popping.
     *
     * @param row Row that should be removed
     */
    void RemoveSwap(size_t row) override
    {
        const size_t lastRow = data.size() - 1;

        // only move, when not in last place
        if (row != lastRow)
            data[row] = std::move(data[lastRow]);

        data.pop_back();
    }

    /**
     * Move the element at the given position
     * to a new archetype destination.
     *
     * @param row Row that should be moved
     * @param destination IColumn-destination from the other archetype
     */
    void MoveElementTo(size_t row, IColumn& destination) override
    {
        // move element (row as index) to another Archetype-destination
        auto& typedDestination = static_cast<Column<T>&>(destination);
        typedDestination.data.push_back(std::move(data[row]));
    }

    /**
     * Create an empty IColumn for
     * an archetype as default container.
     *
     * @return Unique Pointer to the empty Column for Component Data.
     */
    std::unique_ptr<IColumn> CreateEmpty() const override
    {
        return std::make_unique<Column<T>>();
    }
};

// Hash for a Vector of Components
struct TypeHash
{
    size_t operator()(const Type& type) const
    {
        size_t hash = 0;
        for (ComponentId id : type)
        {
            hash ^= std::hash<ComponentId>{}(id)
                +0x9e3779b9
                + (hash << 6)
                + (hash >> 2);
        }
        return hash;
    }
};

// Meshes
using MeshHandle = uint32_t;
using MaterialHandle = uint32_t;

constexpr MeshHandle INVALID_MESH = 0;
constexpr MaterialHandle INVALID_MATERIAL = 0;

// CONSTANTS
constexpr ArchetypeId EMPTY_ARCHETYPE_ID = 0;

// GLOBAL FUNCTIONS

inline bool CheckHR(HRESULT hr, const char* message)
{
    if (FAILED(hr))
    {
        std::ostringstream oss;
        oss << message << "\nHRESULT: 0x" << std::hex << hr;

        MessageBoxA(nullptr, oss.str().c_str(), "DirectX Error", MB_OK | MB_ICONERROR);
        return false;
    }

    return true;
}