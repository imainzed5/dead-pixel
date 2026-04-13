#pragma once

#include "ecs/ComponentArray.h"
#include "ecs/EntityManager.h"

#include <memory>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>
#include <vector>

class World
{
public:
    World() = default;

    Entity createEntity();
    void destroyEntity(Entity entity);

    [[nodiscard]] const std::vector<Entity>& livingEntities() const;

    template <typename T>
    void registerComponent()
    {
        const std::type_index type = std::type_index(typeid(T));
        if (mComponentArrays.contains(type))
        {
            return;
        }

        mComponentArrays[type] = std::make_shared<ComponentArray<T>>();
    }

    template <typename T>
    void addComponent(Entity entity, const T& component)
    {
        getComponentArray<T>()->insertData(entity, component);
    }

    template <typename T>
    void removeComponent(Entity entity)
    {
        getComponentArray<T>()->removeData(entity);
    }

    template <typename T>
    T& getComponent(Entity entity)
    {
        return getComponentArray<T>()->getData(entity);
    }

    template <typename T>
    const T& getComponent(Entity entity) const
    {
        return getComponentArray<T>()->getData(entity);
    }

    template <typename T>
    [[nodiscard]] bool hasComponent(Entity entity) const
    {
        return getComponentArray<T>()->hasData(entity);
    }

    template <typename... Components, typename Func>
    void forEach(Func&& function)
    {
        for (const Entity entity : mEntityManager.livingEntities())
        {
            if ((hasComponent<Components>(entity) && ...))
            {
                function(entity, getComponent<Components>(entity)...);
            }
        }
    }

private:
    template <typename T>
    std::shared_ptr<ComponentArray<T>> getComponentArray()
    {
        const std::type_index type = std::type_index(typeid(T));
        auto it = mComponentArrays.find(type);
        if (it == mComponentArrays.end())
        {
            throw std::runtime_error("Component type has not been registered.");
        }

        return std::static_pointer_cast<ComponentArray<T>>(it->second);
    }

    template <typename T>
    std::shared_ptr<const ComponentArray<T>> getComponentArray() const
    {
        const std::type_index type = std::type_index(typeid(T));
        auto it = mComponentArrays.find(type);
        if (it == mComponentArrays.end())
        {
            throw std::runtime_error("Component type has not been registered.");
        }

        return std::static_pointer_cast<const ComponentArray<T>>(it->second);
    }

    EntityManager mEntityManager;
    std::unordered_map<std::type_index, std::shared_ptr<IComponentArray>> mComponentArrays;
};
