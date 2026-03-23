// #ifndef RESOURCE_MANAGER
// #define RESOURCE_MANAGER
// #include <memory>
// #include <string>
// #include <typeindex>
// #include <unordered_map>

// #include "Resource.h"
// #include "ResourceHandle.h"
// #include <vma/vk_mem_alloc.h>
// #include <vulkan/vulkan.h>

// namespace Expectre {
// class ResourceManager {
// public:
//   ResourceManager(VkDevice device, VmaAllocator allocator,
//                   VkCommandPool cmd_pool, VkQueue graphics_queue)
//       : m_device(device), m_allocator(allocator), m_cmd_pool(cmd_pool),
//         m_graphics_queue(graphics_queue) {}

//   template <typename T> ResourceHandle<T> Load(const std::string &resource_id) {
//     static_assert(std::is_base_of<Resource, T>::value,
//                   "T must derive from Resource");

//     auto &type_resources = resources[std::type_index(typeid(T))];
//     auto it = type_resources.find(resource_id);
//     if (it != type_resources.end()) {
//       // Resource exists in cache - increment reference count and return handle
//       ref_counts[resource_id]++;
//       return ResourceHandle<T>(resource_id, this);
//     }

//     // Create new resource and attempt loading
//     auto resource = std::make_shared<T>(resource_id);
//     if (!resource->Load()) {
//       // loading failed, return invalid/null handle
//       return ResoruceHandle<T>();
//     }

//     // Resource successfully created
//     type_resources[resource_id] = resource;
//     ref_counts[resource_id] = 1;

//     return ResourceHandle<T>(resource_id, this);
//   }

//   template <typename T> T *GetResource(const std::string &resource_id) {
//     // Access type-specific resource
//     auto &type_resources = resources[std::type_index(typeid(T))];
//     auto it = type_resources.find(id);

//     if (it != type_resources.end()) {
//       return static_cast<T *>(it->second.get());
//     }

//     // Resource not found - return null for safe handling
//     return nullptr;
//   }

//   template <typename T> bool HasResource(const std::string &resource_id) {
//     auto resource_it = resources.find(std::type_index(typeid(T)));
//     return resource_it != resources.end();
//   }

//   template <typename T> void Release(const std::string &resource_id) {
//     // 1. Ensure the type is actually a Resource at compile-time
//     static_assert(std::is_base_of<Resource, T>::value,
//                   "T must derive from Resource");

//     // 2. Access the specific "bucket" for this type
//     auto &type_resources = refCounts[std::type_index(typeid(T));];

//     // 3. Look for the resource ID in that specific bucket
//     auto it = type_resources.find(resourceId);

//     if (it != type_resources.end()) {
//       // 4. Decrement the reference count
//       it->second.refCount--;

//       // 5. If no one is using it anymore, clean it up
//       if (it->second.refCount <= 0) {
//         // Call the virtual Unload() to clean up GPU memory (Vulkan
//         // images/buffers)
//         it->second.resource->Unload();

//         // Remove the resource from the storage map
//         resources[type_index].erase(resourceId);

//         // Remove the tracking data from the ref-count map
//         type_resources.erase(it);

//         spdlog::Debug("Successfully unloaded and cleared: {}", resource_id);
//         std::cout << "Successfully unloaded and cleared: " << resourceId
//                   << std::endl;
//       }
//     } else {
//       spdlog::Warn("Warning: Attempted to release non-existent resource: {}",
//                    resource_id);
//     }
//   }

//   void UnloadAll() {
//     // Emergency cleanup method for system shutdown or major state changes
//     for (auto &[type, type_resources] : resources) {
//       for (auto &[id, resource] : type_resources) {
//         resource->unload_from_GPU(); // Ensure all resources clean up properly
//       }
//       type_resources.clear(); // Clear type-specific containers
//     }
//     ref_counts.clear(); // Reset all reference counts
//   }

// protected:
//   VkDevice m_device;
//   VmaAllocator m_allocator;
//   VkCommandPool m_cmd_pool;
//   VkQueue m_graphics_queue;

// private:
//   // Organized by type first, then by unique identifier
//   std::unordered_map<std::type_index,
//                      std::unordered_map<std::string, std::shared_ptr<Resource>>>
//       resources;

//   struct ResourceData {
//     std::shared_ptr<Resource> resource; //
//     int ref_count;
//   };

//   std::unordered_map<std::type_index,
//                      std::unordered_map<std::string, ResourceData>>
//       ref_counts;
// };
// } // namespace Expectre
// #endif // RESOURCE_MANAGER