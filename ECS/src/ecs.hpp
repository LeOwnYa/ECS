#ifndef ECS_HPP
#define ECS_HPP


#include <iostream>
#include <vector>
#include <array>
#include <bitset>
#include <map>
#include <cassert>
#include <algorithm>
#include <stdlib.h>
#include <list>
#include <typeindex>
#include <thread>
#include <chrono>
#include <cmath>
#include <functional>

namespace tlk
{
#define PI	3.1415926535897932384626433

	namespace utils
	{
		template<typename T, typename C>
		inline int binarySearch(const std::vector<std::shared_ptr<T>>& mv, const C& mValue)
		{
			unsigned int first = 0;
			unsigned int last = mv.size() - 1;
			unsigned int middle = 0;
			int index = -1;
			unsigned int bSearchLimit = 5;

			while (index == -1 && last >= first)
			{
				middle = (last + first) / 2;

				if (*mv[middle] == mValue)
				{
					// Value Found
					index = middle;
				}
				else if (*mv[middle] > mValue)
				{
					// Value is less. Not found
					// Move last
					last = middle - 1;
				}
				else if (*mv[middle] < mValue)
				{
					// Value is greater. Not found
					// Move first
					first = middle + 1;
				}

				if ((last - first) <= bSearchLimit)
				{
					// Start linear search instead
					for (unsigned int i = first; i <= last && index == -1; i++)
					{
						if (*mv[i] == mValue)
						{
							index = i;
						}
					}
				}
			}

			return index;
		}

		template<size_t N>
		inline bool bitsetFits(const std::bitset<N>& key, const std::bitset<N>& keyHole)
		{
			if (key.count() > keyHole.count())
			{
				return false;
			}

			bool fits = true;

			for (unsigned int i = 0; i < N && fits; i++)
			{
				if (key[i] && !keyHole[i])
				{
					fits = false;
				}
			}

			return fits;
		}
	}

	namespace ecs
	{
#pragma region Pre-definitions

		struct BaseComponent;
		class World;
		class BaseSystem;

#pragma endregion


#pragma region Settings

		using EntityID = size_t;
		using ComponentID = size_t;
		using ComponentArrayIndex = size_t;

		namespace Internal
		{
			inline ComponentID getComponentUniqueID()
			{
				static ComponentID lastCID{ 0u };
				return lastCID++;
			}
		}

		template <typename ComponentType>
		inline ComponentID getComponentTypeID() noexcept
		{
			static_assert(std::is_base_of<BaseComponent, ComponentType>::value,
				"ComponentType must inherit from BaseComponent");

			static ComponentID typeCID{ Internal::getComponentUniqueID() };
			return typeCID;
		}

		// Let's define a maximum number of components:
		constexpr std::size_t maxComponents{ 32 };

		// Let's define a maximum number of entities that
		// can have the same component type:
		constexpr std::size_t maxEntitiesPerComponent{ 256 };

		// We also need to define a max number of entities in our game
		// As we store all entities in a vector means that max size can 
		// change during runtime, but we want to initially reserve space
		// for 512 entities.
		constexpr std::size_t maxEntities{ 512 };

		// Defining the maximum nr of systems
		constexpr std::size_t maxSystems{ 32 };

		// Let's typedef an `std::bitset` for our components:
		using ComponentBitset = std::bitset<maxComponents>;

		// And let's also typedef an `std::array` for them:
		using ComponentArray = std::array<BaseComponent*, maxComponents>;

#pragma endregion


#pragma region Entity

		struct Entity
		{
			static EntityID lastId;
			EntityID id;
			ComponentBitset cBitset;
			bool destroyOnStateChange;

			Entity() : destroyOnStateChange(true) { id = lastId++; }
			Entity(const Entity& e)
			{
				std::cout << "Copy" << std::endl;
				id = e.id;
				cBitset = e.cBitset;
				destroyOnStateChange = e.destroyOnStateChange;
			}
			virtual ~Entity() { }

			Entity& operator=(const Entity& e)
			{
				if (this != &e)
				{
					std::cout << "Copy" << std::endl;
					id = e.id;
					cBitset = e.cBitset;
					destroyOnStateChange = e.destroyOnStateChange;
				}

				return *this;
			}
			bool operator==(const EntityID& mid) const { return id == mid; }
			bool operator>(const EntityID& mid) const { return id > mid; }
			bool operator<(const EntityID& mid) const { return id < mid; }
			bool operator<=(const EntityID& mid) const { return id <= mid; }
			bool operator>=(const EntityID& mid) const { return id >= mid; }

			friend bool operator<(const Entity& lhs, const Entity& rhs) { return lhs.id < rhs.id; }
			friend bool operator>(const Entity& lhs, const Entity& rhs) { return lhs.id > rhs.id; }
			friend bool operator==(const Entity& lhs, const Entity& rhs) { return lhs.id == rhs.id; }
			friend bool operator!=(const Entity& lhs, const Entity& rhs) { return lhs.id != rhs.id; }
			friend bool operator>=(const Entity& lhs, const Entity& rhs) { return lhs.id >= rhs.id; }
			friend bool operator<=(const Entity& lhs, const Entity& rhs) { return lhs.id <= rhs.id; }


			friend std::ostream& operator<<(std::ostream& out, const Entity& e)
			{
				out << "Entity(#" << e.id << ") has the following bitset:\n" << e.cBitset;
				return out;
			}
		};

		class EntityManager
		{
		public:
			std::vector<std::shared_ptr<Entity>> entities;

		public:
			EntityManager(unsigned int size = maxEntitiesPerComponent) { entities.reserve(size); }
			virtual ~EntityManager() { }

			inline unsigned int getTotalEntities() const { return entities.size(); }

			inline std::shared_ptr<Entity>& getEntityById(const EntityID& mEntityId)
			{
				int index = utils::binarySearch(entities, mEntityId);

				if (index == -1)
				{
					throw("Invalid entity ID.");
				}

				return entities[index];
			}

			inline std::shared_ptr<Entity>& createEntity()
			{
				Entity* e(new Entity());
				std::shared_ptr<Entity> uPtr{ e };
				entities.emplace_back(std::move(uPtr));

				return entities[entities.size() - 1];
			}

			inline void removeEntity(std::shared_ptr<Entity>& mEntity)
			{
				int index = utils::binarySearch(entities, mEntity->id);

				if (index >= 0)
				{
					entities.erase(entities.begin() + index);
				}
			}

			inline void removeComponentById(const Entity& mEntity, ComponentID mCId)
			{
				int index = utils::binarySearch(entities, mEntity.id);

				if (index == -1)
				{
					// If this happens, we are trying to remove an entity which doesn't exists
					throw("Invalid entity ID.");
				}

				entities[index]->cBitset.set(mCId, false);
			}

			friend std::ostream& operator<<(std::ostream& out, const EntityManager& em)
			{
				for (unsigned int i = 0; i < em.entities.size(); i++)
				{
					out << *em.entities[i] << std::endl;
				}
				return out;
			}
		};

#pragma endregion


#pragma region Component

		struct BaseComponent
		{
			std::shared_ptr<Entity> entity;
		};

		struct BaseComponentManager
		{
			World* parentWorld;
			ComponentID cTypeId;

			std::map<std::shared_ptr<Entity>, ComponentArrayIndex> entityMap;
			ComponentArrayIndex total{ 1 };

			BaseComponentManager(World* mWorld) : parentWorld(mWorld) { }
			virtual ~BaseComponentManager() { }

			virtual void removeComponent(std::shared_ptr<Entity>& mEntity) = 0;

			virtual std::string toString() const = 0;
		};

		template<typename ComponentType>
		class ComponentManager : public BaseComponentManager
		{
		private:
			// Our containers
			// In map we have <EntityID/Entity, indexInArray>
			// In array is our components in each index
			// Everytime a component is removed from the array
			// We need to fill the empty gaps and update our map
			//std::map<Entity, ComponentArrayIndex> entityMap;
			std::array<ComponentType, maxEntitiesPerComponent> compArr;

		public:
			ComponentManager(World* mWorld) : BaseComponentManager(mWorld) { }
			virtual ~ComponentManager() { }

			inline ComponentType& getComponent(const std::shared_ptr<Entity>& mEntity)
			{
				// Due to our map we can get the value that corresponds
				// to what index in our array the component for this entity is located.
				ComponentArrayIndex tempIndex = entityMap.at(mEntity);

				return compArr[tempIndex];
			}

			template<typename... TArgs>
			inline ComponentType& addComponent(std::shared_ptr<Entity>& mEntity, TArgs&&... mArgs)
			{
				// We need to decide how to handle if there is no more
				// space in manager for another entity. For now we assert
				assert(total < (maxEntitiesPerComponent + 1));

				// Now we place the entity in our map
				// Then we know to which index this specific entity
				// has its component
				entityMap.emplace(mEntity, total);

				// Now we will add the component to the same index
				// as the entities value from the map. In this case
				// it will always be the value of total.
				compArr.at(total) = ComponentType(std::forward<TArgs>(mArgs)...);

				// We need to change the entity bitset so we now know that
				// this entity has this component as well
				mEntity->cBitset.flip(getComponentTypeID<ComponentType>());

				// After creating the component and adding it to our array
				// we need to attach it to an entity. We do this after update
				// the entity bitset
				compArr.at(total).entity = mEntity;

				// Returning the newly added component and increase total by 1
				return compArr[total++];
			}

			inline void removeComponent(std::shared_ptr<Entity>& mEntity)
			{
				/*
				/// /////////////////////////////////////////////// ///
				///				Order of business					///
				/// 1:		Find array index of entity				///
				/// 2:		Check if entity exists in this system	///
				///	2.1:	Swap index with last index				///
				/// 2.2:	Set last index to nullptr				///
				/// 2.3:	Decrease total and check if total > 1	///
				/// 2.3.1:	Update the entity map (ArrayIndex)		///
				/// 2.4:	Remove entity from map					///
				/// /////////////////////////////////////////////// ///

				// Step 1
				ComponentArrayIndex tempIndex = entityMap[mEntity];

				// Step 2
				if (tempIndex > 0)
				{

					// Step 2.1
					cTransform[tempIndex] = cTransform[total - 1];
					cSprite[tempIndex] = cSprite[total - 1];

					// Step 2.2
					cTransform[total - 1] = nullptr;
					cSprite[total - 1] = nullptr;

					// Step 2.3
					total--;
					if (total > tempIndex)
					{
						// Step 2.3.1
						entityMap[cTransform[tempIndex]->entity] = tempIndex;
					}

					// Step 2.4
					entityMap.erase(mEntity);
				}*/
				/// ///////////////////////////////////////////////////////////////////////	///
				///									Order of business						///
				/// 1. Retrieve ComponentArrayIndex from entityMap							///
				/// 2. Remove ownership to entity from affected component					///
				/// 3. Replace affected component with component at last index				///
				/// 4. Reset all attributes and entity of compnent at last index			///
				/// 5. Decrease total by one and update the entityMap if total > tempIndex	///
				/// 6. Remove the entity it self from the map								///
				/// ///////////////////////////////////////////////////////////////////////	///

				// Step 1
				ComponentArrayIndex tempIndex = entityMap.at(mEntity);

				// Step 2
				compArr[tempIndex].entity.reset();

				// Step 3
				compArr[tempIndex] = compArr[total - 1];

				// Step 4
				compArr[total - 1] = ComponentType();

				// Step 5
				total--;
				if (total > tempIndex)
				{
					entityMap[compArr[tempIndex].entity] = tempIndex;
				}

				// Step 6
				entityMap.erase(mEntity);
				mEntity->cBitset.flip(getComponentTypeID<ComponentType>());

			}

			std::string toString() const
			{
				std::string info = "";

				info = "Printing entities by ID that have component with ID #:" + std::to_string(cTypeId) + "\n";

				for (unsigned int i = 1; i < total; i++)
				{
					info += std::to_string(compArr[i].entity->id) + "\n";
				}

				return info;
			}

			friend std::ostream& operator<<(std::ostream& out, const ComponentManager& manager)
			{
				out << "Printing entities by ID that have component with ID #:" << static_cast<int>(manager.cTypeId) << std::endl;
				for (unsigned int i = 0; i < manager.total; i++)
				{
					out << manager.compArr[i] << ", ";
				}
				return out;
			}

		};

#pragma endregion


#pragma region System

		class BaseSystem
		{
		protected:
			// With a bitset we can easily see what components the system requires
			// and with that we can see which entities fit the criteria
			ComponentBitset signature;
			World* parentWorld{ nullptr };
			std::map<std::shared_ptr<Entity>, ComponentArrayIndex> entityMap;

		public:
			ComponentArrayIndex total{ 1 };

		public:
			BaseSystem(World* mWorld) : parentWorld(mWorld) { }
			virtual ~BaseSystem() { }

			inline bool hasSignaturePart(ComponentID mCId) const { return signature[mCId]; }

			virtual void init() { }
			virtual void input(float mDT) { }
			virtual void update(float mDT) { }
			virtual void draw() { }

			virtual std::string toString() const = 0;

			virtual void addComponent(std::shared_ptr<Entity>& mEntity) = 0;

			virtual void removeEntity(std::shared_ptr<Entity>& mEntity) = 0;

		};

#pragma endregion


#pragma region World

		class World
		{
		public:
			std::array<std::unique_ptr<BaseComponentManager>, maxComponents> cManagers;
			// We might want to have entities in group (if we want an entity to be able to be in multiple groups)
			// or have multiple entity manager (if we want each entity to be in one group only)
			//std::vector<std::unique_ptr<EntityManager>> eManagers;

			// But for now we have one entity manager for all entities
			std::unique_ptr<EntityManager> eManager;

			std::array<std::unique_ptr<BaseSystem>, maxSystems> systems;
			std::size_t totalSystems{ 1 };

			unsigned int frameCount{ 0 };

		public:
			World(unsigned int eManagerSize = maxEntities)
			{
				initiateEntityManager(eManagerSize);
				initiateSystems();
			}
			virtual ~World() { }

			/// Entity Functions ///
			inline std::shared_ptr<Entity>& getEntityById(const EntityID& mEntityId) const
			{
				std::shared_ptr<Entity> temp = std::make_shared<Entity>();
				try
				{
					return eManager.get()->getEntityById(mEntityId);
				}
				catch (const char* exep)
				{
					std::cout << exep << std::endl;;
				}

				return temp;
			}

			inline std::shared_ptr<Entity>& createEntity() { return eManager->createEntity(); }

			inline void printTotals(int mType, int mIndex)
			{
				switch (mType)
				{
				case 0:		// Systems
					std::cout << "Total in systems: " << systems[mIndex]->total << std::endl;
					break;
				case 1:		// Component Managers
					if (cManagers[mIndex] != nullptr)
					{
						std::cout << "Total in component manager: " << cManagers[mIndex]->total << std::endl;
					}
					break;
				default:
					break;
				}
			}

			inline void destroyEntity(std::shared_ptr<Entity>& mEntity)
			{
				/// /////////////////////////////////////////////////////////////////// ///
				///								Order of business						///
				/// 1. Remove all components the entity has from component managers		///
				/// 2. Update systems so everything points to the correct places		///
				/// 3. Remove the entity it self from the world (entity manager)		///
				/// /////////////////////////////////////////////////////////////////// ///

				// Step 1
				for (unsigned int i = 0; i < mEntity->cBitset.size(); i++)
				{
					if (mEntity->cBitset[i])
					{
						cManagers[i]->removeComponent(mEntity);
					}
				}

				// Step 2
				for (size_t i = 1; i < totalSystems; i++)
				{
					systems[i]->removeEntity(mEntity);
				}

				// Step 3
				eManager->removeEntity(mEntity);
			}

			inline int getTotalEntities() const { return eManager->getTotalEntities(); }

			/// Component Functions ///
			template<typename ComponentType>
			inline ComponentType& getComponent(const std::shared_ptr<Entity>& mEntity)
			{
				// First we need to make sure that T inherits from BaseComponent
				// meaning that we only want to create component managers that
				// inherits from BaseComponent
				static_assert(std::is_base_of<BaseComponent, ComponentType>::value,
					"ComponentType must inherit from BaseComponent");

				// Before calling the getComponent function inside its
				// manager, we will make sure that the entity has a component
				// of the type T
				assert(mEntity->cBitset[getComponentTypeID<ComponentType>()]);

				// For us to be able to call the addComponent function inside
				// the correct manager we first retrieve the manager and 
				// then call the function plus return the result.
				return getComponentManager<ComponentType>()->getComponent(mEntity);
			}

			template<typename ComponentType, typename... TArgs>
			inline ComponentType& addComponent(std::shared_ptr<Entity>& mEntity, TArgs&&... mArgs)
			{
				///	/////////////////////////////////////////////////////////////// ///
				///						Order of business							///
				/// 1:		Make sure T inherits from BaseComponent					///
				/// 2:		Make sure that entity doesn't already have T component	///
				/// 3:		Check if a component manager with that type exists		///
				/// 3.1:	If not, then create one									///
				/// 4:		Add component to its manager. Save the component as ref	///
				/// 5:		Ask all systems to see if they need this component		///
				/// 6:		Return a ref to the newly added component				///
				///	/////////////////////////////////////////////////////////////// ///

				// Step 1
				static_assert(std::is_base_of<BaseComponent, ComponentType>::value,
					"ComponentType must inherit from BaseComponent");

				// Step 2
				assert(!mEntity->cBitset[getComponentTypeID<ComponentType>()]);

				// Step 3
				if (cManagers[getComponentTypeID<ComponentType>()] == nullptr)
				{
					// Step 3.1
					ComponentManager<ComponentType>* cm(new ComponentManager<ComponentType>(this));
					cm->cTypeId = getComponentTypeID<ComponentType>();
					std::unique_ptr<ComponentManager<ComponentType>> uPtr{ cm };
					cManagers[cm->cTypeId] = std::move(uPtr);
				}

				// Step 4
				ComponentType& addedComp = getComponentManager<ComponentType>()->addComponent(mEntity, std::forward<TArgs>(mArgs)...);

				// Step 5
				for (size_t i = 1; i < totalSystems; i++)
				{
					systems[i]->addComponent(mEntity);
				}

				// Step 6
				return addedComp;
			}

			/*template<typename ComponentType>
			inline void removeComponent(std::shared_ptr<Entity>& mEntity)
			{
				/// /////////////////////////////////////////////////////////////// ///
				///							Order of business						///
				/// 1.	Check that T inherits from BaseComponent					///
				/// 2.	Make sure the entity has a component of type T				///
				/// 3.	Remove component from its component manager					///
				/// 4.	Update systems so everything points to the correct places	///
				/// 5.	Update entity's bitset										///
				/// /////////////////////////////////////////////////////////////// ///

				// Step 1
				static_assert(std::is_base_of<BaseComponent, ComponentType>::value,
					"ComponentType must inherit from BaseComponent");

				// Step 2
				assert(mEntity->cBitset[getComponentTypeID<ComponentType>()]);

				// Step 3
				cManagers[getComponentTypeID<ComponentType>()].get()->removeComponent(mEntity);

				// Step 4
				for (size_t i = 1; i < totalSystems; i++)
				{
					if (systems[i]->hasSignaturePart(getComponentTypeID<ComponentType>()))
					{
						systems[i]->removeEntity(mEntity);
					}
				}

				// Step 5
				eManager->removeComponentById(*mEntity.get(), getComponentTypeID<ComponentType>());
			}*/

			/// Systems Functions ///
			inline void init()
			{
				// Initiate all systems
				for (size_t i = 1; i < totalSystems; i++)
				{
					systems[i]->init();
				}
			}

			inline void clear()
			{
				for (int i = (int)(eManager->entities.size() - 1); i >= 0; i--)
				{
					if (eManager->entities.at(i)->destroyOnStateChange)
					{
						destroyEntity(eManager->entities.at(i));
					}
				}
			}

			inline void input(float mDT)
			{
				for (size_t i = 1; i < totalSystems; i++)
				{
					systems[i]->input(mDT);
				}
			}

			inline void update(float mDT)
			{
				for (size_t i = 1; i < totalSystems; i++)
				{
					systems[i]->update(mDT);
				}

				// This should always be last line of code in update
				frameCount++;
			}

			inline void draw()
			{
				for (size_t i = 1; i < totalSystems; i++)
				{
					systems[i]->draw();
				}
			}

			/// Other ///
			friend std::ostream& operator<<(std::ostream& out, const World& world)
			{
				out << "------------Printing everything in the world------------\n";
				out << "Entities:\n" << *world.eManager;

				out << "\nComponents:\n";
				for (size_t i = 0; i < maxComponents; i++)
				{
					if (world.cManagers[i].get() != nullptr)
					{
						out << world.cManagers[i]->toString() << std::endl;
					}
				}

				out << "Systems:\n";
				for (size_t i = 1; i < world.totalSystems; i++)
				{
					out << world.systems[i]->toString() << std::endl;
				}

				out << "------------Done print the world------------\n";
				return out;
			}

		private:
			inline void initiateEntityManager(unsigned int mSize)
			{
				EntityManager* em(new EntityManager(mSize));
				std::unique_ptr<EntityManager> uPtr{ em };
				eManager = std::move(uPtr);
			}

			void initiateSystems();

			template<typename ComponentType>
			inline ComponentManager<ComponentType>* getComponentManager()
			{
				return dynamic_cast<ComponentManager<ComponentType>*>(cManagers[getComponentTypeID<ComponentType>()].get());
			}

		};

#pragma endregion


		inline void World::initiateSystems()
		{

		}
	}
}

#endif // !ECS_HPP
