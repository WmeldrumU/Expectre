// #ifndef RESOURCE_HANDLE_H
// #define RESOURCE_HANDLE_H
// #include <string>
// namespace Expectre {
// class ResourceManager;

// template <typename T> class ResourceHandle {
// public:
//   ResourceHandle() : manager(nullptr) {}

//   ResourceHandle(const std::string &id, ResourceManager *manager)
//       : resource_id(id), manager(manager) {}

//   T *Get() const {
//     if (!manager)
//       return nullptr;
//     return manager->getResource<T>(resource_id);
//   }

//   bool IsValid() const {
//     return manager && manager->HasResource<T>(resource_id);
//   }

//   const std::string &get_id() const { return resource_id; }

//   T *operator->() const { return Get(); }

//   T &operator*() const { return *Get(); }

//   operator bool() const { return IsValid(); }

// private:
//   std::string resource_id;
//   ResourceManager *manager;
// };
// } // namespace Expectre
// #endif // RESOURCE_HANDLE_H